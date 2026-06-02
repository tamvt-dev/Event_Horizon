/*
 * EventHorizon Engine - Memory Arena Demo
 * 
 * Demonstrates:
 * - Arena allocation
 * - Pointer-bump allocation
 * - Zero-cost reset
 * - Memory statistics
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "eh_arena.h"

#define ARENA_SIZE (4 * 1024 * 1024)  // 4 MB
#define NUM_ALLOCS 10000

int main(void) {
    printf("EventHorizon Engine - Memory Arena Demo\n");
    printf("========================================\n\n");
    
    // Create arena
    printf("Creating arena (%d MB)...\n", ARENA_SIZE / (1024 * 1024));
    EH_Arena *arena = eh_arena_create(ARENA_SIZE);
    if (!arena) {
        fprintf(stderr, "Error: Failed to create arena\n");
        return 1;
    }
    printf("✓ Arena created\n\n");
    
    // Demonstrate O(1) allocation
    printf("Test 1: Pointer-bump allocation (%d allocations)\n", NUM_ALLOCS);
    clock_t start = clock();
    
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = 128 + (i % 256);  // Variable sizes
        void *ptr = eh_arena_alloc(arena, size);
        if (!ptr) {
            fprintf(stderr, "Allocation failed at iteration %d\n", i);
            break;
        }
        
        // Touch memory to ensure it's committed
        ((char *)ptr)[0] = (char)i;
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("  Time: %.4f sec\n", elapsed);
    printf("  Rate: %.2f million allocs/sec\n", (NUM_ALLOCS / elapsed) / 1e6);
    printf("  Avg:  %.2f ns per allocation\n", (elapsed / NUM_ALLOCS) * 1e9);
    printf("\n");
    
    // Show statistics
    printf("Arena statistics after allocations:\n");
    eh_arena_stats(arena);
    printf("\n");
    
    // Demonstrate O(1) reset
    printf("Test 2: O(1) arena reset\n");
    clock_t reset_start = clock();
    eh_arena_reset(arena);
    clock_t reset_end = clock();
    
    double reset_time = (double)(reset_end - reset_start) / CLOCKS_PER_SEC;
    printf("  Reset time: %.6f sec\n", reset_time);
    printf("  ✓ All %d allocations freed in O(1) time!\n", NUM_ALLOCS);
    printf("\n");
    
    // Verify arena is reusable
    printf("Test 3: Arena reuse after reset\n");
    void *ptr1 = eh_arena_alloc(arena, 1024);
    void *ptr2 = eh_arena_alloc(arena, 2048);
    void *ptr3 = eh_arena_alloc(arena, 512);
    
    if (ptr1 && ptr2 && ptr3) {
        printf("  ✓ Successfully allocated 3 blocks after reset\n");
        printf("  ✓ Arena is fully reusable\n");
    }
    printf("\n");
    
    // Final statistics
    printf("Final arena statistics:\n");
    eh_arena_stats(arena);
    printf("\n");
    
    // Cleanup
    eh_arena_destroy(arena);
    
    printf("========================================\n");
    printf("Key Takeaways:\n");
    printf("  • Allocation: O(1) pointer arithmetic\n");
    printf("  • Reset: O(1) regardless of allocation count\n");
    printf("  • No fragmentation: contiguous memory layout\n");
    printf("  • Cache friendly: sequential access pattern\n");
    
    return 0;
}
