/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_dynamic.h
 * Description: Dynamic Collapse mechanism — input-dependent collapse
 *              evaluated at inference time against the current input vector.
 *
 * Key distinction from Static Collapse:
 *
 *   Static (legacy):  is_collapsed = f(W)          -- computed once at setup
 *   Dynamic (new):    collapse_now = f(X, W, ctx)  -- computed per inference
 *
 * Same node, same weight matrix W:
 *   - Input A with high context correlation → node collapses → O(N)
 *   - Input B with complex context          → node stays dense → O(N^2)
 *
 * Activation Energy formula (cosine similarity with the representative vector):
 *
 *   E(X, μ_W) = |⟨X, μ_W⟩| / (||X|| · ||μ_W||)   ∈ [0, 1]
 *
 *   Collapse fires when: E < low_threshold  (low correlation)
 *                     or  E > high_threshold (saturated / obvious context)
 *
 * μ_W is the column-wise mean vector of W — computed once at setup,
 * stored in EH_DynamicProfile, and incurs zero cost at runtime.
 */

#ifndef EH_DYNAMIC_H
#define EH_DYNAMIC_H

#include <stdbool.h>
#include "eh_dag.h"

/* =================================================================
 * DEFAULT DYNAMIC COLLAPSE THRESHOLDS
 *
 * "Normal" (dense) zone:  low_threshold < E < high_threshold
 * Collapse zone:          E <= low_threshold  (insufficient correlation)
 *                      or E >= high_threshold (saturated / trivially obvious)
 * ================================================================= */
#define EH_DYNAMIC_LOW_THRESHOLD  0.10f  /* Below this: uncorrelated context  */
#define EH_DYNAMIC_HIGH_THRESHOLD 0.92f  /* Above this: saturated context      */

/* =================================================================
 * PER-NODE DYNAMIC PROFILE
 *
 * Computed once at setup (AOT). Stored so that the hot path only needs
 * to read and compute a single O(dim) dot-product without accessing W.
 * ================================================================= */
typedef struct {
    int    node_id;   /* Matches EH_DAGNode.node_id                         */
    float *mu_W;      /* Column-wise mean vector of W, size [cols]           */
    float  mu_norm;   /* ||μ_W|| — pre-computed to avoid sqrt() at runtime  */
    int    dim;       /* Dimensionality of μ_W (= W.cols)                    */
} EH_DynamicProfile;

/* =================================================================
 * DYNAMIC CONTEXT — MANAGES ALL NODE PROFILES
 * ================================================================= */
typedef struct {
    EH_DynamicProfile *profiles;    /* Profile array indexed by node_id */
    int                count;       /* Number of profiles created       */
    float              low_thresh;  /* Low threshold (overrides default) */
    float              high_thresh; /* High threshold (overrides default) */
} EH_DynamicContext;

/* =================================================================
 * DYNAMIC COLLAPSE EVALUATION RESULT
 * Carries enough information for the engine to log and benchmark.
 * ================================================================= */
typedef struct {
    bool  should_collapse;    /* Whether this node should collapse for the current input */
    float activation_energy;  /* Computed value of E(X, μ_W)                            */
    int   reason;             /* 0=dense, 1=low_corr, 2=saturation                      */
} EH_CollapseDecision;

/* =================================================================
 * PUBLIC API
 * ================================================================= */

/*
 * Build a Dynamic Profile for a single node from its weight matrix W.
 * Computes μ_W (column mean) and ||μ_W|| — executed at setup, not inference.
 * Returns: fully populated profile, or an empty profile on failure.
 */
EH_DynamicProfile eh_dynamic_build_profile(const EH_DAGNode *node);

/*
 * Initialize a DynamicContext by traversing the full DAG and building profiles.
 * Returns: new context, or NULL on failure.
 */
EH_DynamicContext *eh_dynamic_init(EH_DAGNode *root,
                                   float       low_thresh,
                                   float       high_thresh);

/*
 * HOT PATH — Evaluate whether this node should collapse for the given input_vector.
 *
 * Complexity: O(dim) — a single dot-product and two comparisons.
 * No allocation, no complex branching.
 *
 * input_norm: ||X|| pre-computed by the caller once per inference pass,
 *             avoiding repeated sqrt() calls across all nodes in one pass.
 */
EH_CollapseDecision eh_dynamic_evaluate(const EH_DynamicContext *dctx,
                                         const float             *input_vector,
                                         float                    input_norm,
                                         const EH_DAGNode        *node);

/*
 * Free the entire DynamicContext and all profiles within it.
 */
void eh_dynamic_free(EH_DynamicContext *dctx);

#endif /* EH_DYNAMIC_H */