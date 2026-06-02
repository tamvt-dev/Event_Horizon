/*
 * EventHorizon Engine - DAG Unit Tests
 */

#include <stdio.h>
#include <assert.h>
#include "eh_dag.h"

#define TEST(name) printf("  Testing: %s ... ", name); fflush(stdout);
#define PASS() printf("PASS\n");
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_dag_create_node(void) {
    TEST("node creation");
    
    EH_DAGNode *node = eh_dag_create_node(0, 4, 4);
    if (!node) FAIL("Failed to create node");
    
    if (node->weights.rows != 4 || node->weights.cols != 4) {
        FAIL("Dimension mismatch");
    }
    
    eh_dag_free_graph(node);
    PASS();
    return 0;
}

static int test_dag_connect_nodes(void) {
    TEST("node connection");
    
    EH_DAGNode *parent = eh_dag_create_node(0, 4, 4);
    EH_DAGNode *child = eh_dag_create_node(1, 4, 4);
    
    if (!parent || !child) FAIL("Failed to create nodes");
    
    bool success = eh_dag_connect_nodes(parent, child);
    if (!success) FAIL("Failed to connect nodes");
    
    if (parent->child_count != 1) FAIL("Child count incorrect");
    if (parent->children[0] != child) FAIL("Child pointer incorrect");
    
    eh_dag_free_graph(parent);
    PASS();
    return 0;
}

static int test_dag_collapse(void) {
    TEST("state collapse");
    
    EH_DAGNode *node = eh_dag_create_node(0, 4, 4);
    if (!node) FAIL("Failed to create node");
    
    // Fill with small values
    for (int i = 0; i < 16; i++) {
        node->weights.data[i] = 0.01f;
    }
    
    eh_dag_evaluate_collapse(node, 0.5f);
    
    if (!node->is_collapsed) FAIL("Node should have collapsed");
    
    eh_dag_free_graph(node);
    PASS();
    return 0;
}

int main(void) {
    printf("EventHorizon Engine - DAG Unit Tests\n");
    printf("====================================\n\n");
    
    int failed = 0;
    
    failed += test_dag_create_node();
    failed += test_dag_connect_nodes();
    failed += test_dag_collapse();
    
    printf("\n====================================\n");
    if (failed == 0) {
        printf("All tests passed! ✓\n");
        return 0;
    } else {
        printf("%d test(s) failed! ✗\n", failed);
        return 1;
    }
}
