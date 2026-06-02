/*
 * EventHorizon Engine - Arena Unit Tests
 * 
 * Tests memory arena functionality:
 * - Creation and destruction
 * - Allocation
 * - Alignment
 * - Reset
 * - Edge cases
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "eh_arena.h"

#define TEST(name) printf("  Testing: %s ... ", name); fflush(stdout);
#define PASS() printf("PASS\n");
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_arena_create_destroy(void) {
    TEST("arena create/destroy");
    
    EH_Arena *arena = eh_arena_create(1024 * 1024);
    if (!arena) FAIL("Failed to create arena");
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_alloc(void) {
    TEST("basic allocation");
    
    EH_Arena *arena = eh_arena_create(1024);
    if (!arena) FAIL("Failed to create arena");
    
    void *ptr1 = eh_arena_alloc(arena, 64);
    void *ptr2 = eh_arena_alloc(arena, 128);
    
    if (!ptr1 || !ptr2) {
        eh_arena_destroy(arena);
        FAIL("Allocation failed");
    }
    
    // Verify pointers are different
    if (ptr1 == ptr2) {
        eh_arena_destroy(arena);
        FAIL("Got same pointer twice");
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_alignment(void) {
    TEST("16-byte alignment");
    
    EH_Arena *arena = eh_arena_create(8192);
    if (!arena) FAIL("Failed to create arena");
    
    // Allocate various sizes and check alignment
    for (int i = 1; i <= 100; i++) {
        void *ptr = eh_arena_alloc(arena, i);
        if (!ptr) {
            eh_arena_destroy(arena);
            FAIL("Allocation failed");
        }
        
        // Check 16-byte alignment
        if (((uintptr_t)ptr) % 16 != 0) {
            eh_arena_destroy(arena);
            FAIL("Pointer not 16-byte aligned");
        }
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_out_of_memory(void) {
    TEST("out of memory handling");
    
    size_t small_size = 256;
    EH_Arena *arena = eh_arena_create(small_size);
    if (!arena) FAIL("Failed to create arena");
    
    // Try to allocate more than capacity
    void *ptr = eh_arena_alloc(arena, small_size * 2);
    
    if (ptr != NULL) {
        eh_arena_destroy(arena);
        FAIL("Should have returned NULL for oversized allocation");
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_reset(void) {
    TEST("arena reset");
    
    EH_Arena *arena = eh_arena_create(4096);
    if (!arena) FAIL("Failed to create arena");
    
    // Allocate some memory
    for (int i = 0; i < 10; i++) {
        eh_arena_alloc(arena, 128);
    }
    
    size_t used_before = arena->used;
    if (used_before == 0) {
        eh_arena_destroy(arena);
        FAIL("Arena should have memory used");
    }
    
    // Reset
    eh_arena_reset(arena);
    
    if (arena->used != 0) {
        eh_arena_destroy(arena);
        FAIL("Arena not properly reset");
    }
    
    // Verify we can allocate again
    void *ptr = eh_arena_alloc(arena, 256);
    if (!ptr) {
        eh_arena_destroy(arena);
        FAIL("Failed to allocate after reset");
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_node_alloc(void) {
    TEST("node allocation");
    
    EH_Arena *arena = eh_arena_create(1024 * 1024);
    if (!arena) FAIL("Failed to create arena");
    
    EH_DAGNode *node = eh_arena_alloc_node(arena, 0, 8, 8);
    if (!node) {
        eh_arena_destroy(arena);
        FAIL("Failed to allocate node");
    }
    
    // Verify node structure
    if (node->node_id != 0) {
        eh_arena_destroy(arena);
        FAIL("Node ID mismatch");
    }
    
    if (node->weights.rows != 8 || node->weights.cols != 8) {
        eh_arena_destroy(arena);
        FAIL("Weight dimensions mismatch");
    }
    
    if (!node->weights.data) {
        eh_arena_destroy(arena);
        FAIL("Weight data pointer is NULL");
    }
    
    // Verify weight data is contiguous after node struct
    uintptr_t node_end = ((uintptr_t)node) + sizeof(EH_DAGNode);
    uintptr_t weights_start = (uintptr_t)node->weights.data;
    
    // Should be close (within alignment padding)
    if (weights_start < node_end || weights_start > node_end + 16) {
        eh_arena_destroy(arena);
        FAIL("Weights not contiguous with node");
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

static int test_arena_zero_size(void) {
    TEST("zero-size allocation");
    
    EH_Arena *arena = eh_arena_create(1024);
    if (!arena) FAIL("Failed to create arena");
    
    void *ptr = eh_arena_alloc(arena, 0);
    
    // Should return NULL for zero-size
    if (ptr != NULL) {
        eh_arena_destroy(arena);
        FAIL("Should return NULL for zero-size allocation");
    }
    
    eh_arena_destroy(arena);
    PASS();
    return 0;
}

int main(void) {
    printf("EventHorizon Engine - Arena Unit Tests\n");
    printf("======================================\n\n");
    
    int failed = 0;
    
    failed += test_arena_create_destroy();
    failed += test_arena_alloc();
    failed += test_arena_alignment();
    failed += test_arena_out_of_memory();
    failed += test_arena_reset();
    failed += test_arena_node_alloc();
    failed += test_arena_zero_size();
    
    printf("\n======================================\n");
    if (failed == 0) {
        printf("All tests passed! ✓\n");
        return 0;
    } else {
        printf("%d test(s) failed! ✗\n", failed);
        return 1;
    }
}
