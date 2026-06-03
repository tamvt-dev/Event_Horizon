/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_scoring.h
 * Description: Scoring Core definition — a lightweight ML kernel that
 *              computes priority probabilities for each DAG branch before
 *              activating the Beam Search.
 */

#ifndef EH_SCORING_H
#define EH_SCORING_H

#include "eh_dag.h"

/* =========================================================
 * CONSTANTS
 * ========================================================= */
#define EH_SCORE_DIM  128   /* Dimensionality for scoring vectors (AVX2-friendly) */

/* =========================================================
 * SCORING CORE STRUCTURE
 *
 * routing_weights is a flat 2D matrix of size:
 *   [EH_MAX_NODES x input_dim]
 * Row i holds the routing weight vector for node i.
 * Score prediction is a simple dot-product: O(input_dim).
 * ========================================================= */
typedef struct {
    int    input_dim;        /* Dimensionality of the input vector          */
    float *routing_weights;  /* Flat matrix [MAX_NODES x input_dim]         */
} EH_ScoringCore;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/*
 * Initialize a Scoring Core with small random weight vectors.
 * input_dim: dimensionality of the engine's input vector.
 * Returns: pointer to the new ScoringCore, or NULL on failure.
 */
EH_ScoringCore *eh_scoring_init(int input_dim);

/*
 * Predict the priority score of a specific branch (node).
 * Computes the dot-product between input_vector and the routing weight
 * row corresponding to target_node->node_id in routing_weights.
 * Returns: raw float score (not normalized) — higher means higher
 *          Beam Search priority for that branch.
 */
float eh_scoring_predict_branch(const EH_ScoringCore *core,
                                const float          *input_vector,
                                const EH_DAGNode     *target_node);

/*
 * Free all resources owned by the Scoring Core.
 */
void eh_scoring_free(EH_ScoringCore *core);

#endif /* EH_SCORING_H */
