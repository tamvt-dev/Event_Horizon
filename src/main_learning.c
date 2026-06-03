/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/main_learning.c
 * Description: Test driver simulating feedback cycles, data self-adaptation,
 *              and structural mutation (Neuroplasticity).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../include/core/eh_dag.h"
#include "../include/core/eh_scoring.h"
#include "../include/core/eh_dynamic.h"
#include "../include/core/eh_learning.h"
#include "../include/core/eh_engine.h"

#define DIM_SIZE 8

int main(void) {
    printf("==================================================\n");
    printf("   EVENTHORIZON: LIVE EVOLUTION & MUTATION TEST   \n");
    printf("==================================================\n\n");

    /* ---------------------------------------------------------------
     * STEP 1: INITIALIZE GRAPH STRUCTURE (DAG SETUP)
     * Simple static DAG base: Root (0) -> Expert Node (1)
     * --------------------------------------------------------------- */
    printf("[SETUP] Initializing base network topology...\n");
    EH_DAGNode *root = eh_dag_create_node(0, DIM_SIZE, DIM_SIZE);
    EH_DAGNode *expert_node = eh_dag_create_node(1, DIM_SIZE, DIM_SIZE);
    eh_dag_connect_nodes(root, expert_node);

    // Assign default weights to the Expert Node
    for (int i = 0; i < DIM_SIZE * DIM_SIZE; i++) {
        expert_node->weights.data[i] = 0.15f; 
    }
    expert_node->distribution_mean = 0.15f;

    /* ---------------------------------------------------------------
     * STEP 2: INTEGRATE DYNAMIC CONTEXT & LEARNING THRESHOLDS
     * --------------------------------------------------------------- */
    // Initialize Routing Core (Scoring Core)
    EH_ScoringCore *score_core = eh_scoring_init(DIM_SIZE);

    // Initialize Dynamic Context: Gating boundaries [0.25, 0.85]
    EH_DynamicContext *dctx = eh_dynamic_init(root, 0.25f, 0.85f);

    // Initialize Learning Context: 1MB Arena, LR=0.05, Mutation Threshold=0.4f
    EH_LearningContext *lctx = eh_learning_init(1024 * 1024, 0.05f, 0.40f);

    printf("-> Total child nodes of Root currently: %d\n\n", root->child_count);

    /* ---------------------------------------------------------------
     * STEP 3: SIMULATE INFERENCE WITH COMPLEX INPUT
     * --------------------------------------------------------------- */
    printf("--------------------------------------------------\n");
    printf("[PASS 1] Starting hard input vector (requires deep routing)\n");
    
    float input_vector[DIM_SIZE] = {0.5f, 0.4f, 0.6f, 0.5f, 0.4f, 0.6f, 0.5f, 0.4f};
    
    // Compute input norm for the Dynamic Collapse Pass
    float input_norm = 0.0f;
    for (int i = 0; i < DIM_SIZE; i++) input_norm += input_vector[i] * input_vector[i];
    input_norm = sqrtf(input_norm);

    // Evaluate morphological state before activation
    EH_CollapseDecision decision = eh_dynamic_evaluate(dctx, input_vector, input_norm, expert_node);
    printf("-> Morphological Evaluation Node 1: Energy=%.4f | Required Status: %s\n",
           decision.activation_energy, decision.should_collapse ? "COLLAPSE" : "DENSE MATRIX");

    /* ---------------------------------------------------------------
     * STEP 4: FEEDBACK LOOP & MUTATION TRIGGER
     * Assume Beam Search selected the route passing through expert_node,
     * but the output severely deviated from real-world targets (large penalty signal)
     * --------------------------------------------------------------- */
    printf("\n[FEEDBACK] Evaluation Environment: Severe deviation detected!\n");
    
    // Simulate the chosen best Beam path sequence
    EH_BeamPath chosen_path;
    chosen_path.nodes[0] = root;
    chosen_path.nodes[1] = expert_node;
    chosen_path.depth = 2;

    float feedback_signal = -1.5f; // Negative Reinforcement penalty signal
    
    // Simulate the error vector extracted from routing evaluation
    float error_vector[DIM_SIZE] = {0.65f, -0.50f, 0.70f, 0.0f, 0.1f, -0.3f, 0.55f, 0.4f};

    printf("[LEARNING] Optimizing routing parameters (Parametric Learning)...\n");
    // Retrieve initial router weights for validation
    float pre_weight = score_core->routing_weights[expert_node->node_id * DIM_SIZE + 0];
    
    eh_learning_reinforce_path(lctx, score_core, input_vector, &chosen_path, feedback_signal);
    
    float post_weight = score_core->routing_weights[expert_node->node_id * DIM_SIZE + 0];
    printf("-> Routing matrix updated. Weight W_route[0]: %.4f -> %.4f\n", pre_weight, post_weight);

    printf("\n[STRUCTURAL] Evaluating structural mutation conditions (Neuroplasticity)...\n");
    // Trigger dynamic node creation to encapsulate the logical error region
    bool mutated = eh_learning_mutate_graph(lctx, expert_node, error_vector, DIM_SIZE);

    /* ---------------------------------------------------------------
     * STEP 5: VERIFY GRAPH EVOLUTION
     * --------------------------------------------------------------- */
    printf("\n--------------------------------------------------\n");
    printf("[GRAPH VALIDATION AFTER MUTATION]\n");
    if (mutated) {
        printf("-> STATUS: SUCCESS! Graph structure branched successfully.\n");
        printf("-> Expert Node (ID: 1) currently has: %d dynamic child node(s).\n", expert_node->child_count);
        
        EH_DAGNode *mutated_child = expert_node->children[0];
        printf("-> Live Mutant Details -> ID: %d | Matrix Dimensions: %dx%d\n",
               mutated_child->node_id, mutated_child->weights.rows, mutated_child->weights.cols);
        printf("-> Expected internal error residual distribution mean: %.4f\n", mutated_child->distribution_mean);
    } else {
        printf("-> STATUS: Failure or error was insufficient to trigger dynamic mutation.\n");
    }

    /* ---------------------------------------------------------------
     * SYSTEM CLEANUP
     * --------------------------------------------------------------- */
    printf("\n[CLEANUP] Initializing safe deallocation procedure...\n");

    // Step 1: Isolate static DAG structure from dynamic mutants in Arena
    if (expert_node) {
        expert_node->child_count = 0;
        printf("-> 1. Mutants safely disconnected from the DAG tree.\n");
    }

    // Step 2: Release base DAG structure
    printf("-> 2. DAG graph system released... ");
    eh_dag_free_graph(root);
    printf("DONE.\n");

    // Step 3: Release dynamic context
    printf("-> 3. Dynamic context buffer released... ");
    eh_dynamic_free(dctx);
    printf("DONE.\n");

    // Step 4: Release Scoring Core router weights
    printf("-> 4. Routing Core (Scoring Core) released... ");
    eh_scoring_free(score_core);
    printf("DONE.\n");

    // Step 5: Release Memory Arena and Learning Context completely
    printf("-> 5. Memory Arena infrastructure released... ");
    eh_learning_free(lctx);
    printf("DONE.\n");

    printf("\n[CLEANUP] Memory successfully cleared. Execution completed cleanly!\n");
    return 0;
}
