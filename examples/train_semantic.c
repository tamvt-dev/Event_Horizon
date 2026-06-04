/* ================================================================
 * examples/train_semantic.c — Train HGN with Semantic Embeddings
 *
 * Improves upon train_cooccur.c by generating semantic embeddings
 * from co-occurrence statistics instead of random sin/cos.
 *
 * Algorithm:
 *   1. Build vocabulary from corpus
 *   2. Compute token co-occurrence matrix (within window of N tokens)
 *   3. Apply dimensionality reduction (simple averaging + normalization)
 *   4. Use semantic embeddings for graph construction
 *   5. Build edges from bigram statistics
 *   6. Save enhanced model
 *
 * Usage:
 *   ./train_semantic <input.txt> <output.ehdag> <vocab.txt>
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/train_semantic.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o train_semantic -lm
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

#define MAX_VOCAB_SIZE    2000
#define MAX_TOKEN_LEN     32
#define MAX_LINE_LEN      256
#define CO_OCC_WINDOW     5      /* Window for semantic co-occurrence */

/* ----------------------------------------------------------------
 * Vocabulary
 * ---------------------------------------------------------------- */
typedef struct {
    char tokens[MAX_VOCAB_SIZE][MAX_TOKEN_LEN];
    uint32_t counts[MAX_VOCAB_SIZE];    /* Token frequencies */
    float embeddings[MAX_VOCAB_SIZE][EH_HGN_EMBED_DIM];  /* Semantic embeddings */
    uint32_t size;
} Vocabulary;

static Vocabulary g_vocab = {0};

/* Co-occurrence matrix (sparse representation) */
typedef struct {
    uint32_t *indices;   /* Co-occurring token IDs */
    float *weights;      /* Co-occurrence weights */
    uint32_t count;
    uint32_t capacity;
} CoOccList;

static CoOccList g_co_occ[MAX_VOCAB_SIZE] = {0};

/* ----------------------------------------------------------------
 * Vocabulary Functions
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

static int vocab_add(const char *token)
{
    if (g_vocab.size >= MAX_VOCAB_SIZE) return -1;
    
    strncpy(g_vocab.tokens[g_vocab.size], token, MAX_TOKEN_LEN - 1);
    g_vocab.tokens[g_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
    g_vocab.counts[g_vocab.size] = 0;
    
    return g_vocab.size++;
}

static int vocab_get_or_add(const char *token)
{
    int id = vocab_find(token);
    if (id >= 0) {
        g_vocab.counts[id]++;
        return id;
    }
    
    int new_id = vocab_add(token);
    if (new_id >= 0) {
        g_vocab.counts[new_id] = 1;
    }
    return new_id;
}

static int vocab_save(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        fprintf(fp, "%s\n", g_vocab.tokens[i]);
    }
    
    fclose(fp);
    return 0;
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
        /* Convert to lowercase */
        for (char *p = token; *p; p++) *p = tolower(*p);
        
        int id = vocab_get_or_add(token);
        if (id >= 0) {
            tokens[count++] = (uint32_t)id;
        }
        token = strtok(NULL, " \t\n");
    }
    
    return count;
}

/* ----------------------------------------------------------------
 * Co-occurrence Recording
 * ---------------------------------------------------------------- */
static void add_co_occurrence(uint32_t token_a, uint32_t token_b, float weight)
{
    if (token_a >= MAX_VOCAB_SIZE) return;
    
    CoOccList *list = &g_co_occ[token_a];
    
    /* Check if already exists */
    for (uint32_t i = 0; i < list->count; i++) {
        if (list->indices[i] == token_b) {
            list->weights[i] += weight;
            return;
        }
    }
    
    /* Add new entry */
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

/* ----------------------------------------------------------------
 * Build Co-occurrence Matrix from Corpus
 * ---------------------------------------------------------------- */
static void build_co_occurrence(const uint32_t *tokens, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t center = tokens[i];
        
        /* Look at tokens within window */
        for (uint32_t j = 1; j <= CO_OCC_WINDOW && i + j < n; j++) {
            uint32_t context = tokens[i + j];
            
            /* Weight decays with distance */
            float weight = 1.0f / (float)j;
            
            /* Symmetric: both directions */
            add_co_occurrence(center, context, weight);
            add_co_occurrence(context, center, weight);
        }
    }
}

/* ----------------------------------------------------------------
 * Generate Semantic Embeddings from Co-occurrence
 *
 * Simple approach: Each embedding dimension represents affinity
 * to a "basis" token. We use the most frequent tokens as basis.
 * ---------------------------------------------------------------- */
