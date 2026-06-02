/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_dag.c
 * Description: Full DAG management logic — node creation, edge connection,
 *              collapse threshold evaluation, and memory deallocation.
 *
 * Compile: gcc -O3 -std=c99 -Wall -Wextra -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../include/eh_dag.h"

/* =================================================================
 * PART 1: NODE CREATION
 * =================================================================
 *
 * Memory allocation strategy:
 *   The entire EH_DAGNode and its weight matrix data region are allocated
 *   in a SINGLE contiguous malloc call.
 *   Memory layout:
 *     [ EH_DAGNode | float data[rows * cols] ]
 *   This guarantees:
 *     - No cache miss when CPU reads node + weights simultaneously.
 *     - Only one free() call needed for the entire node.
 * ================================================================= */
EH_DAGNode *eh_dag_create_node(int node_id, int rows, int cols) {
    /* --- Validate input parameters --- */
    if (rows <= 0 || cols <= 0) {
        fprintf(stderr, "[EH_DAG] Error: invalid matrix dimensions "
                        "(%d x %d) for node %d\n", rows, cols, node_id);
        return NULL;
    }

    /* --- Compute total memory size required --- */
    size_t data_size = (size_t)(rows * cols) * sizeof(float);
    size_t total     = sizeof(EH_DAGNode) + data_size;

    /*
     * Contiguous allocation: node struct + data lie adjacent on the heap.
     * calloc() ensures zero-initialization, preventing garbage values from
     * corrupting Frobenius norm computations right after creation.
     */
    EH_DAGNode *node = (EH_DAGNode *)calloc(1, total);
    if (!node) {
        fprintf(stderr, "[EH_DAG] Error: failed to allocate %zu bytes "
                        "for node %d\n", total, node_id);
        return NULL;
    }

    /* --- Assign metadata --- */
    node->node_id      = node_id;
    node->is_collapsed = false;
    node->visited      = false;
    node->child_count  = 0;

    /* --- Point weight matrix data to the memory block immediately after struct --- */
    node->weights.rows = rows;
    node->weights.cols = cols;
    /*
     * The data pointer points to the byte immediately following the EH_DAGNode struct.
     * The (char*) cast ensures byte-level pointer arithmetic.
     */
    node->weights.data = (float *)((char *)node + sizeof(EH_DAGNode));

    return node;
}

/* =================================================================
 * PART 2: DIRECTED EDGE CONNECTION
 * ================================================================= */
bool eh_dag_connect_nodes(EH_DAGNode *parent, EH_DAGNode *child) {
    if (!parent || !child) {
        fprintf(stderr, "[EH_DAG] Error: cannot connect NULL node\n");
        return false;
    }

    /* Check child slot limit */
    if (parent->child_count >= EH_MAX_CHILDREN) {
        fprintf(stderr, "[EH_DAG] Error: node %d has reached the limit of %d children\n",
                parent->node_id, EH_MAX_CHILDREN);
        return false;
    }

    parent->children[parent->child_count++] = child;
    return true;
}

/* =================================================================
 * PART 3: COLLAPSE THRESHOLD EVALUATION — FROBENIUS NORM
 * =================================================================
 *
 * Formula:
 *   ||W||_F = sqrt( Σ_{i,j} w_{ij}^2 )
 *
 * Collapse condition (Event Horizon):
 *   If ||W||_F < critical_threshold:
 *     → is_collapsed = true
 *     → distribution_mean = (Σ w_{ij}) / (rows * cols)
 *
 * When is_collapsed == true, the entire O(N^2) dense matmul at this node
 * is replaced by an O(N) assignment using distribution_mean — this is the
 * core computational savings mechanism.
 * ================================================================= */
