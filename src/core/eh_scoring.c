/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_scoring.c
 * Description: Implementation of the Scoring Core — a lightweight routing kernel
 *              that uses dot-product multiplication to compute DAG branch priority scores.
 *
 * Design philosophy:
 *   The Scoring Core must be extremely fast because it is called multiple times
 *   at every Beam Search step. Complexity is strictly O(input_dim);
 *   no dynamic allocation is permitted in the hot-path inference loop.
 *
 * Compile: gcc -O3 -std=c99 -Wall -Wextra -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../include/eh_scoring.h"

/* =================================================================
 * PART 1: SCORING CORE INITIALIZATION
 * =================================================================
 *
 * The routing_weights matrix has a flat layout [MAX_NODES x input_dim]:
 *   routing_weights[node_id * input_dim + k] = weight for dimension k
 *   of the routing vector associated with node_id.
 *
 * Weights are initialized with small random values (Xavier-like):
 *   scale = 1.0 / sqrt(input_dim)
 * This prevents dot-product saturation at the start of training
 * when the model has not yet been trained.
 * ================================================================= */
EH_ScoringCore *eh_scoring_init(int input_dim) {
    if (input_dim <= 0) {
        fprintf(stderr, "[EH_SCORING] Error: input_dim must be > 0, got %d\n",
                input_dim);
        return NULL;
    }

    EH_ScoringCore *core = (EH_ScoringCore *)malloc(sizeof(EH_ScoringCore));
    if (!core) {
        fprintf(stderr, "[EH_SCORING] Error: failed to allocate ScoringCore\n");
        return NULL;
    }

    core->input_dim = input_dim;

    /* --- Allocate routing_weights matrix [MAX_NODES x input_dim] --- */
    size_t weight_size = (size_t)EH_MAX_NODES * (size_t)input_dim * sizeof(float);
    core->routing_weights = (float *)malloc(weight_size);
    if (!core->routing_weights) {
        fprintf(stderr, "[EH_SCORING] Error: failed to allocate %zu bytes "
                        "for routing_weights\n", weight_size);
        free(core);
        return NULL;
    }

    /*
     * Initialize weights with a uniform distribution in [-scale, +scale].
     * scale = 1/sqrt(input_dim) keeps dot-product variance stable
     * regardless of input_dim magnitude (analogous to Xavier initialization).
     */
    float scale = 1.0f / sqrtf((float)input_dim);
    int   total = EH_MAX_NODES * input_dim;

    for (int i = 0; i < total; i++) {
        /*
         * Generate a value in [0, 1] and shift it to [-scale, +scale].
         * rand() is not a high-quality PRNG but is sufficient for the
         * initial routing core weight seeding.
         */
        float r = (float)rand() / (float)RAND_MAX; /* [0, 1] */
        core->routing_weights[i] = (2.0f * r - 1.0f) * scale;
    }

#ifdef EH_DEBUG
    printf("[EH_SCORING] ScoringCore initialized: input_dim=%d, "
           "routing memory=%.2f KB\n",
           input_dim, (float)weight_size / 1024.0f);
#endif

    return core;
}

/* =================================================================
 * PART 2: BRANCH PRIORITY SCORE PREDICTION — HOT PATH
 * =================================================================
 *
 * Formula:
 *   score = Σ_{k=0}^{input_dim-1} input_vector[k] * W[node_id][k]
 *         = dot(input_vector, routing_weights[node_id * input_dim])
 *
 * This is an O(input_dim) operation — the fastest possible linear
 * classifier. No allocation and no complex branching inside the
 * inner loop.
 *
 * Optimization with -O3:
 *   The compiler will auto-vectorize this loop with SIMD (SSE/AVX)
 *   if input_dim is sufficiently large and data is properly aligned.
 *   That is why this function is kept minimal with no conditional
 *   branches inside the loop.
 * ================================================================= */
float eh_scoring_predict_branch(const EH_ScoringCore *core,
                                const float          *input_vector,
                                const EH_DAGNode     *target_node) {
    /* --- Safety checks --- */
    if (!core || !input_vector || !target_node) {
        return -1.0f; /* Negative score so Beam Search discards the faulty branch */
    }

    int node_id = target_node->node_id;

    /* Validate node_id is within range */
    if (node_id < 0 || node_id >= EH_MAX_NODES) {
        fprintf(stderr, "[EH_SCORING] Warning: node_id=%d out of range "
                        "[0, %d)\n", node_id, EH_MAX_NODES);
        return -1.0f;
    }

    /*
     * Point to the routing weight row corresponding to node_id.
     * Layout: routing_weights[node_id * input_dim ... (node_id+1) * input_dim - 1]
     */
    const float *weight_row = core->routing_weights +
                              (size_t)node_id * (size_t)core->input_dim;

    int   dim   = core->input_dim;
    float score = 0.0f;

    /*
     * Main dot-product loop.
     * Uses a double-precision accumulator to reduce rounding error for large
     * input_dim, then casts back to float — balancing precision and speed.
     */
    double acc = 0.0;
    for (int k = 0; k < dim; k++) {
        acc += (double)input_vector[k] * (double)weight_row[k];
    }
    score = (float)acc;

    return score;
}

/* =================================================================
 * PART 3: SCORING CORE DEALLOCATION
 * ================================================================= */
void eh_scoring_free(EH_ScoringCore *core) {
    if (!core) return;

    /*
     * routing_weights was allocated separately by malloc() inside
     * eh_scoring_init(), so it must be freed before the struct is freed.
     */
    free(core->routing_weights);
    core->routing_weights = NULL;
    core->input_dim       = 0;

    free(core);
}
