/* ================================================================
 * examples/qa_attention.c — Q&A with Full-Question Attention
 *
 * EH-G2 (Generation 2) - Attention-based inference
 *
 * Key Innovation: Uses ALL question pairs for context, not just
 * sliding window. Solves hub node collision problem.
 *
 * Algorithm:
 *   1. Load model and vocabulary
 *   2. Tokenize question → pairs
 *   3. Compute question embedding = average(ALL pair embeddings)
 *   4. At each generation step:
 *      - Score edges by: prior + similarity(edge, question_embedding)
 *      - Question embedding acts as "query" in attention
 *   5. Generate answer tokens
 *
 * Usage:
 *   ./qa_attention ask <model.ehdag> <vocab.txt> <pair_map> <question>
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/qa_attention.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o qa_attention -lm
 * ================================================================ */

#include "hgn/eh_hgn_dag.h"
#include "hgn/eh_beam_search.h"
#include "hgn/eh_hgn_io.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_VOCAB_SIZE  2000
#define MAX_PAIR_SIZE   50000
#define MAX_TOKEN_LEN   32
#define MAX_LINE_LEN    256
#define EH_EMBED_DIM    128

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

/* Use external question embedding from beam_search.c (EH-G2) */
extern float eh_beam_question_embedding[EH_EMBED_DIM];
extern int eh_beam_use_question_embedding;
extern float eh_beam_attention_mix;

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
        if (line[0] == '#') continue;
        
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
 * Find Token/Pair
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
 * Convert Tokens to Pairs
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
        }
    }
    
    return pair_count;
}

/* ----------------------------------------------------------------
 * Compute Question Embedding (Attention Query)
 *
 * This is the KEY to EH-G2:
 * Instead of using only last 3 tokens, we use ALL question pairs!
 * ---------------------------------------------------------------- */
static void compute_question_embedding(const uint32_t *pair_ids, 
                                      uint32_t n_pairs,
                                      const EH_HGN_BaseDag *dag)
{
    /* Clear embedding */
    for (int i = 0; i < EH_EMBED_DIM; i++) {
        eh_beam_question_embedding[i] = 0.0f;
    }
    
    if (n_pairs == 0) {
        eh_beam_use_question_embedding = 0;
        return;
    }
    
    /* Sum all pair embeddings */
    for (uint32_t p = 0; p < n_pairs; p++) {
        const float *pair_emb = eh_hgn_dag_node_vec(dag, pair_ids[p]);
        if (pair_emb) {
            for (int i = 0; i < EH_EMBED_DIM; i++) {
                eh_beam_question_embedding[i] += pair_emb[i];
            }
        }
    }
    
    /* Average (normalize by number of pairs) */
    float scale = 1.0f / (float)n_pairs;
    for (int i = 0; i < EH_EMBED_DIM; i++) {
        eh_beam_question_embedding[i] *= scale;
    }
    
    eh_beam_use_question_embedding = 1;
    
    printf("✓ Question embedding computed from %u pairs\n", n_pairs);
}

/* ----------------------------------------------------------------
 * Ask Mode with Attention
 * ---------------------------------------------------------------- */
static int ask_mode(const char *model_path, const char *vocab_path,
                   const char *pair_path, const char *question,
                   float mix_ratio)
{
    printf("\n=== EH-G2: Q&A with Full-Question Attention ===\n");
    printf("Mix Ratio: %.1f%% local + %.1f%% global\n\n",
           (1.0f - mix_ratio) * 100, mix_ratio * 100);
    
    /* Set mixing ratio */
    eh_beam_attention_mix = mix_ratio;
    
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
        fprintf(stderr, "Error: Cannot load model\n");
        eh_arena_destroy(arena);
        return 1;
    }
    printf("Model:      %u pairs, %u edges\n\n", dag.vocab_size, dag.total_edges);
    
    /* Tokenize question */
    uint32_t tokens[64];
    uint32_t n_tokens = tokenize(question, tokens, 64);
    
    if (n_tokens < 2) {
        fprintf(stderr, "Error: Need at least 2 tokens\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Question: \"%s\"\n", question);
    printf("Tokens:   ");
    for (uint32_t i = 0; i < n_tokens; i++) {
        printf("%s ", g_vocab.tokens[tokens[i]]);
    }
    printf("\n");
    
    /* Convert to pairs */
    uint32_t pair_ids[64];
    uint32_t n_pairs = tokens_to_pairs(tokens, n_tokens, pair_ids);
    
    if (n_pairs == 0) {
        fprintf(stderr, "Error: No valid pairs\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Pairs:    ");
    for (uint32_t i = 0; i < n_pairs; i++) {
        uint32_t a = g_pair_map.pairs[pair_ids[i]].token_a;
        uint32_t b = g_pair_map.pairs[pair_ids[i]].token_b;
        printf("(%s,%s) ", g_vocab.tokens[a], g_vocab.tokens[b]);
    }
    printf("\n\n");
    
    /* *** EH-G2 KEY INNOVATION: Compute question embedding *** */
    compute_question_embedding(pair_ids, n_pairs, &dag);
    printf("\n");
    
    /* Initialize beam search with last pair */
    EH_HGN_BeamTracker tracker;
    uint32_t initial_pair = pair_ids[n_pairs - 1];
    eh_hgn_beam_init(&tracker, &dag, &initial_pair, 1);
    
    /* Run attention-based generation and print answer tokens */
    printf("Answer:   ");
    fflush(stdout);
    
    uint32_t last_pair = initial_pair;
    
    for (int step = 0; step < 8; step++) {
        uint32_t active = eh_hgn_beam_step(&tracker, &dag);
        if (active == 0) break;  /* All beams finished */
        
        const EH_HGN_BeamPath *best = eh_hgn_beam_get_best(&tracker);
        if (!best || best->seq_len == 0) break;
        
        if (best->is_finished) break;  /* Beam reached terminal state */
        
        uint32_t new_pair_id = best->tokens[best->seq_len - 1];
        
        if (new_pair_id < g_pair_map.size && new_pair_id != last_pair) {
            uint32_t tok_b = g_pair_map.pairs[new_pair_id].token_b;
            if (tok_b < g_vocab.size) {
                printf("%s ", g_vocab.tokens[tok_b]);
                fflush(stdout);
            }
            last_pair = new_pair_id;
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
        fprintf(stderr, "EventHorizon G2 - Q&A with Attention\n\n");
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s ask <model.ehdag> <vocab.txt> <pair_map> <question> [mix_ratio]\n", argv[0]);
        fprintf(stderr, "\nArguments:\n");
        fprintf(stderr, "  mix_ratio: Optional, default 0.3 (30%% global, 70%% local)\n");
        fprintf(stderr, "             0.0 = pure local (EH-G1)\n");
        fprintf(stderr, "             0.3 = 70%% local + 30%% global (default)\n");
        fprintf(stderr, "             0.5 = 50%% local + 50%% global (balanced)\n");
        fprintf(stderr, "             1.0 = pure global (experimental)\n");
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s ask model.ehdag vocab.txt vocab.txt.pairs \"what is your name\" 0.3\n", argv[0]);
        return 1;
    }
    
    const char *mode = argv[1];
    
    if (strcmp(mode, "ask") == 0) {
        float mix_ratio = 0.3f;  /* Default */
        if (argc >= 7) {
            mix_ratio = atof(argv[6]);
            if (mix_ratio < 0.0f) mix_ratio = 0.0f;
            if (mix_ratio > 1.0f) mix_ratio = 1.0f;
        }
        return ask_mode(argv[2], argv[3], argv[4], argv[5], mix_ratio);
    }
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }
}
