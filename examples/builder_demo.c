/* ================================================================
 * examples/builder_demo.c — HGN Builder API Demo
 *
 * Demonstrates how to build an HGN graph programmatically using
 * the Builder API, then save and load it for inference.
 *
 * Example scenario: Simple language model with 5 tokens
 *   0: <BOS> (begin of sequence)
 *   1: "hello"
 *   2: "world"
 *   3: "friend"
 *   4: <EOS> (end of sequence)
 *
 * Graph structure:
 *   0 → 1 (hello)    prior=0.6
 *   0 → 2 (world)    prior=0.4
 *   1 → 2 (world)    prior=0.7
 *   1 → 3 (friend)   prior=0.3
 *   2 → 4 (EOS)      prior=1.0
 *   3 → 4 (EOS)      prior=1.0
 *
 * Compile from EventHorizon/ root:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/builder_demo.c \
 *       src/hgn/eh_hgn_builder.c src/hgn/eh_hgn_io.c \
 *       src/hgn/eh_hgn_dag.c src/core/eh_arena.c \
 *       -o examples/builder_demo -lm && examples/builder_demo
 * ================================================================ */

#include "hgn/eh_hgn_builder.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define VOCAB_SIZE  5
#define MAX_FANOUT  4
#define OUTPUT_FILE "/tmp/builder_demo.ehdag"

/* Token names for display */
static const char *TOKEN_NAMES[] = {
    "<BOS>", "hello", "world", "friend", "<EOS>"
};

/* ----------------------------------------------------------------
 * Helper: Generate random embedding
 * ---------------------------------------------------------------- */
static void random_embedding(float *vec, int seed)
{
    srand(seed);
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        vec[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;  /* [-1, 1] */
    }
}

/* ----------------------------------------------------------------
 * Helper: Generate context-based weight vector
 * ---------------------------------------------------------------- */
