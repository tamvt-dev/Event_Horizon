/* ================================================================
 * examples/train_cooccur.c — Train HGN from Text Corpus
 *
 * Trains an HGN graph using co-occurrence statistics from a text
 * corpus. This is a simple but effective training method that doesn't
 * require gradient computation.
 *
 * Algorithm:
 *   1. Load text corpus and tokenize
 *   2. Build vocabulary (unique tokens)
 *   3. Count token co-occurrences (bigrams)
 *   4. Compute edge priors from counts
 *   5. Generate embeddings (random or identity)
 *   6. Generate edge weights from context
 *   7. Build graph using Builder API
 *   8. Save to EHDAG file
 *
 * Usage:
 *   ./train_cooccur <input.txt> <output.ehdag> [vocab_size] [max_fanout]
 *
 * Example:
 *   ./train_cooccur corpus.txt model.ehdag 1000 32
 *
 * Compile from EventHorizon/ root:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/train_cooccur.c \
 *       src/hgn/eh_hgn_builder.c src/hgn/eh_hgn_io.c \
 *       src/hgn/eh_hgn_dag.c src/core/eh_arena.c \
 *       -o examples/train_cooccur -lm
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
#include <time.h>

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */
#define DEFAULT_VOCAB_SIZE  256    /* Default vocabulary size */
#define DEFAULT_MAX_FANOUT  16     /* Default max edges per node */
#define MAX_TOKEN_LEN       64     /* Max characters per token */
#define MAX_TOKENS_PER_LINE 1024   /* Max tokens per line */

/* ----------------------------------------------------------------
 * Data Structures
 * ---------------------------------------------------------------- */

/* Token vocabulary */
typedef struct {
    char **tokens;          /* Token strings */
    uint32_t *counts;       /* Token frequency counts */
    uint32_t size;          /* Current vocab size */
    uint32_t capacity;      /* Allocated capacity */
} Vocabulary;

/* Edge co-occurrence */
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t count;         /* Co-occurrence count */
    float *context_sum;     /* Sum of context vectors */
} EdgeStat;

/* Training statistics */
typedef struct {
    uint32_t total_tokens;
    uint32_t unique_tokens;
    uint32_t total_bigrams;
    uint32_t unique_edges;
} TrainStats;

/* ----------------------------------------------------------------
 * Vocabulary Functions
 * ---------------------------------------------------------------- */

static Vocabulary *vocab_create(uint32_t capacity)
{
    Vocabulary *v = calloc(1, sizeof(Vocabulary));
    if (!v) return NULL;

    v->tokens = calloc(capacity, sizeof(char*));
    v->counts = calloc(capacity, sizeof(uint32_t));
    v->capacity = capacity;
    v->size = 0;

    if (!v->tokens || !v->counts) {
        free(v->tokens);
        free(v->counts);
        free(v);
        return NULL;
    }

    return v;
}

static void vocab_destroy(Vocabulary *v)
{
    if (!v) return;
    for (uint32_t i = 0; i < v->size; i++) {
        free(v->tokens[i]);
    }
    free(v->tokens);
    free(v->counts);
    free(v);
}

