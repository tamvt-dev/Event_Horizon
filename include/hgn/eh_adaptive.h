/* ================================================================
 * eh_adaptive.h — HGN Adaptive Mechanisms
 *
 * 1. Collapse Gating: Dynamic computation switching
 *    - HIGH similarity → COLLAPSE to O(N) mean pooling
 *    - LOW similarity  → EXPAND full DAG traversal O(N×fanout)
 *
 * 2. Mutant Nodes: OOD detection & temporary bridging
 *    - Trigger: beam entropy exceeds threshold
 *    - Generate: deterministic perturbation via LCG
 *    - Lifetime: 1 step (auto-cleanup via arena reset)
 * ================================================================ */

#ifndef EH_ADAPTIVE_H
#define EH_ADAPTIVE_H

#include "eh_hgn_dag.h"
#include "eh_beam_search.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define EH_COLLAPSE_THRESHOLD   0.85f   /* cos(h_t, h_{t-1}) > 0.85 → collapse */
#define EH_ENTROPY_THRESHOLD    2.5f    /* H > 2.5 → OOD, spawn mutant */
#define EH_MUTANT_SCALE         0.3f    /* Perturbation range [-0.3, +0.3] */
#define EH_MAX_MUTANTS          16      /* Max concurrent mutants */

/* ----------------------------------------------------------------
 * Collapse State
 * ---------------------------------------------------------------- */
typedef enum {
    EH_STATE_EXPAND,    /* Full DAG traversal */
    EH_STATE_COLLAPSE   /* Mean pooling only */
} EH_CollapseState;

/* ----------------------------------------------------------------
 * Mutant Node: temporary bridge vector
 * ---------------------------------------------------------------- */
typedef struct {
    float    vec[EH_HGN_EMBED_DIM];  /* Perturbed embedding */
    uint32_t token_id;                /* Virtual token ID (>= vocab_size) */
    uint32_t step_counter;            /* Birth step */
    bool     active;                  /* Is this slot in use? */
} EH_MutantNode;

/* ----------------------------------------------------------------
 * Adaptive Context: tracks collapse state & mutants
 * ---------------------------------------------------------------- */
typedef struct {
    /* Collapse gating */
    float           h_prev[EH_HGN_EMBED_DIM];  /* Previous hidden state */
    EH_CollapseState state;                     /* Current state */
    uint32_t        collapse_count;             /* Stats: times collapsed */
    uint32_t        expand_count;               /* Stats: times expanded */
    
    /* Mutant management */
    EH_MutantNode   mutants[EH_MAX_MUTANTS];   /* Mutant pool */
    uint32_t        mutant_count;               /* Active mutants */
    uint32_t        step_counter;               /* Global step counter */
    uint32_t        total_mutants_spawned;      /* Stats */
    
    /* Thresholds */
    float           collapse_threshold;
    float           entropy_threshold;
    float           mutant_scale;
} EH_AdaptiveContext;

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

/* Initialize adaptive context */
void eh_adaptive_init(EH_AdaptiveContext *ctx,
                      float collapse_threshold,
                      float entropy_threshold,
                      float mutant_scale);

/* Collapse Gating Decision:
 * Computes cos(h_t, h_{t-1}) và quyết định COLLAPSE hay EXPAND
 * Returns: EH_STATE_COLLAPSE hoặc EH_STATE_EXPAND
 */
EH_CollapseState eh_adaptive_gate(EH_AdaptiveContext *ctx,
                                   const float        *h_current);

/* Mutant Spawning:
 * Kiểm tra beam entropy, nếu > threshold → spawn mutant
 * Returns: token_id của mutant (>= vocab_size) hoặc UINT32_MAX nếu không spawn
 */
uint32_t eh_adaptive_try_spawn_mutant(EH_AdaptiveContext     *ctx,
                                      const EH_HGN_BeamTracker *tracker,
                                      const float              *h_current);

/* Get mutant embedding by token_id (hoặc NULL nếu không tìm thấy) */
const float *eh_adaptive_get_mutant_vec(const EH_AdaptiveContext *ctx,
                                         uint32_t token_id);

/* Reset mutants (gọi sau mỗi sequence hoặc arena reset) */
void eh_adaptive_reset_mutants(EH_AdaptiveContext *ctx);

/* Print statistics */
void eh_adaptive_dump_stats(const EH_AdaptiveContext *ctx);

/* ----------------------------------------------------------------
 * Internal Helpers (exposed for testing)
 * ---------------------------------------------------------------- */

/* Cosine similarity: cos(a, b) = dot(a,b) / (||a|| × ||b||) */
float eh_adaptive_cosine_similarity(const float *a, const float *b, uint32_t dim);

/* Compute beam entropy: H = -Σ p_i log(p_i) */
float eh_adaptive_beam_entropy(const EH_HGN_BeamTracker *tracker);

/* Linear Congruential Generator (LCG) for deterministic randomness */
uint32_t eh_adaptive_lcg_next(uint32_t seed);

/* Generate perturbation vector using LCG */
void eh_adaptive_generate_perturbation(uint32_t seed,
                                        float scale,
                                        float *out_vec,
                                        uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* EH_ADAPTIVE_H */
