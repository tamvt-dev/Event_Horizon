/* ================================================================
 * examples/qa_trigram.c — Q&A Demo with Trigram Model
 *
 * Inference for trigram models where nodes are (token_i, token_j) pairs.
 *
 * Usage:
 *   ./qa_trigram ask <model.ehdag> <vocab.txt> <pair_map> <question>
 *
 * Example:
 *   ./qa_trigram ask trigram_model.ehdag trigram_vocab.txt \
 *                    trigram_vocab.txt.pairs "what is your name"
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/qa_trigram.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o qa_trigram -lm
 * ================================================================ */

#include "hgn/eh_hgn_engine.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VOCAB_SIZE  2000
#define MAX_PAIR_SIZE   50000
#define MAX_TOKEN_LEN   32
#define MAX_LINE_LEN    256

/* ----------------------------------------------------------------
 * Vocabularies
 * ---------------------------------------------------------------- */
typedef struct {
    char tokens[MAX_VOCAB_SIZE][MAX_TOKEN_LEN];
    uint32_t size;
} Vocabulary;

typedef struct {
    uint32_t token_a;
    uint32_t token_b;
} TokenPair;

typedef struct {
    TokenPair pairs[MAX_PAIR_SIZE];
    uint32_t size;
} PairMap;

static Vocabulary g_vocab = {0};
static PairMap g_pair_map = {0};

/* ----------------------------------------------------------------
 * Option B — Level 2: Concept Cluster Classifier
 *
 * Maps token strings to DomainIDs, then classifies:
 *   - The prompt question     → target_domain
 *   - Every pair in the DAG  → node_domains[pair_id]
 *
 * Domain ID 0 = neutral/unknown (no guidance applied)
 * ---------------------------------------------------------------- */
#define DOMAIN_UNKNOWN      0u
#define DOMAIN_IDENTITY     1u
#define DOMAIN_GREETINGS    2u
#define DOMAIN_CAPABILITIES 3u
#define DOMAIN_MATH         4u
#define DOMAIN_SCIENCE      5u
#define DOMAIN_TECHNOLOGY   6u
#define DOMAIN_GEOGRAPHY    7u
#define DOMAIN_ANIMALS      8u
#define DOMAIN_PHILOSOPHY   9u
#define DOMAIN_FUN         10u
#define DOMAIN_COUNT       11u

/* Keyword lists — NULL-terminated, domain index == DOMAIN_* */
static const char *g_domain_kw[DOMAIN_COUNT][20] = {
    /* 0 UNKNOWN */      {NULL},
    /* 1 IDENTITY */     {"assistant","ai","model","name","called","bot","myself",NULL},
    /* 2 GREETINGS */    {"hello","hi","hey","fine","good","morning","greet",NULL},
    /* 3 CAPABILITIES */ {"help","tasks","write","calculate","can","speak","jokes",NULL},
    /* 4 MATH */         {"plus","minus","times","divided","equal","four","seven",
                          "twenty","pi","five","two","ten","sum","number",NULL},
    /* 5 SCIENCE */      {"star","sun","earth","planet","water","liquid","oxygen",
                          "atom","molecule","energy","electrons","gravity","dna",
                          "genetic","photosynthesis","atmosphere","force",NULL},
    /* 6 TECHNOLOGY */   {"python","programming","language","internet","network",
                          "database","html","css","javascript","cpu","ram",
                          "memory","software","hardware","computer",NULL},
    /* 7 GEOGRAPHY */    {"paris","france","capital","city","tokyo","japan",
                          "washington","everest","mountain","amazon","river",
                          "sahara","desert","pacific","ocean","russia",NULL},
    /* 8 ANIMALS */      {"dog","cat","whale","mammal","fish","shark","lion",
                          "tiger","elephant","dolphin","bird","penguin",NULL},
    /* 9 PHILOSOPHY */   {"love","happiness","emotion","mind","joy","purpose",
                          "meaning","life","exist",NULL},
    /* 10 FUN */         {"joke","funny","laugh","chicken","road",NULL},
};

/* Classify a single token string → best matching domain */
static uint32_t classify_token(const char *word) {
    for (uint32_t dom = 1; dom < DOMAIN_COUNT; dom++) {
        for (uint32_t k = 0; g_domain_kw[dom][k] != NULL; k++) {
            if (strcmp(word, g_domain_kw[dom][k]) == 0)
                return dom;
        }
    }
    return DOMAIN_UNKNOWN;
}

/*
 * Classify question tokens[] → target_domain.
 * Count keyword hits per domain, return winner (ties: lower domain ID).
 */
static uint32_t classify_question(const uint32_t *tokens, uint32_t n_tokens) {
    uint32_t hits[DOMAIN_COUNT] = {0};
    for (uint32_t i = 0; i < n_tokens; i++) {
        if (tokens[i] >= g_vocab.size) continue;
        uint32_t dom = classify_token(g_vocab.tokens[tokens[i]]);
        if (dom != DOMAIN_UNKNOWN) hits[dom]++;
    }
    uint32_t best_dom = DOMAIN_UNKNOWN;
    uint32_t best_cnt = 0;
    for (uint32_t d = 1; d < DOMAIN_COUNT; d++) {
        if (hits[d] > best_cnt) { best_cnt = hits[d]; best_dom = d; }
    }
    return best_dom;
}

