/* ============================================================
 * eh_query.c — EH-G3 Interactive Query Binary
 *
 * Reads questions from stdin (one per line), outputs answers.
 * Used for adversarial evaluation on arbitrary question sets.
 *
 * Usage:
 *   echo "what is python" | ./examples/eh_query \
 *       training/trigram_v15_model.ehdag \
 *       training/eh_g3_embeddings.bin \
 *       training/trigram_v15_vocab.txt \
 *       training/trigram_v15_vocab.txt.pairs
 *
 * Output format (one line per question):
 *   Q: <question> | A: <answer>
 *
 * Compilation:
 *   gcc -O3 -std=c99 -Wall -Iinclude -Iinclude/core -Iinclude/hgn \
 *       src/core/*.c src/hgn/*.c examples/eh_query.c \
 *       -o examples/eh_query -lm
 * ============================================================ */

#define _GNU_SOURCE
#include "../include/hgn/eh_hgn_dag.h"
#include "../include/hgn/eh_beam_search.h"
#include "../include/core/eh_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>

/* External beam search tuning parameters */
extern float eh_beam_rep_penalty;
extern float eh_beam_temperature;
extern int   eh_beam_use_question_embedding;

/* ── Query Canonicalization ──────────────────────────────── */
/*
 * Normalize arbitrary question forms → canonical "what is X" form.
 * This fixes paraphrase failures where the concept word isn't last.
 *
 * Rules (applied in order):
 *   "define X"           → "what is X"
 *   "what does X mean"   → "what is X"
 *   "what does X stand for" → "what is X"
 *   "what does a X do"   → "what does a X do"  (keep — server/function)
 *   "tell me about X"    → "what is X"
 *   "explain X"          → "what is X"
 *   "describe X"         → "what is X"
 *   "what is meant by X" → "what is X"
 *   "how would you define X" → "what is X"
 *   "what do we call X"  → "what is X"
 *   "can you name X"     → "what is X"
 *   "who was X"          → "who is X"
 *   "where can you find X" → "where is X"
 */
