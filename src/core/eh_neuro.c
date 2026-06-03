/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_neuro.c
 * Description: Neuroplasticity mechanism — feedback loop, parametric learning,
 *              and structural mutation (allocating new nodes from Arena).
 *
 * Two types of learning in eh_neuro_process_feedback:
 *
 *   A. Parametric Learning (always happens when error > threshold):
 *      Updates the routing_weights of the ScoringCore along the gradient
 *      to reduce error. Does not change the DAG structure.
 *      -> Analogous to: light backprop only on the routing layer.
 *
 *   B. Structural Mutation (with probability mutation_rate):
 *      Spawns a new MutationNode from the Arena, initializing its weights
 *      using the error residual (error signal instead of random noise).
 *      Attaches it to the parent_node -> DAG gains a new branch.
 *      -> Analogous to: neurogenesis in the hippocampus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../../include/core/eh_neuro.h"

/* =================================================================
 * PART 1: NEURO CONTEXT INITIALIZATION
 * ================================================================= */
EH_NeuroContext *eh_neuro_init(EH_Arena *arena,
                               float     error_threshold,
                               float     learning_rate,
                               float     mutation_rate) {
    if (!arena) {
        fprintf(stderr, "[EH_NEURO] Error: Arena is NULL\n");
        return NULL;
    }

    EH_NeuroContext *nctx = (EH_NeuroContext *)malloc(sizeof(EH_NeuroContext));
    if (!nctx) return NULL;

    memset(nctx, 0, sizeof(EH_NeuroContext));
    nctx->arena           = arena;
    nctx->error_threshold = error_threshold;
    nctx->learning_rate   = learning_rate;
    nctx->mutation_rate   = mutation_rate;
    nctx->next_node_id    = EH_NEURO_MUTANT_ID_BASE;
    nctx->mutant_count    = 0;
    nctx->total_mutations = 0;
    nctx->total_pruned    = 0;

    printf("[EH_NEURO] Initialized: threshold=%.3f, lr=%.4f, mut_rate=%.2f\n",
           error_threshold, learning_rate, mutation_rate);

    return nctx;
}

/* =================================================================
 * INTERNAL FUNCTION: COMPUTE ERROR MAGNITUDE
 * =================================================================
 *
 * Uses Mean Absolute Error (MAE) instead of MSE because:
 * - Less sensitive to outliers than MSE.
 * - Easier to interpret: "average deviation per dimension".
 * - No sqrt() needed -> faster than RMSE.
 * ================================================================= */
static float _compute_mae(const float *output,
                           const float *target,
                           int          dim) {
    double sum = 0.0;
    for (int i = 0; i < dim; i++) {
        sum += fabs((double)output[i] - (double)target[i]);
    }
    return (float)(sum / dim);
}

/* =================================================================
 * INTERNAL FUNCTION A: PARAMETRIC LEARNING
 * =================================================================
 *
 * Updates routing_weights of ScoringCore using a pseudo-gradient:
 *
 *   Δw[node_id][k] = -η * error_sign * input[k]
 *
 * Where:
 *   η = learning_rate
 *   error_sign = sign(error) = +1 if output > target, -1 otherwise.
 *   input[k] = input value at dimension k.
 *
 * This is a simple gradient approximation (no full backprop),
 * equivalent to a SGD step on a linear classifier.
 *
 * Why use error_sign instead of error_magnitude in the update:
 *   Prevents exploding gradients when the error is abnormally large.
 *   Similar to sign SGD (SignSGD) — more robust than vanilla SGD.
 * ================================================================= */