/*
 * Build node_domains[dag_vocab_size] by classifying each pair node:
 *   - Lookup token_a and token_b strings for pair_id
 *   - Check both tokens against domain keyword lists
 *   - Assign the non-UNKNOWN domain found (token_b preferred as content word)
 */
static void build_node_domains(const EH_HGN_BaseDag *dag,
                                uint8_t *node_domains)
{
    for (uint32_t pid = 0; pid < dag->vocab_size; pid++) {
        node_domains[pid] = (uint8_t)DOMAIN_UNKNOWN;
        if (pid >= g_pair_map.size) continue;

        uint32_t ta = g_pair_map.pairs[pid].token_a;
        uint32_t tb = g_pair_map.pairs[pid].token_b;

        uint32_t dom = DOMAIN_UNKNOWN;

        /* Prefer content word (token_b) for domain signal */
        if (tb < g_vocab.size)
            dom = classify_token(g_vocab.tokens[tb]);
        if (dom == DOMAIN_UNKNOWN && ta < g_vocab.size)
            dom = classify_token(g_vocab.tokens[ta]);

        node_domains[pid] = (uint8_t)dom;
    }
}

/* Domain name for logging */
static const char *domain_name(uint32_t dom) {
    static const char *names[DOMAIN_COUNT] = {
        "unknown","identity","greetings","capabilities",
        "math","science","technology","geography",
        "animals","philosophy","fun"
    };
    return (dom < DOMAIN_COUNT) ? names[dom] : "?";
}

/* ----------------------------------------------------------------
 * Load Vocabulary
 * ---------------------------------------------------------------- */
static int vocab_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    g_vocab.size = 0;
    char line[MAX_TOKEN_LEN];
    
    while (fgets(line, sizeof(line), fp) && g_vocab.size < MAX_VOCAB_SIZE) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strncpy(g_vocab.tokens[g_vocab.size], line, MAX_TOKEN_LEN - 1);
            g_vocab.tokens[g_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
            g_vocab.size++;
        }
    }
    
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Load Pair Map
 * ---------------------------------------------------------------- */
