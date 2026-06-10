/* ============================================================
 * eh_retrieve.c — EH-G3 Retrieval-Based Q&A
 *
 * Reads questions from stdin (one per line).
 * For each question:
 *   1. Canonicalize ("define X" → "what is X")
 *   2. Compute mean-pool question embedding
 *   3. Find nearest answer from answer_pool.txt by cosine sim
 *   4. Output matched answer
 *
 * answer_pool.txt format (one entry per 2 lines):
 *   question text
 *   answer text
 *
 * Compilation:
 *   gcc -O3 -std=c99 -Iinclude -Iinclude/core -Iinclude/hgn \
 *       src/core/*.c src/hgn/*.c examples/eh_retrieve.c \
 *       -o examples/eh_retrieve -lm
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

/* ── Vocab ──────────────────────────────────────────────── */
#define MAX_VOCAB 10000
#define MAX_TLEN  64

static char   g_vocab[MAX_VOCAB][MAX_TLEN];
static int    g_vsz = 0;

static void load_vocab(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[MAX_TLEN];
    while (fgets(line, sizeof(line), f) && g_vsz < MAX_VOCAB) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line)) { strncpy(g_vocab[g_vsz++], line, MAX_TLEN-1); }
    }
    fclose(f);
}

static int vocab_id(const char *w) {
    for (int i = 0; i < g_vsz; i++) if (!strcmp(g_vocab[i], w)) return i;
    return -1;
}

/* ── Fuzzy token matching (edit distance ≤ 2) ───────────── */
/*
 * Levenshtein distance via Wagner-Fischer DP, capped at max_dist.
 * Returns early when partial row exceeds max_dist (pruning).
 * Cost: O(len_a × len_b) worst case, but pruned for long words.
 */
static int edit_distance(const char *a, const char *b, int max_dist) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);
    /* Fast reject: length difference alone exceeds max */
    if (abs(la - lb) > max_dist) return max_dist + 1;
    /* Only handle short words to keep latency low */
    if (la > 20 || lb > 20) return (la == lb) ? 0 : max_dist + 1;

    int dp[21][21];
    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;

    for (int i = 1; i <= la; i++) {
        int row_min = max_dist + 1;
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = dp[i-1][j-1] + cost;
            if (dp[i-1][j] + 1 < dp[i][j]) dp[i][j] = dp[i-1][j] + 1;
            if (dp[i][j-1] + 1 < dp[i][j]) dp[i][j] = dp[i][j-1] + 1;
            if (dp[i][j] < row_min) row_min = dp[i][j];
        }
        if (row_min > max_dist) return max_dist + 1;  /* prune */
    }
    return dp[la][lb];
}

/*
 * Find best vocab match for unknown word w.
 * max_edit: 1 for short words (≤4 chars), 2 for longer.
 * Returns vocab index or -1 if no close match.
 */
static int vocab_id_fuzzy(const char *w) {
    /* Exact match first */
    int exact = vocab_id(w);
    if (exact >= 0) return exact;

    int wlen = (int)strlen(w);
    /* Don't fuzzy-match very short words or stopwords */
    if (wlen < 3) return -1;

    int max_dist = (wlen <= 5) ? 1 : 2;
    int best_idx  = -1;
    int best_dist = max_dist + 1;

    for (int i = 0; i < g_vsz; i++) {
        /* Fast pre-filter: skip if length difference > max_dist */
        int vlen = (int)strlen(g_vocab[i]);
        if (abs(wlen - vlen) > max_dist) continue;

        /* Fast pre-filter: first char must match within 1 char
         * (catches most non-matches immediately) */
        if (max_dist == 1 && g_vocab[i][0] != w[0]) continue;

        int d = edit_distance(w, g_vocab[i], best_dist - 1);
        if (d < best_dist) {
            best_dist = d;
            best_idx  = i;
            if (d == 1) break;  /* good enough — stop searching */
        }
    }
    return (best_dist <= max_dist) ? best_idx : -1;
}

/* ── Tokenize + embed ────────────────────────────────────── */
static uint32_t djb2(const char *s) {
    uint32_t h = 5381; for (int c; (c=*s++);) h=((h<<5)+h)+c; return h;
}

