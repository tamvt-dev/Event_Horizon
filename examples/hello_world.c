/*
 * EventHorizon Engine - Hello World Example
 * 
 * Demonstrates the absolute minimum code needed to:
 * 1. Initialize the engine
 * 2. Run a single inference
 * 3. Clean up resources
 */

#include <stdio.h>
#include <stdlib.h>
#include "eh_engine.h"
#include "eh_dag.h"
#include "eh_scoring.h"

int main(void) {
    printf("EventHorizon Engine - Hello World\n");
    printf("==================================\n\n");
    
    // Configuration
    const int INPUT_DIM = 16;
    const int OUTPUT_DIM = 16;
    
    printf("Step 1: Creating DAG with single node...\n");
    EH_DAGNode *root = eh_dag_create_node(0, OUTPUT_DIM, INPUT_DIM);
    if (!root) {
        fprintf(stderr, "Error: Failed to create DAG node\n");
        return 1;
    }
    
    // Initialize weights with simple pattern
    for (int i = 0; i < INPUT_DIM * OUTPUT_DIM; i++) {
        root->weights.data[i] = 0.1f;
    }
    printf("   ✓ DAG created (1 node, %dx%d weights)\n\n", OUTPUT_DIM, INPUT_DIM);
    
    printf("Step 2: Creating scoring core...\n");
    EH_ScoringCore *scorer = eh_scoring_init(INPUT_DIM);
    if (!scorer) {
        fprintf(stderr, "Error: Failed to create scoring core\n");
        eh_dag_free_graph(root);
        return 1;
    }
    printf("   ✓ Scoring core initialized\n\n");
    
    printf("Step 3: Setting up engine context...\n");
    EH_Context *ctx = eh_engine_setup(root, scorer, 0.5f);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to setup engine\n");
        eh_dag_free_graph(root);
        eh_scoring_free(scorer);
        return 1;
    }
    printf("   ✓ Engine context ready\n\n");
    
    printf("Step 4: Preparing input vector...\n");
    float input[INPUT_DIM];
    float output[OUTPUT_DIM];
    
    // Fill input with simple pattern
    for (int i = 0; i < INPUT_DIM; i++) {
        input[i] = (float)i * 0.1f;
    }
    printf("   Input: [%.1f, %.1f, %.1f, ..., %.1f]\n", 
           input[0], input[1], input[2], input[INPUT_DIM-1]);
    printf("\n");
    
    printf("Step 5: Running inference...\n");
    bool success = eh_engine_inference(ctx, input, INPUT_DIM, output, OUTPUT_DIM);
    if (!success) {
        fprintf(stderr, "Error: Inference failed\n");
        eh_engine_shutdown(ctx);
        return 1;
    }
    printf("   ✓ Inference complete!\n");
    printf("   Output: [%.3f, %.3f, %.3f, ..., %.3f]\n",
           output[0], output[1], output[2], output[OUTPUT_DIM-1]);
    printf("\n");
    
    printf("Step 6: Cleaning up resources...\n");
    eh_engine_shutdown(ctx);
    printf("   ✓ All resources released\n\n");
    
    printf("==================================\n");
    printf("Success! EventHorizon is working.\n");
    
    return 0;
}
