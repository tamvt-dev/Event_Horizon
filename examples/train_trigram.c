/* ================================================================
 * examples/train_trigram.c — Trigram-based HGN Training
 *
 * Improves upon bigram models by using 2-token context windows.
 * This captures longer dependencies: "your name" → "my" vs "the name" → "is"
 *
 * Architecture:
 *   - Nodes represent (token_i, token_j) pairs
 *   - Edges go from (token_i, token_j) → token_k
 *   - Embeddings are computed from both tokens in pair
 *   - Much better context understanding!
 *
 * Graph Size:
 *   - Vocabulary: N tokens
 *   - Potential nodes: N² pairs (sparsely populated)
 *   - In practice: only ~10-20% of pairs appear in corpus
 *
 * Usage:
 *   ./train_trigram <input.txt> <model.ehdag> <vocab.txt>
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/train_trigram.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o train_trigram -lm
 * ================================================================ */

#include "hgn/eh_hgn_builder.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_BASE_VOCAB      4000   /* Base token vocabulary */
#define MAX_PAIR_VOCAB      50000  /* Max (token_i, token_j) pairs */
#define MAX_TOKEN_LEN       32
#define MAX_LINE_LEN        256

/* ----------------------------------------------------------------
 * Base Vocabulary (single tokens)
 * ---------------------------------------------------------------- */
typedef struct {
    char tokens[MAX_BASE_VOCAB][MAX_TOKEN_LEN];
    float embeddings[MAX_BASE_VOCAB][EH_HGN_EMBED_DIM];
    uint32_t counts[MAX_BASE_VOCAB];
    uint32_t size;
} BaseVocab;

static BaseVocab g_base_vocab = {0};

/* ----------------------------------------------------------------
 * Pair Vocabulary (bigram states for trigram model)
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t token_a;  /* First token in pair */
    uint32_t token_b;  /* Second token in pair */
    uint32_t count;    /* Frequency */
} TokenPair;

typedef struct {
    TokenPair pairs[MAX_PAIR_VOCAB];
    float embeddings[MAX_PAIR_VOCAB][EH_HGN_EMBED_DIM];  /* Pair embeddings */
    uint32_t size;
} PairVocab;

static PairVocab g_pair_vocab = {0};

/* ----------------------------------------------------------------
 * Co-occurrence Matrix for Semantic Base Embeddings
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t *indices;
    float *weights;
    uint32_t count;
    uint32_t capacity;
} CoOccList;

static CoOccList g_co_occ[MAX_BASE_VOCAB] = {0};

static void add_co_occurrence(uint32_t token_a, uint32_t token_b, float weight)
{
    if (token_a >= MAX_BASE_VOCAB) return;
    CoOccList *list = &g_co_occ[token_a];
    for (uint32_t i = 0; i < list->count; i++) {
        if (list->indices[i] == token_b) {
            list->weights[i] += weight;
            return;
        }
    }
    if (list->count >= list->capacity) {
        uint32_t new_cap = (list->capacity == 0) ? 32 : list->capacity * 2;
        list->indices = realloc(list->indices, new_cap * sizeof(uint32_t));
        list->weights = realloc(list->weights, new_cap * sizeof(float));
        list->capacity = new_cap;
    }
    list->indices[list->count] = token_b;
    list->weights[list->count] = weight;
    list->count++;
}

static void build_co_occurrence(const uint32_t *tokens, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t center = tokens[i];
        for (uint32_t j = 1; j <= 5 && i + j < n; j++) {
            uint32_t context = tokens[i + j];
            float weight = 1.0f / (float)j;
            add_co_occurrence(center, context, weight);
            add_co_occurrence(context, center, weight);
        }
    }
}

static void generate_semantic_embeddings(void)
{
    printf("Generating semantic base embeddings...\n");
    for (uint32_t i = 0; i < g_base_vocab.size; i++) {
        float temp[EH_HGN_EMBED_DIM] = {0};
        CoOccList *list = &g_co_occ[i];
        for (uint32_t j = 0; j < list->count && j < EH_HGN_EMBED_DIM; j++) {
            uint32_t co_token = list->indices[j];
            float co_weight = list->weights[j];
            float basis_value = sinf((float)co_token * 0.1f + (float)j * 0.05f);
            int target_dim = (int)(j % EH_HGN_EMBED_DIM);
            temp[target_dim] += basis_value * co_weight;
        }
        float norm = 0.0f;
        for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
            norm += temp[d] * temp[d];
        }
        norm = sqrtf(norm);
        if (norm > 1e-6f) {
            for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
                g_base_vocab.embeddings[i][d] = temp[d] / norm;
            }
        }
    }
    printf("  ✓ Done generating semantic base embeddings\n");
}

/* ----------------------------------------------------------------
 * Trigram Edge Statistics
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t src_pair;   /* Source: pair ID */
    uint32_t dst_token;  /* Destination: single token */
    uint32_t count;
} TrigramEdge;