void eh_dag_evaluate_collapse(EH_DAGNode *node, float critical_threshold) {
    if (!node) return;

    const EH_Matrix *W    = &node->weights;
    int              n    = W->rows * W->cols;
    const float     *data = W->data;

    /* --- Step 1: Compute Frobenius norm ---
     * Single pass through all elements to simultaneously compute
     * sum_sq (for Frobenius) and sum (for distribution_mean).
     * Reduces memory access count to a single O(N) pass.
     */
    double sum_sq = 0.0;
    double sum    = 0.0;

    for (int i = 0; i < n; i++) {
        double val = (double)data[i];
        sum_sq += val * val;
        sum    += val;
    }

    float frob_norm = (float)sqrt(sum_sq);
    node->frobenius_norm = frob_norm;

    /* --- Step 2: Compare against the R_I threshold ---
     * If the Frobenius norm < critical_threshold, this layer lies within
     * the "event horizon" — the information is not valuable enough to
     * maintain a full matrix structure.
     */
    if (frob_norm < critical_threshold) {
        node->is_collapsed       = true;
        node->distribution_mean  = (n > 0) ? (float)(sum / n) : 0.0f;

#ifdef EH_DEBUG
        printf("[EH_DAG] Node %d COLLAPSED: ||W||_F=%.4f < threshold=%.4f, "
               "mean=%.4f\n",
               node->node_id, frob_norm, critical_threshold,
               node->distribution_mean);
#endif
    } else {
        node->is_collapsed = false;

#ifdef EH_DEBUG
        printf("[EH_DAG] Node %d ACTIVE: ||W||_F=%.4f >= threshold=%.4f\n",
               node->node_id, frob_norm, critical_threshold);
#endif
    }
}

/* =================================================================
 * PART 4: RECURSIVE MEMORY DEALLOCATION — PREVENT DOUBLE-FREE
 * =================================================================
 *
 * Problem: DAGs can have shared nodes (multiple parents pointing to the same child).
 * A naive recursive free() would free shared nodes multiple times
 * → undefined behavior (heap corruption).
 *
 * Solution: Use the `visited` flag to mark already-processed nodes.
 *
 * Important note on traversal order:
 *   Post-order traversal: free child nodes BEFORE parent nodes.
 *   This prevents dangling pointers during recursive unwinding.
 *
 * Note on memory layout:
 *   Since the node and weights.data are allocated in the same malloc(),
 *   a single free(node) releases both — weights.data must NOT be freed separately.
 * ================================================================= */

/*
 * Internal function: recursively mark and free nodes in post-order.
 * The `visited = true` flag is set BEFORE recursing into children,
 * ensuring any other path encountering this node will skip it immediately.
 */
static void _eh_dag_free_recursive(EH_DAGNode *node) {
    /* --- Base case: NULL node or already processed --- */
    if (!node || node->visited) return;

    /*
     * Mark BEFORE recursing into children.
     * In case of any cycle (should not occur in a valid DAG, but
     * guarded defensively), this flag prevents infinite recursion.
     */
    node->visited = true;

    /* --- Recursively free all child nodes first --- */
    for (int i = 0; i < node->child_count; i++) {
        _eh_dag_free_recursive(node->children[i]);
        node->children[i] = NULL; /* Clear dangling pointer */
    }

    /*
     * Free the current node.
     * Since the memory layout is [EH_DAGNode | float data[]],
     * free(node) releases weights.data simultaneously.
     */
    free(node);
}

/*
 * Internal function: reset the visited flag across the entire graph
 * before the free pass.
 */
static void _eh_dag_reset_visited(EH_DAGNode *node) {
    if (!node || !node->visited) return;
    /*
     * Set false BEFORE recursing to prevent revisiting in graphs
     * with shared edges.
     */
    node->visited = false;
    for (int i = 0; i < node->child_count; i++) {
        _eh_dag_reset_visited(node->children[i]);
    }
}

void eh_dag_free_graph(EH_DAGNode *root) {
    if (!root) return;

    /*
     * Step 1: DFS pass to reset all visited flags.
     * This safety step ensures all nodes are in the unvisited state
     * before deallocation begins.
     */
    _eh_dag_reset_visited(root);

    /*
     * Step 2: Post-order DFS to free nodes from leaves up to the root.
     * The visited flag is reused in this step to prevent double-free.
     */
    _eh_dag_free_recursive(root);
}