static void context_weight(float *vec, uint32_t src, uint32_t dst)
{
    /* Simple heuristic: weight based on token IDs */
    float base = (float)(src * 100 + dst);
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        vec[i] = sinf(base + (float)i * 0.1f);
    }
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    printf("\n=== HGN Builder Demo ===\n\n");

    /* ----------------------------------------------------------------
     * Step 1: Create Builder
     * ---------------------------------------------------------------- */
    printf("Step 1: Creating builder (vocab=%d, fanout=%d)...\n",
           VOCAB_SIZE, MAX_FANOUT);

    EH_HGN_Builder *builder = eh_hgn_builder_create(VOCAB_SIZE, MAX_FANOUT);
    if (!builder) {
        fprintf(stderr, "Failed to create builder\n");
        return 1;
    }
    printf("  ✓ Builder created\n\n");

    /* ----------------------------------------------------------------
     * Step 2: Set Embeddings
     * ---------------------------------------------------------------- */
    printf("Step 2: Setting node embeddings...\n");

    float emb[EH_HGN_EMBED_DIM];
    for (uint32_t i = 0; i < VOCAB_SIZE; i++) {
        random_embedding(emb, i * 42);
        EH_HGN_BuilderStatus rc = eh_hgn_builder_set_embedding(builder, i, emb);
        if (rc != EH_HGN_BUILDER_OK) {
            fprintf(stderr, "Failed to set embedding %u: %s\n",
                    i, eh_hgn_builder_strerror(rc));
            return 1;
        }
        printf("  ✓ Token %u (%s): embedding set\n", i, TOKEN_NAMES[i]);
    }
    printf("\n");

    /* ----------------------------------------------------------------
     * Step 3: Add Edges
     * ---------------------------------------------------------------- */
    printf("Step 3: Adding edges...\n");

    /* Define edges: {src, dst, prior} */
    struct {
        uint32_t src;
        uint32_t dst;
        float prior;
    } edges[] = {
        {0, 1, 0.6f},  /* BOS → hello */
        {0, 2, 0.4f},  /* BOS → world */
        {1, 2, 0.7f},  /* hello → world */
        {1, 3, 0.3f},  /* hello → friend */
        {2, 4, 1.0f},  /* world → EOS */
        {3, 4, 1.0f},  /* friend → EOS */
    };

    float weight[EH_HGN_EMBED_DIM];
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++) {
        uint32_t src = edges[i].src;
        uint32_t dst = edges[i].dst;
        float prior = edges[i].prior;

        context_weight(weight, src, dst);
        EH_HGN_BuilderStatus rc = eh_hgn_builder_add_edge(builder, src, dst,
                                                           prior, weight);
        if (rc != EH_HGN_BUILDER_OK) {
            fprintf(stderr, "Failed to add edge %u→%u: %s\n",
                    src, dst, eh_hgn_builder_strerror(rc));
            return 1;
        }
        printf("  ✓ Edge %u→%u (%s → %s) prior=%.1f\n",
               src, dst, TOKEN_NAMES[src], TOKEN_NAMES[dst], prior);
    }

    printf("\n  Total edges: %u\n\n", eh_hgn_builder_edge_count(builder));

    /* ----------------------------------------------------------------
     * Step 4: Finalize to BaseDag
     * ---------------------------------------------------------------- */
    printf("Step 4: Finalizing to BaseDag...\n");

    EH_Arena *arena = eh_arena_create(4 * 1024 * 1024);  /* 4MB */
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        return 1;
    }

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    EH_HGN_BuilderStatus rc = eh_hgn_builder_finalize(builder, arena, &dag);
    if (rc != EH_HGN_BUILDER_OK) {
        fprintf(stderr, "Failed to finalize: %s\n",
                eh_hgn_builder_strerror(rc));
        return 1;
    }

    printf("  ✓ BaseDag created\n");
    printf("    vocab_size: %u\n", dag.vocab_size);
    printf("    total_edges: %u\n", dag.total_edges);
    printf("    embed_dim: %u\n", dag.embed_dim);
    printf("    max_fanout: %u\n\n", dag.max_fanout);

    /* ----------------------------------------------------------------
     * Step 5: Save to File
     * ---------------------------------------------------------------- */
    printf("Step 5: Saving to '%s'...\n", OUTPUT_FILE);

    EH_HGN_IO_Status io_rc = eh_hgn_io_save(&dag, OUTPUT_FILE);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Failed to save: %s\n", eh_hgn_io_strerror(io_rc));
        return 1;
    }
    printf("  ✓ Saved successfully\n\n");

    /* ----------------------------------------------------------------
     * Step 6: Validate File
     * ---------------------------------------------------------------- */
    printf("Step 6: Validating saved file...\n");

    io_rc = eh_hgn_io_validate(OUTPUT_FILE);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Validation failed: %s\n", eh_hgn_io_strerror(io_rc));
        return 1;
    }
    printf("  ✓ File is valid\n\n");

    /* ----------------------------------------------------------------
     * Step 7: Load and Verify
     * ---------------------------------------------------------------- */
    printf("Step 7: Loading back and verifying...\n");

    EH_Arena *arena2 = eh_arena_create(4 * 1024 * 1024);
    EH_HGN_BaseDag dag2;
    memset(&dag2, 0, sizeof(dag2));

    io_rc = eh_hgn_io_load(arena2, OUTPUT_FILE, &dag2);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Failed to load: %s\n", eh_hgn_io_strerror(io_rc));
        return 1;
    }

    printf("  ✓ Loaded successfully\n");
    printf("    vocab_size: %u (match: %s)\n",
           dag2.vocab_size,
           dag2.vocab_size == dag.vocab_size ? "YES" : "NO");
    printf("    total_edges: %u (match: %s)\n",
           dag2.total_edges,
           dag2.total_edges == dag.total_edges ? "YES" : "NO");
    printf("\n");

    /* ----------------------------------------------------------------
     * Step 8: Inspect Graph Structure
     * ---------------------------------------------------------------- */
    printf("Step 8: Inspecting graph structure...\n\n");

    for (uint32_t i = 0; i < dag2.vocab_size; i++) {
        uint32_t fanout = eh_hgn_dag_fanout(&dag2, i);
        printf("  Node %u (%s): %u outgoing edge%s\n",
               i, TOKEN_NAMES[i], fanout, fanout == 1 ? "" : "s");

        if (fanout > 0) {
            const EH_HGN_EdgeCompact *begin = eh_hgn_dag_edges_begin(&dag2, i);
            const EH_HGN_EdgeCompact *end = eh_hgn_dag_edges_end(&dag2, i);

            for (const EH_HGN_EdgeCompact *e = begin; e != end; e++) {
                printf("    → %u (%s)  prior=%.1f\n",
                       e->dst, TOKEN_NAMES[e->dst], e->prior);
            }
        }
    }

    printf("\n");

    /* ----------------------------------------------------------------
     * Step 9: Dump Statistics
     * ---------------------------------------------------------------- */
    printf("Step 9: Statistics...\n\n");
    eh_hgn_dag_dump_info(&dag2);

    /* ----------------------------------------------------------------
     * Cleanup
     * ---------------------------------------------------------------- */
    eh_hgn_builder_destroy(builder);
    eh_arena_destroy(arena);
    eh_arena_destroy(arena2);

    printf("\n=== Demo Complete! ===\n");
    printf("\nGenerated file: %s\n", OUTPUT_FILE);
    printf("You can now use this file for inference with eh_hgn_engine.\n\n");

    return 0;
}