static int pair_map_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    g_pair_map.size = 0;
    char line[MAX_LINE_LEN];
    
    while (fgets(line, sizeof(line), fp) && g_pair_map.size < MAX_PAIR_SIZE) {
        if (line[0] == '#') continue;  /* Skip comments */
        
        uint32_t pair_id, tok_a, tok_b;
        if (sscanf(line, "%u %u %u", &pair_id, &tok_a, &tok_b) == 3) {
            if (pair_id == g_pair_map.size) {
                g_pair_map.pairs[pair_id].token_a = tok_a;
                g_pair_map.pairs[pair_id].token_b = tok_b;
                g_pair_map.size++;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Find Token ID
 * ---------------------------------------------------------------- */
static int vocab_find(const char *token)
{
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        if (strcmp(g_vocab.tokens[i], token) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * Find Pair ID
 * ---------------------------------------------------------------- */
static int pair_find(uint32_t tok_a, uint32_t tok_b)
{
    for (uint32_t i = 0; i < g_pair_map.size; i++) {
        if (g_pair_map.pairs[i].token_a == tok_a &&
            g_pair_map.pairs[i].token_b == tok_b) {
            return (int)i;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * Tokenize
 * ---------------------------------------------------------------- */
static uint32_t tokenize(const char *text, uint32_t *tokens, uint32_t max_tokens)
{
    char buffer[MAX_LINE_LEN];
    strncpy(buffer, text, MAX_LINE_LEN - 1);
    buffer[MAX_LINE_LEN - 1] = '\0';
    
    uint32_t count = 0;
    char *token = strtok(buffer, " \t\n");
    
    while (token && count < max_tokens) {
        for (char *p = token; *p; p++) *p = tolower(*p);
        
        int id = vocab_find(token);
        if (id >= 0) {
            tokens[count++] = (uint32_t)id;
        }
        token = strtok(NULL, " \t\n");
    }
    
    return count;
}

/* ----------------------------------------------------------------
 * Convert Token Sequence to Pair Sequence
 * ---------------------------------------------------------------- */
static uint32_t tokens_to_pairs(const uint32_t *tokens, uint32_t n, 
                                uint32_t *pairs)
{
    if (n < 2) return 0;
    
    uint32_t pair_count = 0;
    for (uint32_t i = 0; i + 1 < n; i++) {
        int pair_id = pair_find(tokens[i], tokens[i + 1]);
        if (pair_id >= 0) {
            pairs[pair_count++] = (uint32_t)pair_id;
        } else {
            /* Pair not in training - use fallback */
            printf("  [Warning] Pair (%s, %s) not in model\n",
                   g_vocab.tokens[tokens[i]], g_vocab.tokens[tokens[i + 1]]);
        }
    }
    
    return pair_count;
}

/* ----------------------------------------------------------------
 * Ask Mode
 * ---------------------------------------------------------------- */
static int ask_mode(const char *model_path, const char *vocab_path,
                   const char *pair_path, const char *question)
{
    printf("\n=== Trigram Q&A Ask Mode ===\n\n");
    
    /* Load vocabulary and pairs */
    if (vocab_load(vocab_path) != 0) {
        fprintf(stderr, "Error: Cannot load vocabulary\n");
        return 1;
    }
    printf("Vocabulary: %u tokens\n", g_vocab.size);
    
    if (pair_map_load(pair_path) != 0) {
        fprintf(stderr, "Error: Cannot load pair map\n");
        return 1;
    }
    printf("Pair Map:   %u pairs\n\n", g_pair_map.size);
    
    /* Load model */
    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);
    EH_HGN_BaseDag dag;
    
    EH_HGN_IO_Status io_rc = eh_hgn_io_load(arena, model_path, &dag);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Cannot load model (%s)\n", eh_hgn_io_strerror(io_rc));
        eh_arena_destroy(arena);
        return 1;
    }
    printf("Model:      %u pairs, %u edges\n\n", dag.vocab_size, dag.total_edges);
    
    /* Tokenize question */
    uint32_t tokens[64];
    uint32_t n_tokens = tokenize(question, tokens, 64);
    
    if (n_tokens < 2) {
        fprintf(stderr, "Error: Need at least 2 tokens for trigram model\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Question: \"%s\"\n", question);
    printf("Tokens:   ");
    for (uint32_t i = 0; i < n_tokens; i++) {
        printf("%s ", g_vocab.tokens[tokens[i]]);
    }
    printf("\n");
    
    /* Convert tokens to pairs */
    uint32_t pair_ids[64];
    uint32_t n_pairs = tokens_to_pairs(tokens, n_tokens, pair_ids);
    
    if (n_pairs == 0) {
        fprintf(stderr, "Error: No valid pairs for input\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Pairs:    ");
    for (uint32_t i = 0; i < n_pairs; i++) {
        uint32_t pair_id = pair_ids[i];
        uint32_t a = g_pair_map.pairs[pair_id].token_a;
        uint32_t b = g_pair_map.pairs[pair_id].token_b;
        printf("(%s,%s) ", g_vocab.tokens[a], g_vocab.tokens[b]);
    }
    printf("\n\n");
    
    /* Run inference with full prompt sequence */
    EH_HGN_InferenceSession session;
    EH_HGN_EngineConfig config = eh_hgn_default_config();
    config.max_steps = 10;  /* Generate up to 10 pair transitions */
    
    int rc = eh_hgn_session_init(&session, &dag, arena, pair_ids, n_pairs, &config);
    if (rc != 0) {
        fprintf(stderr, "Error: Session init failed\n");
        eh_arena_destroy(arena);
        return 1;
    }

    /* ── Option B: Hierarchical Graph domain guidance ──────────────
     * 1. Classify question → target_domain (Level 2 concept cluster)
     * 2. Build node_domains[pair_id] for entire DAG (Level 1→Level 2 map)
     * 3. Wire into beam tracker — domain-guided scoring activates
     *    automatically in eh_hgn_beam_step.
     */
    static uint8_t s_node_domains[50000];  /* max pairs in model */
    uint32_t target_domain = classify_question(tokens, n_tokens);
    build_node_domains(&dag, s_node_domains);
    session.beam_tracker.node_domains = s_node_domains;
    session.beam_tracker.target_domain = target_domain;
    printf("Domain:   %s (id=%u)\n", domain_name(target_domain), target_domain);

    
    /* Track how many tokens we have printed (start with prompt length) */
    uint32_t printed_len = n_pairs;
    
    /* Generate continuation */
    while (!eh_hgn_session_is_done(&session)) {
        uint32_t active = eh_hgn_session_step(&session);
        if (active == 0) break;
        
        const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
        if (best && best->seq_len > printed_len) {
            /* Print any newly generated pairs in this step */
            for (uint32_t i = printed_len; i < best->seq_len; i++) {
                uint32_t new_pair_id = best->tokens[i];
                if (new_pair_id < g_pair_map.size) {
                    /* Print second token of the pair (first token is from previous pair in sequence) */
                    uint32_t tok_b = g_pair_map.pairs[new_pair_id].token_b;
                    if (tok_b < g_vocab.size) {
                        printf("%s ", g_vocab.tokens[tok_b]);
                        fflush(stdout);
                    }
                }
            }
            printed_len = best->seq_len;
        }
    }
    printf("\n\n");
    
    eh_arena_destroy(arena);
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s ask <model.ehdag> <vocab.txt> <pair_map> <question>\n", argv[0]);
        return 1;
    }
    
    const char *mode = argv[1];
    
    if (strcmp(mode, "ask") == 0) {
        return ask_mode(argv[2], argv[3], argv[4], argv[5]);
    }
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }
}