/* Find token ID, return -1 if not found */
static int vocab_find(const Vocabulary *v, const char *token)
{
    for (uint32_t i = 0; i < v->size; i++) {
        if (strcmp(v->tokens[i], token) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Add token to vocabulary, return ID */
static int vocab_add(Vocabulary *v, const char *token)
{
    if (v->size >= v->capacity) {
        return -1;  /* Vocabulary full */
    }

    size_t len = strlen(token);
    v->tokens[v->size] = malloc(len + 1);
    if (!v->tokens[v->size]) return -1;
    strcpy(v->tokens[v->size], token);

    v->counts[v->size] = 1;
    return (int)(v->size++);
}

/* Get or add token */
static int vocab_get_or_add(Vocabulary *v, const char *token)
{
    int id = vocab_find(v, token);
    if (id >= 0) {
        v->counts[id]++;
        return id;
    }
    return vocab_add(v, token);
}

/* ----------------------------------------------------------------
 * Edge Statistics
 * ---------------------------------------------------------------- */

static EdgeStat *edge_stats = NULL;
static uint32_t edge_count = 0;
static uint32_t edge_capacity = 0;

static void edge_stats_init(uint32_t capacity)
{
    edge_stats = calloc(capacity, sizeof(EdgeStat));
    edge_capacity = capacity;
    edge_count = 0;
}

static void edge_stats_destroy(void)
{
    if (!edge_stats) return;
    for (uint32_t i = 0; i < edge_count; i++) {
        free(edge_stats[i].context_sum);
    }
    free(edge_stats);
    edge_stats = NULL;
}

static EdgeStat *edge_find(uint32_t src, uint32_t dst)
{
    for (uint32_t i = 0; i < edge_count; i++) {
        if (edge_stats[i].src == src && edge_stats[i].dst == dst) {
            return &edge_stats[i];
        }
    }
    return NULL;
}

static EdgeStat *edge_add(uint32_t src, uint32_t dst)
{
    if (edge_count >= edge_capacity) return NULL;

    EdgeStat *e = &edge_stats[edge_count++];
    e->src = src;
    e->dst = dst;
    e->count = 1;
    e->context_sum = calloc(EH_HGN_EMBED_DIM, sizeof(float));
    return e;
}

static void edge_record(uint32_t src, uint32_t dst, const float *context)
{
    EdgeStat *e = edge_find(src, dst);
    if (!e) {
        e = edge_add(src, dst);
        if (!e) return;  /* Capacity exceeded */
    } else {
        e->count++;
    }

    /* Accumulate context vector */
    if (e->context_sum && context) {
        for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
            e->context_sum[i] += context[i];
        }
    }
}

/* ----------------------------------------------------------------
 * Tokenization (simple whitespace-based)
 * ---------------------------------------------------------------- */

static uint32_t tokenize_line(const char *line, char tokens[][MAX_TOKEN_LEN],
                               uint32_t max_tokens)
{
    uint32_t count = 0;
    const char *p = line;

    while (*p && count < max_tokens) {
        /* Skip whitespace */
        while (*p && isspace(*p)) p++;
        if (!*p) break;

        /* Extract token */
        uint32_t len = 0;
        while (*p && !isspace(*p) && len < MAX_TOKEN_LEN - 1) {
            tokens[count][len++] = tolower(*p);
            p++;
        }
        tokens[count][len] = '\0';

        if (len > 0) count++;
    }

    return count;
}

/* ----------------------------------------------------------------
 * Context Vector Generation
 * ---------------------------------------------------------------- */

static void generate_context_vector(float *vec, uint32_t src, uint32_t dst)
{
    /* Simple heuristic: combine token IDs with sine waves */
    float base = (float)(src * 137 + dst * 271);  /* Prime numbers */
    
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        float phase = base + (float)i * 0.1f;
        vec[i] = sinf(phase) * 0.5f + cosf(phase * 0.7f) * 0.5f;
    }
}

/* ----------------------------------------------------------------
 * Embedding Generation (identity-based)
 * ---------------------------------------------------------------- */

static void generate_embedding(float *emb, uint32_t token_id, uint32_t vocab_size)
{
    memset(emb, 0, EH_HGN_EMBED_DIM * sizeof(float));
    
    /* One-hot-like encoding distributed across dimensions */
    uint32_t dims_per_token = EH_HGN_EMBED_DIM / vocab_size;
    if (dims_per_token == 0) dims_per_token = 1;
    
    uint32_t start = (token_id * dims_per_token) % EH_HGN_EMBED_DIM;
    for (uint32_t i = 0; i < dims_per_token && start + i < EH_HGN_EMBED_DIM; i++) {
        emb[start + i] = 1.0f;
    }
    
    /* Normalize */
    float norm = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        norm += emb[i] * emb[i];
    }
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
            emb[i] /= norm;
        }
    }
}

/* ----------------------------------------------------------------
 * Training Pipeline
 * ---------------------------------------------------------------- */

