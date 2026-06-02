/*
 * EventHorizon Engine - Engine Unit Tests
 */

#include <stdio.h>
#include "eh_engine.h"
#include "eh_dag.h"
#include "eh_scoring.h"

#define TEST(name) printf("  Testing: %s ... ", name); fflush(stdout);
#define PASS() printf("PASS\n");
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_engine_setup(void) {
    TEST("engine setup");
    
    EH_DAGNode *root = eh_dag_create_node(0, 8, 8);
    EH_ScoringCore *scorer = eh_scoring_init(8);
    
    if (!root || !scorer) FAIL("Failed to create components");
    
    EH_Context *ctx = eh_engine_setup(root, scorer, 0.5f);
    if (!ctx) FAIL("Failed to setup engine");
    
    eh_engine_shutdown(ctx);
    PASS();
    return 0;
}

static int test_engine_inference(void) {
    TEST("basic inference");
    
    EH_DAGNode *root = eh_dag_create_node(0, 8, 8);
    EH_ScoringCore *scorer = eh_scoring_init(8);
    EH_Context *ctx = eh_engine_setup(root, scorer, 0.5f);
    
    if (!ctx) FAIL("Failed to setup engine");
    
    float input[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[8] = {0};
    
    bool success = eh_engine_inference(ctx, input, 8, output, 8);
    if (!success) FAIL("Inference failed");
    
    eh_engine_shutdown(ctx);
    PASS();
    return 0;
}

int main(void) {
    printf("EventHorizon Engine - Engine Unit Tests\n");
    printf("=======================================\n\n");
    
    int failed = 0;
    
    failed += test_engine_setup();
    failed += test_engine_inference();
    
    printf("\n=======================================\n");
    if (failed == 0) {
        printf("All tests passed! ✓\n");
        return 0;
    } else {
        printf("%d test(s) failed! ✗\n", failed);
        return 1;
    }
}