static float _parametric_update(EH_ScoringCore  *scorer,
                                 const EH_DAGNode *parent_node,
                                 const float      *input_vector,
                                 int               input_dim,
                                 float             error,
                                 float             learning_rate) {
    if (!scorer || !scorer->routing_weights || !parent_node || !input_vector) {
        return 0.0f;
    }

    int   node_id = parent_node->node_id;
    if (node_id < 0 || node_id >= EH_MAX_NODES) return 0.0f;

    int    dim        = (input_dim < scorer->input_dim) ? input_dim : scorer->input_dim;
    float *weight_row = scorer->routing_weights + (size_t)node_id * scorer->input_dim;

    /*
     * error_sign: direction of the gradient.
     * If output is greater than target (error > 0) -> decrease weight.
     * If output is less than target (error < 0) -> increase weight.
     */
    float error_sign = (error >= 0.0f) ? 1.0f : -1.0f;
    float max_delta  = 0.0f;

    for (int k = 0; k < dim; k++) {
        float delta     = -learning_rate * error_sign * input_vector[k];
        weight_row[k]  += delta;
        float abs_delta = fabsf(delta);
        if (abs_delta > max_delta) max_delta = abs_delta;
    }

#ifdef EH_DEBUG
    printf("[EH_NEURO] Parametric update node %d: error_sign=%.0f, "
           "max_delta=%.6f\n", node_id, error_sign, max_delta);
#endif

    return max_delta;
}

/* =================================================================
 * INTERNAL FUNCTION B: STRUCTURAL MUTATION — SPAWN NEW NODE
 * =================================================================
 *
 * MutationNode is initialized with weights via Error Residual Seeding:
 *   W_mutant[i][j] = (target[i % out_dim] - output[i % out_dim]) * scale
 *
 * Instead of random initialization, we use the error residual as a "guide"
 * for the new node. Meaning: the mutant node is spawned specifically
 * to "correct" what the current beam path got wrong.
 *
 * scale = learning_rate to prevent overly large weights at the beginning.
 * ================================================================= */
static EH_DAGNode *_create_mutation_node(EH_NeuroContext *nctx,
                                          EH_DAGNode      *parent,
                                          const float     *output_vec,
                                          const float     *target_vec,
                                          int              vec_dim) {
    if (nctx->mutant_count >= EH_NEURO_MAX_MUTANTS) {
        fprintf(stderr, "[EH_NEURO] Reached mutant limit of %d\n",
                EH_NEURO_MAX_MUTANTS);
        return NULL;
    }

    /* Check child count limit before allocating memory */
    if (parent->child_count >= EH_MAX_CHILDREN) {
#ifdef EH_DEBUG
        fprintf(stderr, "[EH_NEURO] Parent node %d is full (%d children), "
                        "cannot attach mutant\n",
                parent->node_id, EH_MAX_CHILDREN);
#endif
        return NULL;
    }

    int new_id = nctx->next_node_id++;
    int rows   = vec_dim;
    int cols   = vec_dim;

    /* Allocate node from Arena — no discrete malloc */
    EH_DAGNode *mutant = eh_arena_alloc_node(nctx->arena, new_id, rows, cols);
    if (!mutant) return NULL;

    /*
     * Error Residual Seeding:
     * Populate weights based on the residual error of the current beam path.
     * Each row i of W_mutant is set according to the corresponding error dimension.
     */
    float scale = nctx->learning_rate;
    for (int i = 0; i < rows; i++) {
        float residual = (target_vec[i % vec_dim] - output_vec[i % vec_dim]);
        for (int j = 0; j < cols; j++) {
            /* Add small noise to each column j to break symmetry */
            float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.001f;
            mutant->weights.data[i * cols + j] = residual * scale + noise;
        }
    }

    /*
     * Compute mutant's distribution_mean = average error residual.
     * Used as the collapse value if the mutant collapses in the future.
     */
    double sum = 0.0;
    for (int i = 0; i < vec_dim; i++) {
        sum += fabs((double)(target_vec[i] - output_vec[i]));
    }
    mutant->distribution_mean = (float)(sum / vec_dim);
    mutant->is_collapsed      = false;

    parent->children[parent->child_count++] = mutant;

    printf("[EH_NEURO] Mutant Node %d spawned: parent=%d, dim=%dx%d, "
           "dist_mean=%.4f\n",
           new_id, parent->node_id, rows, cols, mutant->distribution_mean);

    return mutant;
}