static void text_embedding(const char *text,
                            const EH_HGN_BaseDag *dag,
                            float *out) {
    memset(out, 0, EH_HGN_EMBED_DIM * sizeof(float));
    char buf[512]; strncpy(buf, text, 511); buf[511]=0;
    for (char *p=buf; *p; p++) *p=(char)tolower((unsigned char)*p);
    int count = 0;
    char *w = strtok(buf, " \t\n\r,?.!");
    while (w) {
        int vid = vocab_id_fuzzy(w);   /* fuzzy match — handles typos */
        uint32_t tid = (vid >= 0) ? (uint32_t)vid : (djb2(w) % dag->vocab_size);
        const float *v = eh_hgn_dag_node_vec(dag, tid);
        if (v) {
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++) out[i] += v[i];
            count++;
        }
        w = strtok(NULL, " \t\n\r,?.!");
    }
    if (count > 0) {
        float s = 1.0f / (float)count;
        for (int i = 0; i < EH_HGN_EMBED_DIM; i++) out[i] *= s;
    }
}

/* ── Query normalization ─────────────────────────────────── */
static void normalize_query(const char *raw, char *out, size_t sz) {
    char buf[512]; size_t n = strlen(raw);
    if (n >= 512) n = 511;
    memcpy(buf, raw, n); buf[n] = 0;
    for (char *p=buf; *p; p++) *p=(char)tolower((unsigned char)*p);
    char *end = buf+strlen(buf)-1;
    while (end>buf && (*end=='?'||*end==' '||*end=='.')) *end--=0;

    #define MATCH(pre) (strncmp(buf,(pre),strlen(pre))==0 ? buf+strlen(pre) : NULL)
    const char *rest; char concept[400];

    if ((rest=MATCH("define a ")))    { snprintf(concept,400,"a %s",rest);  goto wi; }
    if ((rest=MATCH("define an ")))   { snprintf(concept,400,"an %s",rest); goto wi; }
    if ((rest=MATCH("define ")))      { snprintf(concept,400,"%s",rest);    goto wi; }
    if ((rest=MATCH("what does "))) {
        char tmp[400]; snprintf(tmp,400,"%s",rest);
        char *m=strstr(tmp," mean"); if(m){*m=0; snprintf(concept,400,"%s",tmp); goto wi;}
        char *sf=strstr(tmp," stand for"); if(sf){*sf=0; snprintf(concept,400,"%s",tmp); goto wi;}
    }
    if ((rest=MATCH("what is meant by ")))    { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("tell me about ")))       { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("explain ")))             { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("describe ")))            { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("how would you define "))){ snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("what do we call ")))     { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("can you name ")))        { snprintf(concept,400,"%s",rest); goto wi; }
    if ((rest=MATCH("who was ")))             { snprintf(out,sz,"who is %s",rest); return; }
    if ((rest=MATCH("where can you find ")))  { snprintf(out,sz,"where is %s",rest); return; }
    snprintf(out, sz, "%s", buf); return;
wi: snprintf(out, sz, "what is %s", concept);
    #undef MATCH
}

/* ── Answer pool ─────────────────────────────────────────── */
#define MAX_POOL 50000

typedef struct {
    char  question[256];
    char  answer[256];
    float embed[EH_HGN_EMBED_DIM];
} PoolEntry;

static PoolEntry *g_pool = NULL;
static int        g_pool_sz = 0;

