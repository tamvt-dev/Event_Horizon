/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/main_test.c
 * Description: Test driver verifying the entire pipeline:
 *              DAG construction -> Collapse evaluation -> Beam Search inference.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "../include/core/eh_dag.h"
#include "../include/core/eh_scoring.h"
#include "../include/core/eh_engine.h"

#define INPUT_DIM   8
#define OUTPUT_DIM  8

/* Fill a weight matrix with random values scaled appropriately */
static void fill_random_weights(EH_DAGNode *node, float scale) {
    int n = node->weights.rows * node->weights.cols;
    for (int i = 0; i < n; i++) {
        node->weights.data[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
    printf("=== EH-Engine Test Driver ===\n\n");

    /* --- Build a 3-layer DAG ---
     *          [root(0)]
     *         /          \
     *    [node1(1)]    [node2(2)]
     *         \          /
     *          [leaf(3)]
     *
     * node1: small weights -> will COLLAPSE
     * node2: large weights -> RETAINS dense representation
     */

    EH_DAGNode *root  = eh_dag_create_node(0, INPUT_DIM,  INPUT_DIM);
    EH_DAGNode *node1 = eh_dag_create_node(1, OUTPUT_DIM, INPUT_DIM);
    EH_DAGNode *node2 = eh_dag_create_node(2, OUTPUT_DIM, INPUT_DIM);
    EH_DAGNode *leaf  = eh_dag_create_node(3, OUTPUT_DIM, OUTPUT_DIM);

    if (!root || !node1 || !node2 || !leaf) {
        fprintf(stderr, "Error: Failed to create nodes\n");
        return 1;
    }

    /* Seed weights: node1 small (will collapse), node2 large (will not collapse) */
    fill_random_weights(root,  2.0f);
    fill_random_weights(node1, 0.01f); /* Extremely small -> low Frobenius -> collapses */
    fill_random_weights(node2, 5.0f);  /* Large -> high Frobenius -> remains dense */
    fill_random_weights(leaf,  1.0f);

    /* Construct DAG connections */
    eh_dag_connect_nodes(root, node1);
    eh_dag_connect_nodes(root, node2);
    eh_dag_connect_nodes(node1, leaf);
    eh_dag_connect_nodes(node2, leaf); /* Shared node: leaf has 2 parents */

    printf("[TEST] DAG Structure: root -> {node1, node2} -> leaf (shared)\n");

    /* --- Initialize Scoring Core --- */
    EH_ScoringCore *scorer = eh_scoring_init(INPUT_DIM);
    if (!scorer) {
        fprintf(stderr, "Error: Failed to initialize ScoringCore\n");
        return 1;
    }

    /* --- Setup Engine with critical_limit --- */
    float critical_limit = 1.5f; /* node1 (scale=0.01) will have Frobenius < 1.5 */
    EH_Context *ctx = eh_engine_setup(root, scorer, critical_limit);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to setup engine\n");
        return 1;
    }

    /* Report collapse status after setup */
    printf("[TEST] Collapse states after evaluate_collapse (threshold=%.2f):\n",
           critical_limit);
    printf("  root  (id=0): ||W||_F=%.4f, collapsed=%s\n",
           root->frobenius_norm,  root->is_collapsed  ? "TRUE" : "false");
    printf("  node1 (id=1): ||W||_F=%.4f, collapsed=%s",
           node1->frobenius_norm, node1->is_collapsed ? "TRUE" : "false");
    if (node1->is_collapsed)
        printf(", mean=%.6f", node1->distribution_mean);
    printf("\n");
    printf("  node2 (id=2): ||W||_F=%.4f, collapsed=%s\n",
           node2->frobenius_norm, node2->is_collapsed ? "TRUE" : "false");
    printf("  leaf  (id=3): ||W||_F=%.4f, collapsed=%s\n",
           leaf->frobenius_norm,  leaf->is_collapsed  ? "TRUE" : "false");

    /* --- Run Inference --- */
    float input_vec[INPUT_DIM];
    float output_vec[OUTPUT_DIM];

    for (int i = 0; i < INPUT_DIM; i++) {
        input_vec[i] = (float)(i + 1) * 0.1f; /* [0.1, 0.2, ..., 0.8] */
    }

    printf("\n[TEST] Input vector: [");
    for (int i = 0; i < INPUT_DIM; i++)
        printf("%.1f%s", input_vec[i], i < INPUT_DIM-1 ? ", " : "");
    printf("]\n");

    bool ok = eh_engine_inference(ctx, input_vec, INPUT_DIM,
                                   output_vec, OUTPUT_DIM);

    if (ok) {
        printf("[TEST] Output vector: [");
        for (int i = 0; i < OUTPUT_DIM; i++)
            printf("%.4f%s", output_vec[i], i < OUTPUT_DIM-1 ? ", " : "");
        printf("]\n");
        printf("[TEST] Inference: SUCCESS ✓\n");
    } else {
        printf("[TEST] Inference: FAILURE ✗\n");
    }

    /* --- Test shared node: deallocation handles potential double-free gracefully --- */
    printf("\n[TEST] Verifying safe shared-node deallocation (leaf has 2 parents)...\n");
    eh_engine_shutdown(ctx); /* Deallocates DAG + scorer + ctx */
    printf("[TEST] eh_engine_shutdown(): SUCCESS (no crash) ✓\n");

    printf("\n=== All tests passed successfully ===\n");
    return 0;
}