/* =================================================================
 * PART 2: PROCESS FEEDBACK — MAIN FUNCTION
 * ================================================================= */
EH_FeedbackResult eh_neuro_process_feedback(
    EH_NeuroContext *nctx,
    EH_DAGNode      *parent_node,
    const float     *output_vec,
    const float     *target_vec,
    int              vec_dim,
    EH_ScoringCore  *scorer,
    const float     *input_vector,
    int              input_dim) {

    EH_FeedbackResult result = { 0.0f, false, -1, false, 0.0f };

    if (!nctx || !parent_node || !output_vec || !target_vec) return result;

    /* Arena safety gate: if space is running low, do NOT create structural mutants.
     * Prevents giant "OUT OF SPACE" logs from cluttering execution. */
    if (nctx->arena) {
        size_t data_size = (size_t)vec_dim * (size_t)vec_dim * sizeof(float);
        size_t total_est = sizeof(EH_DAGNode) + data_size;
        size_t aligned_est = (total_est + EH_ARENA_ALIGN - 1) & ~(size_t)(EH_ARENA_ALIGN - 1);

        if (nctx->arena->used + aligned_est > nctx->arena->capacity) {
            printf("[EH_NEURO] Mutation SKIP: Arena is nearly full "
                   "(needed~%zu, used=%zu/%zu).\n",
                   aligned_est, nctx->arena->used, nctx->arena->capacity);
            return result;
        }
    }

    /* --- Step 1: Measure error --- */
    float error = _compute_mae(output_vec, target_vec, vec_dim);
    result.error_magnitude = error;

#ifdef EH_DEBUG
    printf("[EH_NEURO] Feedback: MAE=%.4f, threshold=%.4f\n",
           error, nctx->error_threshold);
#endif

    /* Error is within threshold -> no learning needed, beam path is sufficient */
    if (error < nctx->error_threshold) {
#ifdef EH_DEBUG
        printf("[EH_NEURO] Error within threshold — skipping update.\n");
#endif
        return result;
    }

    /* --- Step 2: Parametric Learning --- */
    /*
     * Sign of error: use (mean_output - mean_target) to obtain direction.
     * _compute_mae returns |error|, but we need direction for correct updates.
     */
    double mean_out = 0.0, mean_tgt = 0.0;
    for (int i = 0; i < vec_dim; i++) {
        mean_out += output_vec[i];
        mean_tgt += target_vec[i];
    }
    float signed_error = (float)((mean_out - mean_tgt) / vec_dim);

#ifdef EH_DEBUG
    float w_before = scorer->routing_weights[(size_t)parent_node->node_id * scorer->input_dim];
#endif
    float delta = _parametric_update(scorer, parent_node,
                                      input_vector, input_dim,
                                      signed_error, nctx->learning_rate);
    result.weights_updated = true;
    result.weight_delta    = delta;

#ifdef EH_DEBUG
    printf("[EH_NEURO] Parametric Learning: W_route[%d][0]: %.4f -> %.4f\n",
           parent_node->node_id,
           w_before,
           scorer->routing_weights[(size_t)parent_node->node_id * scorer->input_dim]);
#endif

    /* --- Step 3: Check structural mutation conditions --- */
    float rand_val = (float)rand() / RAND_MAX;
    if (rand_val > nctx->mutation_rate) {
#ifdef EH_DEBUG
        printf("[EH_NEURO] Mutation skipped (rand=%.3f > rate=%.3f)\n",
               rand_val, nctx->mutation_rate);
#endif
        return result;
    }

    /* --- Step 4: Structural Mutation --- */
    EH_DAGNode *mutant = _create_mutation_node(nctx, parent_node,
                                                output_vec, target_vec, vec_dim);
    if (!mutant) return result;

    /* Write MutantRecord to registry */
    int idx = nctx->mutant_count;
    nctx->mutants[idx].node             = mutant;
    nctx->mutants[idx].parent           = parent_node;
    nctx->mutants[idx].generation       = 0;
    nctx->mutants[idx].error_at_birth   = error;
    nctx->mutants[idx].cumulative_error = error;
    nctx->mutants[idx].activation_count = 0;
    nctx->mutants[idx].status           = EH_MUTANT_HEALTHY;
    nctx->mutant_count++;
    nctx->total_mutations++;

    result.mutation_fired = true;
    result.mutant_id      = mutant->node_id;

    return result;
}

