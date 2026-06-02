/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_dag.h
 * Description: DAG (Directed Acyclic Graph) data structure definitions
 *              and graph manipulation API.
 */

#ifndef EH_DAG_H
#define EH_DAG_H

#include <stdbool.h>
#include <stddef.h>

/* =========================================================
 * STRUCTURAL CAPACITY CONSTANTS
 * ========================================================= */
#define EH_MAX_CHILDREN   8    /* Maximum number of children per node */
#define EH_MAX_NODES    256    /* Maximum total nodes in the graph    */

/* =========================================================
 * WEIGHT MATRIX STRUCTURE
 * Stored contiguously for optimal CPU cache performance.
 * data points to a flat memory region: data[r * cols + c] = element [r][c]
 * ========================================================= */
typedef struct {
    int    rows;   /* Number of rows    */
    int    cols;   /* Number of columns */
    float *data;   /* Pointer to flat (row-major) memory block */
} EH_Matrix;

/* =========================================================
 * DAG NODE STRUCTURE
 * Each node represents one layer or neuron cluster in the network.
 * When is_collapsed == true, the entire weight matrix has "collapsed"
 * and is replaced by a scalar distribution_mean.
 * ========================================================= */
typedef struct EH_DAGNode {
    int     node_id;             /* Unique node identifier */

    /* --- Weight matrix and collapse state --- */
    EH_Matrix weights;           /* Original weight matrix               */
    bool      is_collapsed;      /* Flag: true when Frobenius norm < R_I */
    float     distribution_mean; /* Replacement scalar when collapsed    */
    float     frobenius_norm;    /* Pre-computed Frobenius norm          */

    /* --- Graph topology --- */
    struct EH_DAGNode *children[EH_MAX_CHILDREN]; /* Child node pointers  */
    int                child_count;               /* Current child count  */

    /* --- Traversal flag (prevents double-free on shared nodes) --- */
    bool visited;
} EH_DAGNode;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/*
 * Create a new node with a weight matrix of dimensions (rows x cols).
 * Node struct and weight data are allocated contiguously.
 * Returns: pointer to the new node, or NULL on failure.
 */
EH_DAGNode *eh_dag_create_node(int node_id, int rows, int cols);

/*
 * Add a directed edge from parent -> child in the DAG.
 * Returns: true on success, false if parent has no available child slots.
 */
bool eh_dag_connect_nodes(EH_DAGNode *parent, EH_DAGNode *child);

/*
 * Evaluate the collapse condition for a node against the R_I threshold.
 * - Computes Frobenius norm: ||W||_F = sqrt(sum(w_ij^2))
 * - If ||W||_F < critical_threshold: sets is_collapsed = true,
 *   computes distribution_mean = average of all weight elements.
 */
void eh_dag_evaluate_collapse(EH_DAGNode *node, float critical_threshold);

/*
 * Recursively free the entire graph.
 * Guarantees no double-free on shared nodes via the visited flag.
 * After this call, the root pointer becomes invalid.
 */
void eh_dag_free_graph(EH_DAGNode *root);

#endif /* EH_DAG_H */
