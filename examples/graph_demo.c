/*
 * EventHorizon Engine - Graph/DAG Demo
 * 
 * Demonstrates:
 * - Building a multi-layer DAG
 * - State collapse mechanism
 * - Branch connections
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "eh_dag.h"

// Helper to compute Frobenius norm
static float frobenius_norm(const EH_Matrix *w) {
    double sum = 0.0;
    int n = w->rows * w->cols;
    for (int i = 0; i < n; i++) {
        sum += (double)w->data[i] * (double)w->data[i];
    }
    return (float)sqrt(sum);
}

int main(void) {
    printf("EventHorizon Engine - Graph/DAG Demo\n");
    printf("====================================\n\n");
    
    const int DIM = 8;
    
    // Create a simple DAG with branches
    printf("Building DAG structure:\n");
    printf("         Root (0)\n");
    printf("        /    \\\n");
    printf("       /      \\\n");
    printf("   Expert1  Expert2\n");
    printf("     (1)      (2)\n");
    printf("       \\      /\n");
    printf("        \\    /\n");
    printf("       Merger (3)\n");
    printf("\n");
    
    // Create nodes
    EH_DAGNode *root = eh_dag_create_node(0, DIM, DIM);
    EH_DAGNode *expert1 = eh_dag_create_node(1, DIM, DIM);
    EH_DAGNode *expert2 = eh_dag_create_node(2, DIM, DIM);
    EH_DAGNode *merger = eh_dag_create_node(3, DIM, DIM);
    
    if (!root || !expert1 || !expert2 || !merger) {
        fprintf(stderr, "Error: Failed to create nodes\n");
        return 1;
    }
    
    // Initialize weights with different patterns
    printf("Initializing weights:\n");
    
    // Root: moderate weights
    for (int i = 0; i < DIM * DIM; i++) {
        root->weights.data[i] = 0.5f;
    }
    printf("  Root: uniform 0.5 weights\n");
    
    // Expert 1: strong weights (won't collapse)
    for (int i = 0; i < DIM * DIM; i++) {
        expert1->weights.data[i] = 1.0f + (i % 10) * 0.1f;
    }
    printf("  Expert1: strong weights (1.0 - 1.9)\n");
    
    // Expert 2: weak weights (will collapse)
    for (int i = 0; i < DIM * DIM; i++) {
        expert2->weights.data[i] = 0.01f;
    }
    printf("  Expert2: weak weights (0.01)\n");
    
    // Merger: moderate weights
    for (int i = 0; i < DIM * DIM; i++) {
        merger->weights.data[i] = 0.3f;
    }
    printf("  Merger: uniform 0.3 weights\n");
    printf("\n");
    
    // Connect DAG
    printf("Connecting DAG edges...\n");
    eh_dag_connect_nodes(root, expert1);
    eh_dag_connect_nodes(root, expert2);
    eh_dag_connect_nodes(expert1, merger);
    eh_dag_connect_nodes(expert2, merger);
    printf("  ✓ 4 edges connected\n\n");
    
    // Evaluate collapse
    printf("Evaluating state collapse (threshold = 0.5):\n");
    float threshold = 0.5f;
    
    EH_DAGNode *nodes[] = {root, expert1, expert2, merger};
    const char *names[] = {"Root", "Expert1", "Expert2", "Merger"};
    
    for (int i = 0; i < 4; i++) {
        eh_dag_evaluate_collapse(nodes[i], threshold);
        
        float norm = frobenius_norm(&nodes[i]->weights);
        
        printf("  %s (id=%d):\n", names[i], nodes[i]->node_id);
        printf("    Frobenius norm: %.4f\n", norm);
        printf("    Status: %s\n", 
               nodes[i]->is_collapsed ? "COLLAPSED (O(N))" : "ACTIVE (O(N²))");
        if (nodes[i]->is_collapsed) {
            printf("    Mean value: %.4f\n", nodes[i]->distribution_mean);
        }
        printf("\n");
    }
    
    // Summary
    printf("====================================\n");
    printf("Summary:\n");
    printf("  • Expert2 collapsed: ||W|| < threshold\n");
    printf("  • Expert1 active: ||W|| > threshold\n");
    printf("  • Collapsed nodes use O(N) mean fill\n");
    printf("  • Active nodes use O(N²) matrix multiply\n");
    printf("  • Adaptive computation based on weight magnitude\n");
    
    // Cleanup
    eh_dag_free_graph(root);
    
    return 0;
}