/* =================================================================
 * PART 3: PRUNING — REMOVE INEFFICIENT MUTANTS
 * ================================================================= */
int eh_neuro_prune(EH_NeuroContext *nctx, float prune_threshold) {
    if (!nctx) return 0;

    int pruned = 0;
    for (int i = 0; i < nctx->mutant_count; i++) {
        EH_MutantRecord *rec = &nctx->mutants[i];
        if (rec->status != EH_MUTANT_HEALTHY) continue;

        /*
         * Pruning criteria: if activated at least once
         * and cumulative error / activation count > threshold.
         */
        if (rec->activation_count > 0) {
            float avg_error = rec->cumulative_error / rec->activation_count;
            if (avg_error > prune_threshold) {
                rec->status = EH_MUTANT_PRUNED;
                /*
                 * Disconnect from parent by removing from children[].
                 * Find and remove by pointer address.
                 */
                EH_DAGNode *p = rec->parent;
                if (p) {
                    for (int c = 0; c < p->child_count; c++) {
                        if (p->children[c] == rec->node) {
                            /* Move last child to this position */
                            p->children[c] = p->children[p->child_count - 1];
                            p->children[p->child_count - 1] = NULL;
                            p->child_count--;
                            break;
                        }
                    }
                }
                pruned++;
                nctx->total_pruned++;
                printf("[EH_NEURO] Pruned node %d (avg_err=%.4f > %.4f)\n",
                       rec->node->node_id, avg_error, prune_threshold);
            }
        }
    }
    return pruned;
}

/* =================================================================
 * PART 4: UTILITIES — REPORT, DISCONNECT, FREE
 * ================================================================= */
void eh_neuro_report(const EH_NeuroContext *nctx) {
    if (!nctx) return;
    printf("[EH_NEURO] === Mutation Registry ===\n");
    printf("  Total mutations: %d | Total pruned: %d | Live mutants: %d\n",
           nctx->total_mutations, nctx->total_pruned, nctx->mutant_count);
    for (int i = 0; i < nctx->mutant_count; i++) {
        const EH_MutantRecord *r = &nctx->mutants[i];
        const char *status_str[] = { "HEALTHY", "PRUNED", "FROZEN" };
        printf("  [%d] Node=%d, Parent=%d, ErrBirth=%.4f, "
               "Activations=%d, Status=%s\n",
               i, r->node->node_id,
               r->parent ? r->parent->node_id : -1,
               r->error_at_birth, r->activation_count,
               status_str[r->status]);
    }
}

void eh_neuro_disconnect_all(EH_NeuroContext *nctx) {
    if (!nctx) return;
    printf("[EH_NEURO] Disconnecting %d mutant(s) from DAG...\n",
           nctx->mutant_count);
    for (int i = 0; i < nctx->mutant_count; i++) {
        EH_MutantRecord *rec = &nctx->mutants[i];
        if (rec->status == EH_MUTANT_PRUNED) continue;
        EH_DAGNode *p = rec->parent;
        if (!p) continue;
        for (int c = 0; c < p->child_count; c++) {
            if (p->children[c] == rec->node) {
                p->children[c] = p->children[p->child_count - 1];
                p->children[p->child_count - 1] = NULL;
                p->child_count--;
                break;
            }
        }
        rec->status = EH_MUTANT_PRUNED;
    }
    printf("[EH_NEURO] All mutants successfully disconnected.\n");
}

void eh_neuro_free(EH_NeuroContext *nctx) {
    if (!nctx) return;
    /* Arena is managed by caller — do not free here */
    free(nctx);
}