static TrigramEdge *g_trigram_edges = NULL;
static uint32_t g_trigram_count = 0;
static uint32_t g_trigram_capacity = 0;

/* ----------------------------------------------------------------
 * Base Vocabulary Functions
 * ---------------------------------------------------------------- */
static int base_vocab_find(const char *token)
{
    for (uint32_t i = 0; i < g_base_vocab.size; i++) {
        if (strcmp(g_base_vocab.tokens[i], token) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int base_vocab_add(const char *token)
{
    if (g_base_vocab.size >= MAX_BASE_VOCAB) return -1;
    
    strncpy(g_base_vocab.tokens[g_base_vocab.size], token, MAX_TOKEN_LEN - 1);
    g_base_vocab.tokens[g_base_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
    g_base_vocab.counts[g_base_vocab.size] = 0;
    
    /* Initialize embedding (will be refined later) */
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        g_base_vocab.embeddings[g_base_vocab.size][i] = 
            sinf((float)g_base_vocab.size * 0.1f + (float)i * 0.01f);
    }
    
    return g_base_vocab.size++;
}

static int base_vocab_get_or_add(const char *token)
{
    int id = base_vocab_find(token);
    if (id >= 0) {
        g_base_vocab.counts[id]++;
        return id;
    }
    
    int new_id = base_vocab_add(token);
    if (new_id >= 0) {
        g_base_vocab.counts[new_id] = 1;
    }
    return new_id;
}

/* ----------------------------------------------------------------
 * Pair Vocabulary Functions
 * ---------------------------------------------------------------- */
static int pair_vocab_find(uint32_t token_a, uint32_t token_b)
{
    for (uint32_t i = 0; i < g_pair_vocab.size; i++) {
        if (g_pair_vocab.pairs[i].token_a == token_a &&
            g_pair_vocab.pairs[i].token_b == token_b) {
            return (int)i;
        }
    }
    return -1;
}

static int pair_vocab_add(uint32_t token_a, uint32_t token_b)
{
    if (g_pair_vocab.size >= MAX_PAIR_VOCAB) return -1;
    
    uint32_t idx = g_pair_vocab.size;
    g_pair_vocab.pairs[idx].token_a = token_a;
    g_pair_vocab.pairs[idx].token_b = token_b;
    g_pair_vocab.pairs[idx].count = 0;
    
    /* Pair embedding = average of two token embeddings */
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        g_pair_vocab.embeddings[idx][i] = 
            (g_base_vocab.embeddings[token_a][i] + 
             g_base_vocab.embeddings[token_b][i]) * 0.5f;
    }
    
    return g_pair_vocab.size++;
}

static int pair_vocab_get_or_add(uint32_t token_a, uint32_t token_b)
{
    int id = pair_vocab_find(token_a, token_b);
    if (id >= 0) {
        g_pair_vocab.pairs[id].count++;
        return id;
    }
    
    int new_id = pair_vocab_add(token_a, token_b);
    if (new_id >= 0) {
        g_pair_vocab.pairs[new_id].count = 1;
    }
    return new_id;
}

/* ----------------------------------------------------------------
 * Trigram Edge Functions
 * ---------------------------------------------------------------- */
static void add_trigram_edge(uint32_t src_pair, uint32_t dst_token)
{
    /* Check if edge exists */
    for (uint32_t i = 0; i < g_trigram_count; i++) {
        if (g_trigram_edges[i].src_pair == src_pair &&
            g_trigram_edges[i].dst_token == dst_token) {
            g_trigram_edges[i].count++;
            return;
        }
    }
    
    /* Add new edge */
    if (g_trigram_count >= g_trigram_capacity) {
        uint32_t new_cap = (g_trigram_capacity == 0) ? 1024 : g_trigram_capacity * 2;
        g_trigram_edges = realloc(g_trigram_edges, new_cap * sizeof(TrigramEdge));
        g_trigram_capacity = new_cap;
    }
    
    g_trigram_edges[g_trigram_count].src_pair = src_pair;
    g_trigram_edges[g_trigram_count].dst_token = dst_token;
    g_trigram_edges[g_trigram_count].count = 1;
    g_trigram_count++;
}

/* ----------------------------------------------------------------
 * Tokenization
 * ---------------------------------------------------------------- */
static uint32_t tokenize(const char *text, uint32_t *tokens, uint32_t max_tokens)
{
    char buffer[MAX_LINE_LEN];
    strncpy(buffer, text, MAX_LINE_LEN - 1);
    buffer[MAX_LINE_LEN - 1] = '\0';
    
    uint32_t count = 0;
    char *token = strtok(buffer, " \t\n");
    
    while (token && count < max_tokens) {
        /* Lowercase */
        for (char *p = token; *p; p++) *p = tolower(*p);
        
        int id = base_vocab_get_or_add(token);
        if (id >= 0) {
            tokens[count++] = (uint32_t)id;
        }
        token = strtok(NULL, " \t\n");
    }
    
    return count;
}

/* ----------------------------------------------------------------
 * Save Vocabularies
 * ---------------------------------------------------------------- */
static int save_vocab(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    
    /* Save base tokens */
    for (uint32_t i = 0; i < g_base_vocab.size; i++) {
        fprintf(fp, "%s\n", g_base_vocab.tokens[i]);
    }
    
    fclose(fp);
    return 0;
}

static int save_pair_map(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    
    /* Save pair mappings for inference */
    fprintf(fp, "# Pair ID -> (Token A, Token B)\n");
    for (uint32_t i = 0; i < g_pair_vocab.size; i++) {
        uint32_t a = g_pair_vocab.pairs[i].token_a;
        uint32_t b = g_pair_vocab.pairs[i].token_b;
        fprintf(fp, "%u %u %u\n", i, a, b);
    }
    
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Main Training
 * ---------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <input.txt> <output.ehdag> <vocab.txt>\n", argv[0]);
        return 1;
    }
    
    const char *input_path = argv[1];
    const char *output_path = argv[2];
    const char *vocab_path = argv[3];
    
    printf("\n=== Trigram Training Mode ===\n\n");
    printf("Input:  %s\n", input_path);
    printf("Output: %s\n", output_path);
    printf("Vocab:  %s\n\n", vocab_path);
    
    /* ---- Pass 1: Build base vocabulary ---- */
    printf("Pass 1: Building base vocabulary...\n");
    
    FILE *fp = fopen(input_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open input file\n");
        return 1;
    }
    
    char line[MAX_LINE_LEN];
    uint32_t tokens[1024];
    uint32_t total_tokens = 0;
    uint32_t total_lines = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize(line, tokens, 1024);
        if (n > 0) {
            build_co_occurrence(tokens, n);
            total_tokens += n;
            total_lines++;
        }
    }
    rewind(fp);
    
    printf("  Lines:       %u\n", total_lines);
    printf("  Tokens:      %u\n", total_tokens);
    printf("  Base Vocab:  %u\n\n", g_base_vocab.size);
    
    /* Generate semantic embeddings from co-occurrence */
    generate_semantic_embeddings();
    printf("\n");
    
    /* ---- Pass 2: Build trigrams ---- */
    printf("Pass 2: Building trigram pairs and edges...\n");
    
    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize(line, tokens, 1024);
        
        /* Build trigrams: (token[i], token[i+1]) → token[i+2] */
        for (uint32_t i = 0; i + 2 < n; i++) {
            uint32_t tok_a = tokens[i];
            uint32_t tok_b = tokens[i + 1];
            uint32_t tok_c = tokens[i + 2];
            
            /* Get or create pair (tok_a, tok_b) */
            int pair_id = pair_vocab_get_or_add(tok_a, tok_b);
            if (pair_id < 0) continue;
            
            /* Add edge: pair → tok_c */
            add_trigram_edge((uint32_t)pair_id, tok_c);
        }
    }
    fclose(fp);
    
    printf("  Pair Vocab:  %u (from %u² possible)\n", 
           g_pair_vocab.size, g_base_vocab.size);
    printf("  Trigrams:    %u edges\n", g_trigram_count);
    printf("  Sparsity:    %.1f%% (pairs used)\n\n",
           (100.0f * g_pair_vocab.size) / (g_base_vocab.size * g_base_vocab.size));
    
    /* ---- Pass 3: Build HGN graph ---- */
    printf("Pass 3: Building HGN graph...\n");
    
    /* Graph vocabulary = pair vocabulary */
    EH_HGN_Builder *builder = eh_hgn_builder_create(g_pair_vocab.size, 16);
    if (!builder) {
        fprintf(stderr, "Error: Failed to create builder\n");
        return 1;
    }
    
    /* Set pair embeddings */
    for (uint32_t i = 0; i < g_pair_vocab.size; i++) {
        eh_hgn_builder_set_embedding(builder, i, g_pair_vocab.embeddings[i]);
    }
    
    /* Add edges: pair → next token
     * But wait! dst must also be a pair ID. So we need to map:
     * Edge: pair(A,B) → pair(B,C) when we see (A,B,C)
     */
    uint32_t edge_count = 0;
    /* First pass: compute fanout per source pair for normalization */
    uint32_t *pair_fanout = calloc(g_pair_vocab.size, sizeof(uint32_t));
    if (!pair_fanout) {
        fprintf(stderr, "Error: OOM for fanout table\n");
        return 1;
    }
    for (uint32_t i = 0; i < g_trigram_count; i++) {
        uint32_t sp = g_trigram_edges[i].src_pair;
        if (sp < g_pair_vocab.size) pair_fanout[sp]++;
    }
    
    for (uint32_t i = 0; i < g_trigram_count; i++) {
        uint32_t src_pair_id = g_trigram_edges[i].src_pair;
        uint32_t tok_c = g_trigram_edges[i].dst_token;
        
        /* Source pair is (tok_a, tok_b) */
        uint32_t tok_b = g_pair_vocab.pairs[src_pair_id].token_b;
        
        /* Destination pair is (tok_b, tok_c) */
        int dst_pair_id = pair_vocab_find(tok_b, tok_c);
        if (dst_pair_id < 0) {
            /* Create destination pair if it doesn't exist */
            dst_pair_id = pair_vocab_add(tok_b, tok_c);
        }
        if (dst_pair_id < 0) continue;
        
        /* Edge weight: combine embeddings */
        float weight[EH_HGN_EMBED_DIM];
        for (int j = 0; j < EH_HGN_EMBED_DIM; j++) {
            weight[j] = (g_pair_vocab.embeddings[src_pair_id][j] +
                        g_pair_vocab.embeddings[dst_pair_id][j]) * 0.5f;
        }
        
        /* Prior = log(1 + count) normalized by fanout.
         * Normalization prevents high-fanout pairs (like (what,is)) from
         * creating runaway scores that bleed into wrong topics.
         * Cap count at 5 to prevent hyper-reinforcement.
         */
        uint32_t capped_count = g_trigram_edges[i].count;
        if (capped_count > 5) capped_count = 5;
        float raw_prior  = logf(1.0f + (float)capped_count);
        uint32_t fanout  = pair_fanout[src_pair_id];
        float norm       = (fanout > 1) ? logf(1.0f + (float)fanout) : 1.0f;
        float prior      = raw_prior / norm;
        
        EH_HGN_BuilderStatus rc = eh_hgn_builder_add_edge(
            builder, src_pair_id, (uint32_t)dst_pair_id, prior, weight);
        
        if (rc == EH_HGN_BUILDER_OK) edge_count++;
    }
    free(pair_fanout);
    
    printf("  Graph edges: %u\n\n", edge_count);
    
    /* ---- Pass 4: Finalize and save ---- */
    printf("Pass 4: Finalizing and saving...\n");
    
    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);  /* Larger arena for trigrams */
    EH_HGN_BaseDag dag;
    
    EH_HGN_BuilderStatus rc = eh_hgn_builder_finalize(builder, arena, &dag);
    if (rc != EH_HGN_BUILDER_OK) {
        fprintf(stderr, "Error: Finalize failed\n");
        return 1;
    }
    
    EH_HGN_IO_Status io_rc = eh_hgn_io_save(&dag, output_path);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Save failed (%s)\n", eh_hgn_io_strerror(io_rc));
        return 1;
    }
    
    printf("  ✓ Model saved: %s\n", output_path);
    
    if (save_vocab(vocab_path) != 0) {
        fprintf(stderr, "Error: Vocab save failed\n");
        return 1;
    }
    printf("  ✓ Vocab saved: %s\n", vocab_path);
    
    /* Save pair mappings for inference */
    char pair_path[512];
    snprintf(pair_path, sizeof(pair_path), "%s.pairs", vocab_path);
    if (save_pair_map(pair_path) != 0) {
        fprintf(stderr, "Warning: Pair map save failed\n");
    } else {
        printf("  ✓ Pair map saved: %s\n", pair_path);
    }
    
    printf("\n=== Trigram Training Complete ===\n\n");
    printf("Final Stats:\n");
    printf("  Base vocabulary: %u tokens\n", g_base_vocab.size);
    printf("  Pair vocabulary: %u pairs\n", g_pair_vocab.size);
    printf("  Graph edges:     %u\n", edge_count);
    printf("  Model type:      Trigram (2-token context)\n");
    printf("  Model size:      ~%.2f MB\n\n", 
           (float)(g_pair_vocab.size * EH_HGN_EMBED_DIM * sizeof(float) +
                   edge_count * 140) / (1024.0f * 1024.0f));
    
    eh_hgn_builder_destroy(builder);
    eh_arena_destroy(arena);
    free(g_trigram_edges);
    
    /* Cleanup co-occurrence lists */
    for (uint32_t i = 0; i < MAX_BASE_VOCAB; i++) {
        if (g_co_occ[i].indices) free(g_co_occ[i].indices);
        if (g_co_occ[i].weights) free(g_co_occ[i].weights);
    }
    
    return 0;
}
