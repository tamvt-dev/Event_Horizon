/* ================================================================
 * examples/inference_trained.c — Test Trained Model Inference
 *
 * Demonstrates inference using a trained HGN model.
 *
 * Usage:
 *   ./inference_trained <model.ehdag> <start_token_id> <max_steps>
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/inference_trained.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o examples/inference_trained -lm
 * ================================================================ */

#include "hgn/eh_hgn_engine.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.ehdag> [start_token] [max_steps]\n",
                argv[0]);
        fprintf(stderr, "  start_token: Initial token ID (default: 0)\n");
        fprintf(stderr, "  max_steps:   Maximum generation steps (default: 20)\n");
        return 1;
    }

    const char *model_path = argv[1];
    uint32_t start_token = (argc > 2) ? atoi(argv[2]) : 0;
    uint32_t max_steps = (argc > 3) ? atoi(argv[3]) : 20;

    printf("\n=== HGN Inference with Trained Model ===\n\n");
    printf("Model:       %s\n", model_path);
    printf("Start token: %u\n", start_token);
    printf("Max steps:   %u\n\n", max_steps);

    /* ----------------------------------------------------------------
     * Load Model
     * ---------------------------------------------------------------- */
    printf("Loading model...\n");

    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);
    if (!arena) {
        fprintf(stderr, "Error: Failed to create arena\n");
        return 1;
    }

    EH_HGN_BaseDag dag;
    EH_HGN_IO_Status io_rc = eh_hgn_io_load(arena, model_path, &dag);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Load failed: %s\n", eh_hgn_io_strerror(io_rc));
        eh_arena_destroy(arena);
        return 1;
    }

    printf("  ✓ Model loaded\n");
    printf("    Vocabulary: %u tokens\n", dag.vocab_size);
    printf("    Edges:      %u\n", dag.total_edges);
    printf("    Embed dim:  %u\n\n", dag.embed_dim);

    /* Validate start token */
    if (start_token >= dag.vocab_size) {
        fprintf(stderr, "Error: start_token %u >= vocab_size %u\n",
                start_token, dag.vocab_size);
        eh_arena_destroy(arena);
        return 1;
    }

    /* ----------------------------------------------------------------
     * Create Inference Session
     * ---------------------------------------------------------------- */
    printf("Creating inference session...\n");

    EH_HGN_EngineConfig config = eh_hgn_default_config();
    config.max_steps = max_steps;

    EH_HGN_InferenceSession session;
    uint32_t prompt[] = {start_token};
    
    int rc = eh_hgn_session_init(&session, &dag, arena, prompt, 1, &config);
    if (rc != 0) {
        fprintf(stderr, "Error: Session initialization failed\n");
        eh_arena_destroy(arena);
        return 1;
    }

    printf("  ✓ Session created\n\n");

    /* ----------------------------------------------------------------
     * Print Initial State
     * ---------------------------------------------------------------- */
    printf("=== Generation Sequence ===\n\n");
    printf("  Start: [%u]\n", start_token);

    /* ----------------------------------------------------------------
     * Generation Loop
     * ---------------------------------------------------------------- */
    uint32_t step = 0;
    while (!eh_hgn_session_is_done(&session) && step < max_steps) {
        uint32_t active = eh_hgn_session_step(&session);
        if (active == 0) break;  /* No active beams */

        step++;

        /* Get best beam */
        const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
        if (!best || best->seq_len == 0) break;

        /* Print current sequence */
        printf("  Step %2u: [", step);
        for (uint32_t i = 0; i < best->seq_len; i++) {
            printf("%u", best->tokens[i]);
            if (i < best->seq_len - 1) printf(", ");
        }
        printf("] (score: %.3f)\n", best->score);
    }

    /* ----------------------------------------------------------------
     * Final Results
     * ---------------------------------------------------------------- */
    printf("\n=== Generation Complete ===\n\n");

    const EH_HGN_BeamPath *final = eh_hgn_session_get_best(&session);
    if (final) {
        printf("Final sequence (%u tokens):\n  ", final->seq_len);
        for (uint32_t i = 0; i < final->seq_len; i++) {
            printf("%u", final->tokens[i]);
            if (i < final->seq_len - 1) printf(" → ");
        }
        printf("\n\nFinal score: %.3f\n", final->score);
        printf("Finished: %s\n", final->is_finished ? "YES" : "NO");
    } else {
        printf("No sequence generated\n");
    }

    /* ----------------------------------------------------------------
     * Statistics
     * ---------------------------------------------------------------- */
    printf("\n=== Statistics ===\n\n");
    eh_hgn_session_dump_stats(&session);

    /* Cleanup */
    eh_arena_destroy(arena);

    printf("\n=== Inference Complete ===\n\n");
    return 0;
}
