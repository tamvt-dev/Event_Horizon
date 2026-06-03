/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_dynamic.c
 * Description: Implementation of the Dynamic Collapse evaluation mechanism
 *              based on input context correlation (Cosine Similarity).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../include/core/eh_dynamic.h"

/* --- Internal: DFS graph traversal to collect nodes and build profiles --- */
static void _eh_dynamic_collect_nodes(EH_DAGNode *node, EH_DynamicProfile *profiles, int *count) {
    if (!node || node->visited) return;
    node->visited = true;

    /* Direct profile mapping via node_id for O(1) lookup */
    if (node->node_id >= 0 && node->node_id < EH_MAX_NODES) {
        profiles[node->node_id] = eh_dynamic_build_profile(node);
        if (profiles[node->node_id].node_id != -1) {
            (*count)++;
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        _eh_dynamic_collect_nodes(node->children[i], profiles, count);
    }
}

/* --- Internal: Reset the visited flag across the entire graph --- */
static void _eh_dynamic_reset_visited(EH_DAGNode *node) {
    if (!node || !node->visited) return;
    node->visited = false;
    for (int i = 0; i < node->child_count; i++) {
        _eh_dynamic_reset_visited(node->children[i]);
    }
}

/* ================================================================
 * BUILD PROFILE FOR A SINGLE NODE (AOT COMPILATION PROCESS)
 * ================================================================ */
EH_DynamicProfile eh_dynamic_build_profile(const EH_DAGNode *node) {
    EH_DynamicProfile prof;
    memset(&prof, 0, sizeof(EH_DynamicProfile));

    if (!node || node->weights.cols <= 0 || node->weights.rows <= 0) {
        prof.node_id = -1;
        return prof;
    }

    prof.node_id = node->node_id;
    prof.dim     = node->weights.cols;
    prof.mu_W    = (float *)malloc(sizeof(float) * (size_t)prof.dim);
    if (!prof.mu_W) {
        prof.node_id = -1;
        return prof;
    }

    int    rows     = node->weights.rows;
    int    cols     = node->weights.cols;
    double norm_acc = 0.0;

    /* Compute column-wise mean vector μ_W */
    for (int c = 0; c < cols; c++) {
        double col_sum = 0.0;
        for (int r = 0; r < rows; r++) {
            col_sum += (double)node->weights.data[r * cols + c];
        }
        prof.mu_W[c]  = (float)(col_sum / (double)rows);
        norm_acc     += (double)prof.mu_W[c] * (double)prof.mu_W[c];
    }

    /*
     * Pre-compute and cache the vector norm to eliminate a sqrt() call
     * on the hot-path at runtime.
     */
    prof.mu_norm = (float)sqrt(norm_acc);
    if (prof.mu_norm < 1e-7f) {
        prof.mu_norm = 1e-7f; /* Guard against division-by-zero */
    }

    return prof;
}

/* ================================================================
 * INITIALIZE DYNAMIC CONTEXT FOR THE ENTIRE GRAPH
 * ================================================================ */
EH_DynamicContext *eh_dynamic_init(EH_DAGNode *root, float low_thresh, float high_thresh) {
    if (!root) return NULL;

    EH_DynamicContext *dctx = (EH_DynamicContext *)malloc(sizeof(EH_DynamicContext));
    if (!dctx) return NULL;

    /* Allocate a flat direct-mapped table of size EH_MAX_NODES */
    dctx->profiles = (EH_DynamicProfile *)malloc(sizeof(EH_DynamicProfile) * EH_MAX_NODES);
    if (!dctx->profiles) {
        free(dctx);
        return NULL;
    }

    /* Initialize sentinel values for all slots */
    for (int i = 0; i < EH_MAX_NODES; i++) {
        dctx->profiles[i].node_id  = -1;
        dctx->profiles[i].mu_W     = NULL;
        dctx->profiles[i].mu_norm  = 0.0f;
        dctx->profiles[i].dim      = 0;
    }

    dctx->count       = 0;
    dctx->low_thresh  = low_thresh;
    dctx->high_thresh = high_thresh;

    /* Traverse the graph to build the profile database */
    _eh_dynamic_reset_visited(root);
    _eh_dynamic_collect_nodes(root, dctx->profiles, &dctx->count);
    _eh_dynamic_reset_visited(root);

    return dctx;
}

/* ================================================================
 * HOT PATH INFERENCE — REAL-TIME CONTEXT CORRELATION EVALUATION
 * ================================================================ */
EH_CollapseDecision eh_dynamic_evaluate(const EH_DynamicContext *dctx,
                                        const float             *input_vector,
                                        float                    input_norm,
                                        const EH_DAGNode        *node) {
    EH_CollapseDecision decision;
    decision.should_collapse   = false;
    decision.activation_energy = 0.0f;
    decision.reason            = 0; /* Default: retain dense matrix */

    if (!dctx || !input_vector || !node || node->node_id < 0 || node->node_id >= EH_MAX_NODES) {
        return decision;
    }

    /* O(1) direct lookup via node_id */
    const EH_DynamicProfile *prof = &dctx->profiles[node->node_id];
    if (prof->node_id == -1 || !prof->mu_W) {
        return decision;
    }

    int    dim     = prof->dim;
    double dot_acc = 0.0;

    /* Hint the compiler to auto-vectorize (SIMD via AVX/SSE registers) */
    #pragma GCC ivdep
    for (int i = 0; i < dim; i++) {
        dot_acc += (double)input_vector[i] * (double)prof->mu_W[i];
    }

    /* Compute activation energy: actual Cosine Similarity */
    float denominator = input_norm * prof->mu_norm;
    float energy      = 0.0f;
    if (denominator > 1e-7f) {
        energy = (float)(fabs(dot_acc) / (double)denominator);
    }

    if (energy > 1.0f) energy = 1.0f; /* Handle floating-point rounding artifacts */
    decision.activation_energy = energy;

    /* Evaluate collapse logic against the two distribution thresholds */
    if (energy <= dctx->low_thresh) {
        decision.should_collapse = true;
        decision.reason          = 1; /* Low correlation context */
    } else if (energy >= dctx->high_thresh) {
        decision.should_collapse = true;
        decision.reason          = 2; /* Saturated / obvious context */
    }

    return decision;
}

/* ================================================================
 * RELEASE DYNAMIC CONTEXT RESOURCES
 * ================================================================ */
void eh_dynamic_free(EH_DynamicContext *dctx) {
    if (!dctx) return;
    if (dctx->profiles) {
        for (int i = 0; i < EH_MAX_NODES; i++) {
            if (dctx->profiles[i].mu_W) {
                free(dctx->profiles[i].mu_W);
            }
        }
        free(dctx->profiles);
    }
    free(dctx);
}