static int load_pool(const char *path, const EH_HGN_BaseDag *dag) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[WARN] Cannot open pool: %s\n", path); return -1; }

    g_pool = calloc(MAX_POOL, sizeof(PoolEntry));
    if (!g_pool) { fclose(f); return -1; }

    char line[512];
    while (fgets(line, sizeof(line), f) && g_pool_sz < MAX_POOL) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        line[strcspn(line, "\r\n")] = 0;
        if (!strlen(line)) continue;

        /* Lines starting with "q:" or just text — alternating q/a */
        char *text = line;
        if (strncmp(text, "q:", 2) == 0) text += 2;
        if (strncmp(text, "a:", 2) == 0) text += 2;
        while (*text == ' ') text++;

        /* We use a simple state machine: even lines = question, odd = answer */
        /* But the pool file is: q_line, a_line, q_line, a_line ... */
        /* We'll read pairs */
        char q_buf[256], a_buf[256];
        strncpy(q_buf, text, 255);

        /* Read next non-empty line as answer */
        bool got_answer = false;
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            line[strcspn(line, "\r\n")] = 0;
            if (!strlen(line)) continue;
            char *atext = line;
            if (strncmp(atext, "a:", 2) == 0) atext += 2;
            if (strncmp(atext, "q:", 2) == 0) {
                /* Next question — save current and use this as next q */
                strncpy(q_buf, atext+2, 255);
                continue;
            }
            while (*atext == ' ') atext++;
            strncpy(a_buf, atext, 255);
            got_answer = true;
            break;
        }
        if (!got_answer) break;

        PoolEntry *e = &g_pool[g_pool_sz];
        strncpy(e->question, q_buf, 255);
        strncpy(e->answer, a_buf, 255);

        /* Compute question embedding */
        char canonical[512];
        normalize_query(q_buf, canonical, sizeof(canonical));
        text_embedding(canonical, dag, e->embed);

        g_pool_sz++;
    }
    fclose(f);
    fprintf(stderr, "[OK] Loaded %d pool entries from %s\n", g_pool_sz, path);
    return 0;
}

/* Cosine similarity (embeddings may not be normalized) */
static float cosine_sim(const float *a, const float *b) {
    float dot=0, na=0, nb=0;
    for (int i=0; i<EH_HGN_EMBED_DIM; i++) {
        dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i];
    }
    if (na < 1e-9f || nb < 1e-9f) return 0.0f;
    return dot / (sqrtf(na) * sqrtf(nb));
}

/* ── Retrieve ────────────────────────────────────────────── */
static const char* retrieve(const char *question,
                              const EH_HGN_BaseDag *dag,
                              float *score_out) {
    char canonical[512];
    normalize_query(question, canonical, sizeof(canonical));

    float qembed[EH_HGN_EMBED_DIM];
    text_embedding(canonical, dag, qembed);

    float best = -1e9f;
    int   best_idx = -1;

    for (int i = 0; i < g_pool_sz; i++) {
        float s = cosine_sim(qembed, g_pool[i].embed);
        if (s > best) { best = s; best_idx = i; }
    }

    if (score_out) *score_out = best;
    return (best_idx >= 0) ? g_pool[best_idx].answer : "[no match]";
}

/* ── Main ────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *dag_path   = (argc > 1) ? argv[1] : "training/trigram_v20_model.ehdag";
    const char *embed_path = (argc > 2) ? argv[2] : "training/eh_g3_embeddings.bin";
    const char *vocab_path = (argc > 3) ? argv[3] : "training/trigram_v20_vocab.txt";
    const char *pool_path  = (argc > 4) ? argv[4] : "training/answer_pool.txt";

    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);
    if (!arena) { fprintf(stderr, "ERROR: arena\n"); return 1; }

    load_vocab(vocab_path);

    EH_HGN_BaseDag dag; memset(&dag, 0, sizeof(dag));
    if (eh_hgn_dag_load(arena, dag_path, &dag) != EH_HGN_OK) {
        fprintf(stderr, "ERROR: DAG load\n"); eh_arena_destroy(arena); return 1;
    }

    if (load_pool(pool_path, &dag) < 0) {
        fprintf(stderr, "ERROR: Pool load failed\n"); eh_arena_destroy(arena); return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strlen(line) || line[0]=='#') continue;

        float score;
        const char *answer = retrieve(line, &dag, &score);

        char canonical[512];
        normalize_query(line, canonical, sizeof(canonical));
        if (strcmp(canonical, line) != 0)
            printf("Q: %-50s | A: %s  [sim=%.3f →%s]\n", line, answer, score, canonical);
        else
            printf("Q: %-50s | A: %s  [sim=%.3f]\n", line, answer, score);
        fflush(stdout);
    }

    free(g_pool);
    eh_arena_destroy(arena);
    return 0;
}