static void generate_semantic_embeddings(void)
{
    printf("Generating semantic embeddings...\n");
    
    /* Use vocabulary embeddings: each token's embedding is influenced
     * by tokens it co-occurs with, weighted by frequency */
    
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        /* Initialize to zero */
        for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
            g_vocab.embeddings[i][d] = 0.0f;
        }
        
        /* Add contributions from co-occurring tokens */
        CoOccList *list = &g_co_occ[i];
        
        for (uint32_t j = 0; j < list->count && j < EH_HGN_EMBED_DIM; j++) {
            uint32_t co_token = list->indices[j];
            float co_weight = list->weights[j];
            
            /* Simple encoding: dimension j gets contribution from j-th co-occurring token */
            /* Use token ID as basis for now (simple but captures patterns) */
            float basis_value = sinf((float)co_token * 0.1f + (float)j * 0.05f);
            
            /* Weight by co-occurrence strength */
            int target_dim = (int)(j % EH_HGN_EMBED_DIM);
            g_vocab.embeddings[i][target_dim] += basis_value * co_weight;
        }
        
        /* Normalize embedding to unit length */
        float norm = 0.0f;
        for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
            norm += g_vocab.embeddings[i][d] * g_vocab.embeddings[i][d];
        }
        norm = sqrtf(norm);
        
        if (norm > 1e-6f) {
            for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
                g_vocab.embeddings[i][d] /= norm;
            }
        } else {
            /* Fallback for rare tokens: random but consistent */
            for (int d = 0; d < EH_HGN_EMBED_DIM; d++) {
                g_vocab.embeddings[i][d] = sinf((float)i * 0.1f + (float)d * 0.01f);
            }
        }
    }
    
    printf("  ✓ Generated %u semantic embeddings\n", g_vocab.size);
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
    
    printf("\n=== Semantic Training Mode ===\n\n");
    printf("Input:  %s\n", input_path);
    printf("Output: %s\n", output_path);
    printf("Vocab:  %s\n\n", vocab_path);
    
    /* ---- Pass 1: Load corpus and build vocabulary ---- */
    FILE *fp = fopen(input_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open input file\n");
        return 1;
    }
    
    printf("Pass 1: Building vocabulary and co-occurrence...\n");
    
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
    fclose(fp);
    
    printf("  Lines:  %u\n", total_lines);
    printf("  Tokens: %u\n", total_tokens);
    printf("  Vocab:  %u\n\n", g_vocab.size);
    
    /* ---- Pass 2: Generate semantic embeddings ---- */
    generate_semantic_embeddings();
    printf("\n");
    
    /* ---- Pass 3: Build graph edges ---- */
    printf("Pass 2: Building graph edges...\n");
    
    EH_HGN_Builder *builder = eh_hgn_builder_create(g_vocab.size, 16);
    if (!builder) {
        fprintf(stderr, "Error: Failed to create builder\n");
        return 1;
    }
    
    /* Set semantic embeddings */
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        eh_hgn_builder_set_embedding(builder, i, g_vocab.embeddings[i]);
    }
    
    /* Add edges from bigram statistics */
    rewind(fp = fopen(input_path, "r"));
    uint32_t edge_count = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize(line, tokens, 1024);
        
        for (uint32_t i = 0; i < n - 1; i++) {
            uint32_t src = tokens[i];
            uint32_t dst = tokens[i + 1];
            
            /* Edge weight: use semantic similarity as context hint */
            float weight[EH_HGN_EMBED_DIM];
            for (int j = 0; j < EH_HGN_EMBED_DIM; j++) {
                weight[j] = (g_vocab.embeddings[src][j] + g_vocab.embeddings[dst][j]) * 0.5f;
            }
            
            EH_HGN_BuilderStatus rc = eh_hgn_builder_add_edge(builder, src, dst, 
                                                               0.5f, weight);
            if (rc == EH_HGN_BUILDER_OK) edge_count++;
        }
    }
    fclose(fp);
    
    printf("  Edges: %u\n\n", edge_count);
    
    /* ---- Pass 4: Finalize and save ---- */
    printf("Pass 3: Finalizing and saving...\n");
    
    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);
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
    
    if (vocab_save(vocab_path) != 0) {
        fprintf(stderr, "Error: Vocab save failed\n");
        return 1;
    }
    
    printf("  ✓ Vocab saved: %s\n\n", vocab_path);
    
    printf("=== Semantic Training Complete ===\n\n");
    printf("Model Stats:\n");
    printf("  Vocabulary: %u tokens\n", g_vocab.size);
    printf("  Edges:      %u\n", edge_count);
    printf("  Embeddings: Semantic (co-occurrence based)\n\n");
    
    eh_hgn_builder_destroy(builder);
    eh_arena_destroy(arena);
    
    /* Cleanup co-occurrence lists */
    for (uint32_t i = 0; i < MAX_VOCAB_SIZE; i++) {
        if (g_co_occ[i].indices) free(g_co_occ[i].indices);
        if (g_co_occ[i].weights) free(g_co_occ[i].weights);
    }
    
    return 0;
}