static int train_from_corpus(const char *input_path,
                             const char *output_path,
                             uint32_t vocab_size,
                             uint32_t max_fanout)
{
    printf("\n=== HGN Co-occurrence Training ===\n\n");
    printf("Input:  %s\n", input_path);
    printf("Output: %s\n", output_path);
    printf("Vocab:  %u tokens\n", vocab_size);
    printf("Fanout: %u max edges per node\n\n", max_fanout);

    TrainStats stats = {0};
    time_t start_time = time(NULL);

    /* ----------------------------------------------------------------
     * Step 1: Build vocabulary and count co-occurrences
     * ---------------------------------------------------------------- */
    printf("Step 1: Scanning corpus...\n");

    FILE *fp = fopen(input_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open '%s'\n", input_path);
        return 1;
    }

    Vocabulary *vocab = vocab_create(vocab_size);
    if (!vocab) {
        fprintf(stderr, "Error: Failed to create vocabulary\n");
        fclose(fp);
        return 1;
    }

    /* Estimate edge capacity: vocab_size * max_fanout */
    edge_stats_init(vocab_size * max_fanout);

    char line[4096];
    char tokens[MAX_TOKENS_PER_LINE][MAX_TOKEN_LEN];
    float context[EH_HGN_EMBED_DIM];

    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize_line(line, tokens, MAX_TOKENS_PER_LINE);
        
        for (uint32_t i = 0; i < n; i++) {
            int src_id = vocab_get_or_add(vocab, tokens[i]);
            if (src_id < 0) continue;  /* Vocab full */

            stats.total_tokens++;

            /* Record bigram (src → dst) */
            if (i + 1 < n) {
                int dst_id = vocab_get_or_add(vocab, tokens[i + 1]);
                if (dst_id >= 0) {
                    generate_context_vector(context, src_id, dst_id);
                    edge_record(src_id, dst_id, context);
                    stats.total_bigrams++;
                }
            }
        }
    }

    fclose(fp);

    stats.unique_tokens = vocab->size;
    stats.unique_edges = edge_count;

    printf("  Total tokens:     %u\n", stats.total_tokens);
    printf("  Unique tokens:    %u\n", stats.unique_tokens);
    printf("  Total bigrams:    %u\n", stats.total_bigrams);
    printf("  Unique edges:     %u\n\n", stats.unique_edges);

    if (stats.unique_tokens == 0) {
        fprintf(stderr, "Error: No tokens found in corpus\n");
        vocab_destroy(vocab);
        edge_stats_destroy();
        return 1;
    }

    /* ----------------------------------------------------------------
     * Step 2: Build graph
     * ---------------------------------------------------------------- */
    printf("Step 2: Building graph...\n");

    EH_HGN_Builder *builder = eh_hgn_builder_create(stats.unique_tokens, max_fanout);
    if (!builder) {
        fprintf(stderr, "Error: Failed to create builder\n");
        vocab_destroy(vocab);
        edge_stats_destroy();
        return 1;
    }

    /* Set embeddings */
    float emb[EH_HGN_EMBED_DIM];
    for (uint32_t i = 0; i < stats.unique_tokens; i++) {
        generate_embedding(emb, i, stats.unique_tokens);
        eh_hgn_builder_set_embedding(builder, i, emb);
    }
    printf("  ✓ Embeddings set for %u tokens\n", stats.unique_tokens);

    /* Add edges */
    uint32_t added = 0, skipped = 0;
    float weight[EH_HGN_EMBED_DIM];

    for (uint32_t i = 0; i < edge_count; i++) {
        EdgeStat *e = &edge_stats[i];
        
        /* Compute prior from normalized count */
        float prior = (float)e->count / (float)stats.total_bigrams;
        
        /* Average context vector */
        for (int j = 0; j < EH_HGN_EMBED_DIM; j++) {
            weight[j] = e->context_sum[j] / (float)e->count;
        }
        
        EH_HGN_BuilderStatus rc = eh_hgn_builder_add_edge(builder, e->src, e->dst,
                                                           prior, weight);
        if (rc == EH_HGN_BUILDER_OK) {
            added++;
        } else {
            skipped++;
            if (rc == EH_HGN_BUILDER_ERR_FANOUT && skipped == 1) {
                printf("  (Note: Some edges skipped due to fanout limit)\n");
            }
        }
    }

    printf("  ✓ Edges added: %u (skipped: %u)\n\n", added, skipped);

    /* ----------------------------------------------------------------
     * Step 3: Finalize and save
     * ---------------------------------------------------------------- */
    printf("Step 3: Finalizing and saving...\n");

    size_t arena_size = 64 * 1024 * 1024;  /* 64MB */
    EH_Arena *arena = eh_arena_create(arena_size);
    if (!arena) {
        fprintf(stderr, "Error: Failed to create arena\n");
        eh_hgn_builder_destroy(builder);
        vocab_destroy(vocab);
        edge_stats_destroy();
        return 1;
    }

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    EH_HGN_BuilderStatus rc = eh_hgn_builder_finalize(builder, arena, &dag);
    if (rc != EH_HGN_BUILDER_OK) {
        fprintf(stderr, "Error: Finalize failed: %s\n",
                eh_hgn_builder_strerror(rc));
        eh_arena_destroy(arena);
        eh_hgn_builder_destroy(builder);
        vocab_destroy(vocab);
        edge_stats_destroy();
        return 1;
    }

    printf("  ✓ Graph finalized\n");

    EH_HGN_IO_Status io_rc = eh_hgn_io_save(&dag, output_path);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Save failed: %s\n", eh_hgn_io_strerror(io_rc));
        eh_arena_destroy(arena);
        eh_hgn_builder_destroy(builder);
        vocab_destroy(vocab);
        edge_stats_destroy();
        return 1;
    }

    printf("  ✓ Saved to %s\n\n", output_path);

    /* ----------------------------------------------------------------
     * Statistics
     * ---------------------------------------------------------------- */
    time_t end_time = time(NULL);
    double elapsed = difftime(end_time, start_time);

    printf("=== Training Complete ===\n\n");
    printf("Model Statistics:\n");
    printf("  Vocabulary:    %u tokens\n", dag.vocab_size);
    printf("  Edges:         %u\n", dag.total_edges);
    printf("  Embed dim:     %u\n", dag.embed_dim);
    printf("  Max fanout:    %u\n\n", dag.max_fanout);
    
    printf("Training Time:   %.0f seconds\n", elapsed);
    printf("Tokens/sec:      %.0f\n", stats.total_tokens / (elapsed > 0 ? elapsed : 1));
    printf("\nModel ready for inference!\n\n");

    /* Cleanup */
    eh_arena_destroy(arena);
    eh_hgn_builder_destroy(builder);
    vocab_destroy(vocab);
    edge_stats_destroy();

    return 0;
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.txt> <output.ehdag> [vocab_size] [max_fanout]\n",
                argv[0]);
        fprintf(stderr, "\nDefaults:\n");
        fprintf(stderr, "  vocab_size  = %u\n", DEFAULT_VOCAB_SIZE);
        fprintf(stderr, "  max_fanout  = %u\n", DEFAULT_MAX_FANOUT);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    uint32_t vocab_size = (argc > 3) ? atoi(argv[3]) : DEFAULT_VOCAB_SIZE;
    uint32_t max_fanout = (argc > 4) ? atoi(argv[4]) : DEFAULT_MAX_FANOUT;

    /* Validate parameters */
    if (vocab_size == 0 || vocab_size > EH_HGN_VOCAB_SIZE) {
        fprintf(stderr, "Error: vocab_size must be 1-%u\n", EH_HGN_VOCAB_SIZE);
        return 1;
    }

    if (max_fanout == 0 || max_fanout > EH_HGN_MAX_FANOUT) {
        fprintf(stderr, "Error: max_fanout must be 1-%u\n", EH_HGN_MAX_FANOUT);
        return 1;
    }

    return train_from_corpus(input_path, output_path, vocab_size, max_fanout);
}