static void normalize_query(const char *raw, char *out, size_t out_sz) {
    /* Copy and lowercase */
    char buf[512];
    size_t n = strlen(raw);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, raw, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* Strip trailing '?' and whitespace */
    char *end = buf + strlen(buf) - 1;
    while (end > buf && (*end == '?' || *end == ' ' || *end == '.')) *end-- = '\0';

    /* Helper: match prefix, return pointer to rest */
    #define MATCH(prefix) (strncmp(buf, (prefix), strlen(prefix)) == 0 \
                           ? buf + strlen(prefix) : NULL)

    const char *rest;
    char concept[400];

    /* "define X" → "what is X" */
    if ((rest = MATCH("define a ")))        { snprintf(concept, sizeof(concept), "a %s", rest);   goto what_is; }
    if ((rest = MATCH("define an ")))       { snprintf(concept, sizeof(concept), "an %s", rest);  goto what_is; }
    if ((rest = MATCH("define ")))          { snprintf(concept, sizeof(concept), "%s", rest);      goto what_is; }

    /* "what does X mean" → "what is X" */
    if ((rest = MATCH("what does "))) {
        char tmp[400]; snprintf(tmp, sizeof(tmp), "%s", rest);
        char *mean = strstr(tmp, " mean");
        if (mean) { *mean = '\0'; snprintf(concept, sizeof(concept), "%s", tmp); goto what_is; }
        /* "what does X stand for" → "what is X" */
        char *stand = strstr(tmp, " stand for");
        if (stand) { *stand = '\0'; snprintf(concept, sizeof(concept), "%s", tmp); goto what_is; }
    }

    /* "what is meant by X" → "what is X" */
    if ((rest = MATCH("what is meant by "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "tell me about X" → "what is X" */
    if ((rest = MATCH("tell me about "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "explain X" / "explain what X is" → "what is X" */
    if ((rest = MATCH("explain what "))) {
        char tmp[400]; snprintf(tmp, sizeof(tmp), "%s", rest);
        char *is = strstr(tmp, " is"); if (is) *is = '\0';
        snprintf(concept, sizeof(concept), "%s", tmp); goto what_is;
    }
    if ((rest = MATCH("explain "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "describe X" → "what is X" */
    if ((rest = MATCH("describe "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "how would you define X" → "what is X" */
    if ((rest = MATCH("how would you define "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "what do we call X" → "what is X" */
    if ((rest = MATCH("what do we call "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "can you name X" / "can you describe X" → "what is X" */
    if ((rest = MATCH("can you name ")))     { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }
    if ((rest = MATCH("can you describe "))) { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }
    if ((rest = MATCH("can you explain ")))  { snprintf(concept, sizeof(concept), "%s", rest); goto what_is; }

    /* "who was X" → "who is X" */
    if ((rest = MATCH("who was "))) { snprintf(out, out_sz, "who is %s", rest); return; }

    /* "where can you find X" → "where is X" */
    if ((rest = MATCH("where can you find "))) { snprintf(out, out_sz, "where is %s", rest); return; }

    /* "which X" → "what is X" (e.g. "which river is longest") — skip, too risky */

    /* No transformation — pass through */
    snprintf(out, out_sz, "%s", buf);
    return;

what_is:
    /* Strip filler: "the ", "a ", "an " from concept start only if multi-word */
    snprintf(out, out_sz, "what is %s", concept);

    #undef MATCH
}

/* ── Vocabulary ──────────────────────────────────────────── */
#define MAX_VOCAB_SIZE 10000
#define MAX_TOKEN_LEN  64

typedef struct {
    char     words[MAX_VOCAB_SIZE][MAX_TOKEN_LEN];
    uint32_t size;
} VocabTable;

static VocabTable g_vocab = {0};

static int load_vocab(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "[WARN] Cannot load vocab: %s\n", path); return -1; }
    g_vocab.size = 0;
    char line[MAX_TOKEN_LEN];
    while (fgets(line, sizeof(line), fp) && g_vocab.size < MAX_VOCAB_SIZE) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            strncpy(g_vocab.words[g_vocab.size], line, MAX_TOKEN_LEN - 1);
            g_vocab.words[g_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
            g_vocab.size++;
        }
    }
    fclose(fp);
    return 0;
}

static int find_word(const char *word) {
    for (uint32_t i = 0; i < g_vocab.size; i++)
        if (strcmp(g_vocab.words[i], word) == 0) return (int)i;
    return -1;
}

/* ── Fuzzy token matching (edit distance ≤ 2) ───────────── */
static int edit_dist(const char *a, const char *b, int max_d) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (abs(la - lb) > max_d) return max_d + 1;
    if (la > 20 || lb > 20)   return (la == lb) ? 0 : max_d + 1;
    int dp[21][21];
    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;
    for (int i = 1; i <= la; i++) {
        int row_min = max_d + 1;
        for (int j = 1; j <= lb; j++) {
            int c = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = dp[i-1][j-1] + c;
            if (dp[i-1][j]+1 < dp[i][j]) dp[i][j] = dp[i-1][j]+1;
            if (dp[i][j-1]+1 < dp[i][j]) dp[i][j] = dp[i][j-1]+1;
            if (dp[i][j] < row_min) row_min = dp[i][j];
        }
        if (row_min > max_d) return max_d + 1;
    }
    return dp[la][lb];
}

static int find_word_fuzzy(const char *word) {
    int exact = find_word(word);
    if (exact >= 0) return exact;
    int wlen = (int)strlen(word);
    if (wlen < 3) return -1;
    int max_d = (wlen <= 5) ? 1 : 2;
    int best_idx = -1, best_d = max_d + 1;
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        int vlen = (int)strlen(g_vocab.words[i]);
        if (abs(wlen - vlen) > max_d) continue;
        if (max_d == 1 && g_vocab.words[i][0] != word[0]) continue;
        int d = edit_dist(word, g_vocab.words[i], best_d - 1);
        if (d < best_d) {
            best_d = d; best_idx = (int)i;
            if (d == 1) break;
        }
    }
    return (best_d <= max_d) ? best_idx : -1;
}

/* ── Pair Map ────────────────────────────────────────────── */
#define MAX_PAIRS 25000

typedef struct { uint32_t a, b; } Pair;
static Pair     g_pairs[MAX_PAIRS];
static uint32_t g_pair_count = 0;

static int load_pairs(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "[WARN] Cannot load pairs: %s\n", path); return -1; }
    char line[256];
    while (fgets(line, sizeof(line), fp) && g_pair_count < MAX_PAIRS) {
        if (line[0] == '#') continue;
        uint32_t pid, ta, tb;
        if (sscanf(line, "%u %u %u", &pid, &ta, &tb) == 3 && pid < MAX_PAIRS) {
            g_pairs[pid].a = ta;
            g_pairs[pid].b = tb;
            if (pid >= g_pair_count) g_pair_count = pid + 1;
        }
    }
    fclose(fp);
    return 0;
}

static int find_pair(uint32_t a, uint32_t b) {
    for (uint32_t i = 0; i < g_pair_count; i++)
        if (g_pairs[i].a == a && g_pairs[i].b == b) return (int)i;
    return -1;
}

/* ── Tokenizer ───────────────────────────────────────────── */
#define MAX_TOKENS 256

typedef struct {
    char    *strs[MAX_TOKENS];
    uint32_t ids[MAX_TOKENS];
    uint32_t count;
} Tokenization;

static uint32_t djb2(const char *s) {
    uint32_t h = 5381;
    for (int c; (c = *s++);) h = ((h << 5) + h) + c;
    return h;
}

static void tokenize(const char *text, uint32_t dag_vsz, Tokenization *out) {
    memset(out, 0, sizeof(*out));
    char *copy = strdup(text);
    if (!copy) return;
    for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *w = strtok(copy, " \t\n\r");
    while (w && out->count < MAX_TOKENS) {
        out->strs[out->count] = strdup(w);
        int vid = find_word_fuzzy(w);
        out->ids[out->count] = (vid >= 0) ? (uint32_t)vid : (djb2(w) % dag_vsz);
        out->count++;
        w = strtok(NULL, " \t\n\r");
    }
    free(copy);
}

static void tokenize_free(Tokenization *tok) {
    for (uint32_t i = 0; i < tok->count; i++) free(tok->strs[i]);
    memset(tok, 0, sizeof(*tok));
}

/* ── Question embedding ──────────────────────────────────── */
/*
 * FIX: DAG nodes are pair_ids (0..dag->vocab_size-1), NOT token_ids.
 * token_id 20 ("what") ≠ pair_id 20 (some unrelated pair).
 *
 * Correct approach:
 *   1. For each consecutive bigram (ids[i-1], ids[i]), resolve to a pair_id
 *      via find_pair().
 *   2. Look up pair node embedding: eh_hgn_dag_node_vec(dag, pair_id).
 *   3. Mean-pool the valid pair embeddings.
 *
 * Fallback: if no bigram pairs found, try (tok, tok) self-pairs for each
 * known token. If still nothing, output zero vector.
 */
static void question_embedding(const uint32_t *ids, uint32_t n,
                                const EH_HGN_BaseDag *dag, float *out) {
    memset(out, 0, EH_HGN_EMBED_DIM * sizeof(float));
    if (!n) return;

    uint32_t valid = 0;

    /* Phase 1: consecutive bigrams → pair_ids */
    for (uint32_t t = 1; t < n; t++) {
        int pid = find_pair(ids[t-1], ids[t]);
        if (pid < 0) continue;
        const float *v = eh_hgn_dag_node_vec(dag, (uint32_t)pid);
        if (!v) continue;
        for (uint32_t d = 0; d < EH_HGN_EMBED_DIM; d++) out[d] += v[d];
        valid++;
    }

    /* Phase 2 fallback: self-pairs (tok, tok) for each token */
    if (valid == 0) {
        for (uint32_t t = 0; t < n; t++) {
            int pid = find_pair(ids[t], ids[t]);
            if (pid < 0) continue;
            const float *v = eh_hgn_dag_node_vec(dag, (uint32_t)pid);
            if (!v) continue;
            for (uint32_t d = 0; d < EH_HGN_EMBED_DIM; d++) out[d] += v[d];
            valid++;
        }
    }

    if (valid > 0) {
        float scale = 1.0f / (float)valid;
        for (uint32_t d = 0; d < EH_HGN_EMBED_DIM; d++) out[d] *= scale;
    }
    /* If still valid==0: zero vector — no pair info available for this query */
}

/* ── Domain detection ────────────────────────────────────── */
/* Domain IDs: 0=unknown, 1=tech, 2=science, 3=geography, 4=people, 5=nature */
#define DOM_UNKNOWN   0
#define DOM_TECH      1
#define DOM_SCIENCE   2
#define DOM_GEOGRAPHY 3
#define DOM_PEOPLE    4
#define DOM_NATURE    5
#define DOM_MATH      6
#define DOM_EVERYDAY  7

static const char *TECH_WORDS[] = {
    "computer","database","algorithm","network","server","api","python","javascript",
    "html","css","compiler","operating","linux","git","cloud","machine","neural",
    "deep","blockchain","encryption","firewall","cache","bandwidth","latency","cpu",
    "ram","gpu","binary","stack","queue","array","loop","class","recursion",
    "debugging","version","software","hardware","browser","internet","website",
    "program","code","function","variable","pointer","thread","process","memory",
    "byte","pixel","router","dns","tcp","http","sql","json","xml","docker",NULL
};
static const char *SCIENCE_WORDS[] = {
    "gravity","photosynthesis","evolution","dna","cell","atom","molecule",
    "electricity","magnetism","energy","radiation","virus","photon","black","star",
    "planet","solar","entropy","nuclear","quantum","relativity","chemistry",
    "physics","biology","oxygen","hydrogen","carbon","water","light","sound",
    "wave","electron","proton","neutron","acid","base","compound","element",
    "temperature","pressure","velocity","mass","density","force","fission","fusion",NULL
};
static const char *GEO_WORDS[] = {
    "capital","france","japan","germany","italy","spain","china","india","canada",
    "australia","brazil","russia","mexico","egypt","paris","tokyo","berlin","rome",
    "madrid","beijing","delhi","london","moscow","continent","ocean","river",
    "mountain","country","europe","asia","africa","america","pacific","atlantic",
    "sahara","everest","amazon","nile","himalayas","arctic","antarctica",NULL
};
static const char *PEOPLE_WORDS[] = {
    "einstein","newton","darwin","curie","turing","shakespeare","napoleon",
    "gandhi","mandela","lincoln","caesar","cleopatra","edison","columbus",
    "galileo","tesla","aristotle","plato","socrates","historian","philosopher",
    "author","writer","physicist","mathematician","scientist","inventor",NULL
};

static uint32_t detect_domain(const Tokenization *tok) {
    /* Check each token against domain keywords */
    for (uint32_t t = 0; t < tok->count; t++) {
        const char *w = tok->strs[t];
        for (int i = 0; TECH_WORDS[i]; i++)
            if (strcmp(w, TECH_WORDS[i]) == 0) return DOM_TECH;
        for (int i = 0; SCIENCE_WORDS[i]; i++)
            if (strcmp(w, SCIENCE_WORDS[i]) == 0) return DOM_SCIENCE;
        for (int i = 0; GEO_WORDS[i]; i++)
            if (strcmp(w, GEO_WORDS[i]) == 0) return DOM_GEOGRAPHY;
        for (int i = 0; PEOPLE_WORDS[i]; i++)
            if (strcmp(w, PEOPLE_WORDS[i]) == 0) return DOM_PEOPLE;
    }
    return DOM_UNKNOWN;
}

#define MAX_STEPS   8
#define BEAM_WIDTH  8

extern float eh_beam_attention_mix;
typedef struct { float attention_mix; float hub_penalty; float temperature; float rep_penalty; uint32_t max_steps; } Params;

/* Build node_domains array: for each pair_id in DAG, assign a domain
 * based on the tokens that make up the pair. Called once per model load. */
static uint8_t *g_node_domains = NULL;

static void build_node_domains(const EH_HGN_BaseDag *dag) {
    free(g_node_domains);
    g_node_domains = calloc(dag->vocab_size, sizeof(uint8_t));
    if (!g_node_domains) return;

    for (uint32_t pid = 0; pid < dag->vocab_size; pid++) {
        if (pid >= g_pair_count) continue;
        uint32_t ta = g_pairs[pid].a;
        uint32_t tb = g_pairs[pid].b;
        const char *wa = (ta < g_vocab.size) ? g_vocab.words[ta] : "";
        const char *wb = (tb < g_vocab.size) ? g_vocab.words[tb] : "";

        uint8_t dom = DOM_UNKNOWN;
        const char *words[2] = {wa, wb};
        for (int wi = 0; wi < 2 && dom == DOM_UNKNOWN; wi++) {
            const char *w = words[wi];
            for (int i = 0; TECH_WORDS[i]    && dom==DOM_UNKNOWN; i++) if (!strcmp(w,TECH_WORDS[i]))    dom=DOM_TECH;
            for (int i = 0; SCIENCE_WORDS[i] && dom==DOM_UNKNOWN; i++) if (!strcmp(w,SCIENCE_WORDS[i])) dom=DOM_SCIENCE;
            for (int i = 0; GEO_WORDS[i]    && dom==DOM_UNKNOWN; i++) if (!strcmp(w,GEO_WORDS[i]))     dom=DOM_GEOGRAPHY;
            for (int i = 0; PEOPLE_WORDS[i] && dom==DOM_UNKNOWN; i++) if (!strcmp(w,PEOPLE_WORDS[i]))  dom=DOM_PEOPLE;
        }
        g_node_domains[pid] = dom;
    }
}

static void run_query(const char *question,
                      const EH_HGN_BaseDag *dag,
                      const Params *p,
                      char *answer_out, size_t answer_sz)
{
    /* Canonicalize question before processing */
    char canonical[512];
    normalize_query(question, canonical, sizeof(canonical));

    Tokenization tok;
    tokenize(canonical, dag->vocab_size, &tok);

    /* Compute question embedding — mean pool of ALL question tokens */
    float qembed[EH_HGN_EMBED_DIM];
    question_embedding(tok.ids, tok.count, dag, qembed);

    /* ── Option A: Full question context in beam scoring ──
     * Set global question embedding so beam search uses hybrid scoring:
     *   score = (1-mix) * local_context + mix * question_embedding
     * This biases every beam step toward the question's semantic domain,
     * preventing drift to unrelated super-hubs.
     */
    memcpy(eh_beam_question_embedding, qembed, EH_HGN_EMBED_DIM * sizeof(float));
    eh_beam_use_question_embedding = 1;

    /* Detect question domain for domain guidance scoring */
    uint32_t question_domain = detect_domain(&tok);

    /* Find start pair ─────────────────────────────────────────────────
     * Strategy: prefer a concept-anchored starting pair over generic
     * function-word pairs. Priority order:
     *   1. (concept, is)  — e.g., (git, is), (evolution, is)
     *   2. (is, concept)  — if in DAG  
     *   3. Any valid bigram from question right-to-left
     *   4. Embedding-nearest pair with fanout > 0
     */
    uint32_t start_pair = UINT32_MAX;

    /* Find 'is' token ID */
    uint32_t is_id_local = UINT32_MAX;
    for (uint32_t j = 0; j < g_vocab.size; j++) {
        if (strcmp(g_vocab.words[j], "is") == 0) { is_id_local = j; break; }
    }

    /* Priority 1: (concept_word, is) — concept is the LAST meaningful token */
    if (tok.count >= 1) {
        uint32_t concept = tok.ids[tok.count - 1];
        if (is_id_local != UINT32_MAX) {
            int pid = find_pair(concept, is_id_local);
            if (pid >= 0 && (uint32_t)pid < dag->vocab_size &&
                eh_hgn_dag_fanout(dag, (uint32_t)pid) > 0) {
                start_pair = (uint32_t)pid;
            }
        }
        /* Also try any pair starting with concept word */
        if (start_pair == UINT32_MAX) {
            for (uint32_t pid = 0; pid < (uint32_t)g_pair_count && pid < dag->vocab_size; pid++) {
                if (g_pairs[pid].a == concept &&
                    eh_hgn_dag_fanout(dag, pid) > 0) {
                    start_pair = pid;
                    break;
                }
            }
        }
    }

    /* Priority 2: any valid bigram right-to-left (skip if only generic words) */
    if (start_pair == UINT32_MAX) {
        /* Generic tokens that make bad start pairs alone */
        static const char *STOPWORDS[] = {"what","is","a","an","the","who","where","how","does","do",NULL};
        for (int i = (int)tok.count - 1; i >= 1 && start_pair == UINT32_MAX; i--) {
            /* Skip if both tokens are stopwords */
            bool a_stop = false, b_stop = false;
            if (tok.ids[i-1] < g_vocab.size) {
                for (int s = 0; STOPWORDS[s]; s++)
                    if (!strcmp(g_vocab.words[tok.ids[i-1]], STOPWORDS[s])) { a_stop = true; break; }
            }
            if (tok.ids[i] < g_vocab.size) {
                for (int s = 0; STOPWORDS[s]; s++)
                    if (!strcmp(g_vocab.words[tok.ids[i]], STOPWORDS[s])) { b_stop = true; break; }
            }
            if (a_stop && b_stop) continue;  /* Skip pure stopword bigrams */

            int pid = find_pair(tok.ids[i-1], tok.ids[i]);
            if (pid >= 0 && (uint32_t)pid < dag->vocab_size &&
                eh_hgn_dag_fanout(dag, (uint32_t)pid) > 0) {
                start_pair = (uint32_t)pid;
            }
        }
    }

    /* Priority 3: any bigram at all (including stopword pairs) */
    if (start_pair == UINT32_MAX) {
        for (int i = (int)tok.count - 1; i >= 1; i--) {
            int pid = find_pair(tok.ids[i-1], tok.ids[i]);
            if (pid >= 0 && (uint32_t)pid < dag->vocab_size &&
                eh_hgn_dag_fanout(dag, (uint32_t)pid) > 0) {
                start_pair = (uint32_t)pid;
                break;
            }
        }
    }

    /* 3. Embedding fallback: score pairs (pair_id 0..dag->vocab_size-1) by
     *    similarity to full question embedding, preferring pairs with fanout > 0.
     *    dag->vocab_size = number of pair nodes in the trigram DAG.
     *    dag node vector at pair_id = embedding of that pair.
     */
    if (start_pair == UINT32_MAX) {
        float best_score = -1e9f;
        for (uint32_t pid = 0; pid < dag->vocab_size; pid++) {
            /* Skip sink nodes — beam can't expand from them */
            if (eh_hgn_dag_fanout(dag, pid) == 0) continue;

            const float *pv = eh_hgn_dag_node_vec(dag, pid);
            if (!pv) continue;

            float sim = 0.0f;
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++)
                sim += qembed[i] * pv[i];

            if (sim > best_score) {
                best_score = sim;
                start_pair = pid;
            }
        }
    }

    if (start_pair == UINT32_MAX) {
        snprintf(answer_out, answer_sz, "[no pair found]");
        tokenize_free(&tok);
        return;
    }

    /* Set beam params */
    eh_beam_attention_mix         = p->attention_mix;
    eh_beam_query_ctx.hub_penalty = p->hub_penalty;
    eh_beam_temperature           = p->temperature;
    eh_beam_rep_penalty           = p->rep_penalty;

    /* Run beam search with domain guidance */
    EH_HGN_BeamTracker tracker;
    EH_HGN_BeamPath paths_buf[BEAM_WIDTH];
    eh_hgn_beam_init(&tracker, dag, &start_pair, 1, paths_buf, BEAM_WIDTH);

    /* Set domain guidance — steers beam toward question's domain */
    tracker.node_domains  = g_node_domains;
    tracker.target_domain = question_domain;

    for (uint32_t step = 0; step < p->max_steps; step++) {
        if (!eh_hgn_beam_step(&tracker, dag)) break;
    }

    const EH_HGN_BeamPath *best = eh_hgn_beam_get_best(&tracker);
    if (!best || best->seq_len == 0) {
        snprintf(answer_out, answer_sz, "[no answer]");
        tokenize_free(&tok);
        return;
    }

    /* Decode pair IDs to words */
    char buf[512] = {0};
    int  pos = 0;
    for (uint32_t i = 0; i < best->seq_len && pos < 500; i++) {
        uint32_t pid = best->tokens[i];
        if (pid < g_pair_count) {
            uint32_t ta = g_pairs[pid].a;
            uint32_t tb = g_pairs[pid].b;
            /* Print second token of pair (avoid duplicates with chaining) */
            if (i == 0) {
                /* Print both for first pair */
                if (ta < g_vocab.size)
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ", g_vocab.words[ta]);
                if (tb < g_vocab.size)
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ", g_vocab.words[tb]);
            } else {
                /* Subsequent pairs: only print second token */
                if (tb < g_vocab.size)
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ", g_vocab.words[tb]);
                else
                    pos += snprintf(buf + pos, sizeof(buf) - pos, "<p%u> ", pid);
            }
        } else {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "<p%u> ", pid);
        }
    }

    /* Trim trailing space */
    while (pos > 0 && buf[pos-1] == ' ') { buf[pos-1] = '\0'; pos--; }
    snprintf(answer_out, answer_sz, "%s", buf);

    tokenize_free(&tok);
}

/* ── Main ────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *dag_path   = (argc > 1) ? argv[1] : "training/trigram_v15_model.ehdag";
    const char *embed_path = (argc > 2) ? argv[2] : "training/eh_g3_embeddings.bin";
    const char *vocab_path = (argc > 3) ? argv[3] : "training/trigram_v15_vocab.txt";
    const char *pair_path  = (argc > 4) ? argv[4] : "training/trigram_v15_vocab.txt.pairs";

    /* Init arena */
    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);
    if (!arena) { fprintf(stderr, "ERROR: arena alloc\n"); return 1; }

    load_vocab(vocab_path);
    load_pairs(pair_path);

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    if (eh_hgn_dag_load(arena, dag_path, &dag) != EH_HGN_OK) {
        fprintf(stderr, "ERROR: failed to load DAG\n");
        eh_arena_destroy(arena);
        return 1;
    }

    /* Build domain labels for all pair nodes (Option A+B hybrid) */
    build_node_domains(&dag);

    /* Load external embeddings if provided */
    FILE *ef = fopen(embed_path, "rb");
    if (ef) {
        /* File format: [vocab_size:u32][embed_dim:u32][data:float...] */
        uint32_t vsz = 0, edim = 0;
        if (fread(&vsz, 4, 1, ef) == 1 && fread(&edim, 4, 1, ef) == 1) {
            /* embeddings loaded into dag node vectors via the dag loader already,
               this file is used for question embeddings — skip for query mode */
        }
        fclose(ef);
    }

    /* Params: high attention_mix = more question context, strong hub + domain penalty */
    Params p = { 0.7f, 4.0f, 0.5f, 6.0f, 8 };

    /* Read questions from stdin */
    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;
        if (line[0] == '#')    continue;

        char answer[512];
        run_query(line, &dag, &p, answer, sizeof(answer));

        /* Show canonical form in brackets if different */
        char canonical[512];
        normalize_query(line, canonical, sizeof(canonical));
        if (strcmp(canonical, line) != 0)
            printf("Q: %-50s | A: %s  [→%s]\n", line, answer, canonical);
        else
            printf("Q: %-50s | A: %s\n", line, answer);
        fflush(stdout);
    }

    free(g_node_domains);
    eh_arena_destroy(arena);
    return 0;
}
