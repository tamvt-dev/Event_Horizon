/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_learning.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../include/core/eh_learning.h"

/* --- Internal: Allocate from Memory Arena (zero fragmentation) --- */
static void *arena_alloc(EH_MemoryArena *arena, size_t size) {
    /* 8-byte alignment for optimal CPU cache-line access */
    size_t aligned_size = (size + 7) & ~7;
    if (arena->offset + aligned_size > arena->capacity) {
        fprintf(stderr, "[EH_ARENA] Out of memory in Arena Allocator!\n");
        return NULL;
    }
    void *ptr = &arena->buffer[arena->offset];
    arena->offset += aligned_size;
    memset(ptr, 0, aligned_size);
    return ptr;
}

/* ================================================================
 * INITIALIZE LEARNING CONTEXT
 * ================================================================ */
EH_LearningContext *eh_learning_init(size_t arena_size, float lr, float mutation_thresh) {
    EH_LearningContext *lctx = (EH_LearningContext *)malloc(sizeof(EH_LearningContext));
    if (!lctx) return NULL;

    lctx->arena = (EH_MemoryArena *)malloc(sizeof(EH_MemoryArena));
    if (!lctx->arena) {
        free(lctx);
        return NULL;
    }

    lctx->arena->buffer = (uint8_t *)malloc(arena_size);
    if (!lctx->arena->buffer) {
        free(lctx->arena); free(lctx);
        return NULL;
    }

    lctx->arena->capacity   = arena_size;
    lctx->arena->offset     = 0;
    lctx->learning_rate     = lr;
    lctx->mutation_thresh   = mutation_thresh;

#ifdef EH_DEBUG
    printf("[EH_LEARNING] Online learning system initialized. Arena: %zu KB\n", arena_size / 1024);
#endif
    return lctx;
}

/* ================================================================
 * PART 1: PARAMETER LEARNING (REINFORCEMENT DELTA UPDATE)
 * ================================================================ */
void eh_learning_reinforce_path(const EH_LearningContext *lctx,
                                EH_ScoringCore           *score_core,
                                const float              *input_vector,
                                const EH_BeamPath        *chosen_path,
                                float                     feedback_signal)
{
    if (!lctx || !score_core || !input_vector || !chosen_path) return;

    float eta = lctx->learning_rate;
    int   dim = score_core->input_dim;

    /* Traverse nodes visited along the best Beam path to update routing weights */
    for (int i = 0; i < chosen_path->depth; i++) {
        EH_DAGNode *node = chosen_path->nodes[i];
        if (!node) continue;

        int   node_id = node->node_id;
        float *w_row  = score_core->routing_weights + ((size_t)node_id * (size_t)dim);

        /* Online O(N) gradient update: refine per-node routing vector */
        #pragma GCC ivdep
        for (int k = 0; k < dim; k++) {
            w_row[k] += eta * feedback_signal * input_vector[k];
        }
    }
}

/* ================================================================
 * PART 2: STRUCTURAL LEARNING (DYNAMIC NEUROPLASTICITY MUTATION)
 * ================================================================ */
bool eh_learning_mutate_graph(EH_LearningContext *lctx,
                              EH_DAGNode         *parent_node,
                              const float        *error_vector,
                              int                 dim)
{
    if (!lctx || !parent_node || !error_vector) return false;

    /* Check whether the parent node has available child slots */
    if (parent_node->child_count >= EH_MAX_CHILDREN) {
        return false;
    }

    /* Compute the L2 norm of the error vector to measure uncertainty */
    double err_sum = 0.0;
    for (int i = 0; i < dim; i++) {
        err_sum += (double)error_vector[i] * (double)error_vector[i];
    }
    float total_error = (float)sqrt(err_sum);

    /* If error is below the mutation threshold, no structural change is needed */
    if (total_error < lctx->mutation_thresh) {
        return false;
    }

    /* PERFORM MUTATION: allocate a new node from the Arena */
    size_t node_struct_size = sizeof(EH_DAGNode);
    size_t weight_data_size = sizeof(float) * dim * dim; /* Assume square refinement matrix */

    /* Contiguous allocation eliminates cache misses for node + weights */
    EH_DAGNode *new_mutated_node = (EH_DAGNode *)arena_alloc(lctx->arena,
                                                              node_struct_size + weight_data_size);
    if (!new_mutated_node) return false;

    /* Assign metadata to the new mutated node */
    static int dynamic_id_counter = 100; /* Dynamic node ID namespace starts at 100 */
    new_mutated_node->node_id       = dynamic_id_counter++;
    new_mutated_node->weights.rows  = dim;
    new_mutated_node->weights.cols  = dim;
    new_mutated_node->weights.data  = (float *)((uint8_t *)new_mutated_node + node_struct_size);

    /*
     * Error-driven weight initialization:
     * The new node is seeded with knowledge specialized to cancel the current error.
     * Diagonal elements carry scaled error components; off-diagonal elements are zero.
     */
    for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
            new_mutated_node->weights.data[r * dim + c] = (r == c) ? error_vector[r] * 0.5f : 0.0f;
        }
    }

    new_mutated_node->distribution_mean = total_error / (float)dim;
    new_mutated_node->child_count       = 0;
    new_mutated_node->visited           = false;

    /* ATTACH NEW EDGE IN REAL TIME (real-time topology restructuring) */
    parent_node->children[parent_node->child_count] = new_mutated_node;
    parent_node->child_count++;

#ifdef EH_DEBUG
    printf("[NEUROPLASTICITY] Mutation successful! New node ID: %d attached to parent ID: %d | Suppressed error: %.4f\n",
           new_mutated_node->node_id, parent_node->node_id, total_error);
#endif

    return true;
}

/* ================================================================
 * RELEASE LEARNING CONTEXT RESOURCES
 * ================================================================ */
void eh_learning_free(EH_LearningContext *lctx) {
    if (!lctx) return;
    if (lctx->arena) {
        if (lctx->arena->buffer) free(lctx->arena->buffer);
        free(lctx->arena);
    }
    free(lctx);
}