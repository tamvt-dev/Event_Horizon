/* ================================================================
 * examples/hgn_inference_demo.c — HGN Unified Engine Demo
 *
 * Demonstrates full inference pipeline với tất cả 4 layers:
 *   Layer 1: Load CSR DAG từ binary
 *   Layer 2: Collapse gating (auto-adaptive)
 *   Layer 3: Beam search (K=4 beams)
 *   Layer 4: Unified engine (orchestration)
 *
 * Compile từ EventHorizon/:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I include \
 *       examples/hgn_inference_demo.c \
 *       src/hgn/eh_hgn_engine.c \
 *       src/hgn/eh_hgn_collapse.c \
 *       src/hgn/eh_hgn_dag.c \
 *       src/hgn/eh_beam_search.c \
 *       src/core/eh_arena.c \
 *       -o hgn_inference_demo -lm
 *
 * Usage:
 *   ./hgn_inference_demo <graph.ehdag> <prompt_token1> [token2...]
 *
 * Example:
 *   ./hgn_inference_demo model.ehdag 42 17 99
 * ================================================================ */

#include "hgn/eh_hgn_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <graph.ehdag> <prompt_token1> [token2...]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  %s model.ehdag 0 1\n", prog);
    fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *dag_path = argv[1];
    
    /* Parse prompt tokens từ command line */
    uint32_t prompt[64];
    uint32_t prompt_len = 0;
    for (int i = 2; i < argc && i < 66; i++) {
        prompt[prompt_len++] = (uint32_t)atoi(argv[i]);
    }

    fprintf(stderr, "=== HGN Inference Demo ===\n");
    fprintf(stderr, "DAG file     : %s\n", dag_path);
    fprintf(stderr, "Prompt tokens: [");
    for (uint32_t i = 0; i < prompt_len; i++) {
        fprintf(stderr, "%u%s", prompt[i], (i < prompt_len - 1) ? ", " : "");
    }
    fprintf(stderr, "]\n\n");

    /* ---- Step 1: Setup arena (512MB) ---- */
    fprintf(stderr, "[1/5] Creating arena...\n");
    EH_Arena *arena = eh_arena_create(512 * 1024 * 1024);
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return 1;
    }

    /* ---- Step 2: Load DAG (Layer 1) ---- */
    fprintf(stderr, "[2/5] Loading DAG from %s...\n", dag_path);
    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    
    EH_HGN_Status rc = eh_hgn_dag_load(arena, dag_path, &dag);
    if (rc != EH_HGN_OK) {
        fprintf(stderr, "ERROR: Failed to load DAG (status=%d)\n", rc);
        eh_arena_destroy(arena);
        return 1;
    }

    fprintf(stderr, "  Vocab size : %u\n", dag.vocab_size);
    fprintf(stderr, "  Total edges: %u\n", dag.total_edges);
    fprintf(stderr, "  Embed dim  : %u\n", dag.embed_dim);

    /* ---- Step 3: Configure engine ---- */
    fprintf(stderr, "[3/5] Configuring inference engine...\n");
    EH_HGN_EngineConfig config = eh_hgn_default_config();
    config.max_steps = 20;  /* Limit to 20 steps */
    
    fprintf(stderr, "  Collapse gating: %s (threshold=%.2f)\n",
            config.enable_collapse ? "ON" : "OFF", config.collapse_thresh);
    fprintf(stderr, "  Mutant nodes   : %s (entropy_thresh=%.2f)\n",
            config.enable_mutants ? "ON" : "OFF", config.entropy_thresh);
    fprintf(stderr, "  Max steps      : %u\n", config.max_steps);

    /* ---- Step 4: Initialize session (Layers 2,3,4) ---- */
    fprintf(stderr, "[4/5] Initializing inference session...\n");
    EH_HGN_InferenceSession session;
    
    if (eh_hgn_session_init(&session, &dag, arena, prompt, prompt_len, &config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize session\n");
        eh_arena_destroy(arena);
        return 1;
    }

    fprintf(stderr, "  Session ready!\n\n");

    /* ---- Step 5: Generation loop ---- */
    fprintf(stderr, "[5/5] Running autoregressive generation...\n");
    fprintf(stderr, "────────────────────────────────────────\n");

    uint32_t step = 0;
    while (!eh_hgn_session_is_done(&session)) {
        uint32_t active = eh_hgn_session_step(&session);
        
        const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
        if (best) {
            fprintf(stderr, "Step %2u: active=%u, best_score=%.3f, seq_len=%u\n",
                    ++step, active, best->score, best->seq_len);
        }

        if (active == 0) {
            fprintf(stderr, "  → All beams finished (generation complete)\n");
            break;
        }

        if (step >= config.max_steps) {
            fprintf(stderr, "  → Max steps reached\n");
            break;
        }
    }

    fprintf(stderr, "────────────────────────────────────────\n\n");

    /* ---- Results: Print all K beams ---- */
    fprintf(stderr, "=== Generation Results ===\n");
    const EH_HGN_BeamPath *beams = eh_hgn_session_get_beams(&session);
    
    for (uint32_t i = 0; i < EH_BEAM_WIDTH; i++) {
        if (beams[i].seq_len == 0) continue;
        
        fprintf(stderr, "\nBeam %u: score=%.3f, len=%u, finished=%s\n",
                i, beams[i].score, beams[i].seq_len,
                beams[i].is_finished ? "YES" : "NO");
        
        fprintf(stderr, "  Tokens: [");
        for (uint32_t j = 0; j < beams[i].seq_len; j++) {
            fprintf(stderr, "%u%s", beams[i].tokens[j],
                    (j < beams[i].seq_len - 1) ? ", " : "");
        }
        fprintf(stderr, "]\n");
    }

    /* ---- Stats ---- */
    fprintf(stderr, "\n");
    eh_hgn_session_dump_stats(&session);

    /* ---- Cleanup ---- */
    eh_arena_destroy(arena);
    
    fprintf(stderr, "\n=== Demo Complete ===\n");
    return 0;
}
