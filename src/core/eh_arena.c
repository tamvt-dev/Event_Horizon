/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_arena.c
 * Description: Memory Arena implementation — $O(1)$ fragmentation-free allocator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/eh_arena.h"

/* =================================================================
 * PART 1: ARENA INITIALIZATION
 * ================================================================= */
EH_Arena *eh_arena_create(size_t capacity) {
    if (capacity == 0) capacity = EH_ARENA_SIZE;

    EH_Arena *arena = (EH_Arena *)malloc(sizeof(EH_Arena));
    if (!arena) {
        fprintf(stderr, "[EH_ARENA] Error: failed to allocate Arena struct\n");
        return NULL;
    }

    /*
     * Allocate static memory block once.
     * calloc() guarantees zero-initialization to avoid old heap data
     * from corrupting mutation nodes created later.
     */
    arena->base = (char *)calloc(1, capacity);
    if (!arena->base) {
        fprintf(stderr, "[EH_ARENA] Error: failed to allocate %zu bytes\n",
                capacity);
        free(arena);
        return NULL;
    }

    arena->capacity    = capacity;
    arena->used        = 0;
    arena->node_count  = 0;
    arena->alloc_count = 0;

    printf("[EH_ARENA] Initialized: %.2f MB at %p\n",
           (float)capacity / (1024.0f * 1024.0f), (void *)arena->base);

    return arena;
}

/* =================================================================
 * PART 2: ARENA ALLOCATION — HOT PATH
 * =================================================================
 *
 * Alignment Technique:
 *   CPUs are most efficient when reading memory aligned to boundaries:
 *   float (4 bytes) → 4-byte alignment
 *   AVX register (256-bit) → 32-byte alignment
 *   We use 16-byte alignment to be safe for both SSE and most AVX instructions.
 *
 *   Formula: aligned = (raw + align - 1) & ~(align - 1)
 *   Example with align = 16:
 *     raw = 17 → aligned = 32 (jumps to next multiple of 16)
 *     raw = 16 → aligned = 16 (already aligned, unchanged)
 * ================================================================= */
void *eh_arena_alloc(EH_Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    /* Align size to EH_ARENA_ALIGN boundary */
    size_t aligned_size = (size + EH_ARENA_ALIGN - 1) & ~(size_t)(EH_ARENA_ALIGN - 1);

    /* Check if there is enough space remaining */
    if (arena->used + aligned_size > arena->capacity) {
        fprintf(stderr, "[EH_ARENA] OUT OF SPACE: requested %zu, remaining %zu bytes\n",
                aligned_size, arena->capacity - arena->used);
        return NULL;
    }

    /*
     * ALLOCATION OPERATION: simple pointer-bump offset addition — absolute $O(1)$.
     * No mutexes, no syscalls, zero fragmentation.
     */
    void *ptr = (void *)(arena->base + arena->used);
    arena->used        += aligned_size;
    arena->alloc_count += 1;

    return ptr;
}

/* =================================================================
 * PART 3: NODE ALLOCATION — CONTIGUOUS LAYOUT
 * =================================================================
 *
 * Layout inside the Arena:
 *   [ EH_DAGNode (struct) | padding | float data[rows * cols] ]
 *   Everything lies contiguously, allocated via a single eh_arena_alloc call.
 * ================================================================= */
EH_DAGNode *eh_arena_alloc_node(EH_Arena *arena,
                                int       node_id,
                                int       rows,
                                int       cols) {
    if (!arena || rows <= 0 || cols <= 0) return NULL;

    size_t data_size  = (size_t)(rows * cols) * sizeof(float);
    size_t total_size = sizeof(EH_DAGNode) + data_size;

    /* Allocate from Arena — $O(1)$ */
    EH_DAGNode *node = (EH_DAGNode *)eh_arena_alloc(arena, total_size);
    if (!node) {
        fprintf(stderr, "[EH_ARENA] Failed to allocate node %d (%zu bytes)\n",
                node_id, total_size);
        return NULL;
    }

    /* Zero-init the entire struct */
    memset(node, 0, total_size);

    /* Assign metadata */
    node->node_id      = node_id;
    node->is_collapsed = false;
    node->visited      = false;
    node->child_count  = 0;

    /* Weight matrix starts directly after the struct inside the Arena */
    node->weights.rows = rows;
    node->weights.cols = cols;
    node->weights.data = (float *)((char *)node + sizeof(EH_DAGNode));

    arena->node_count++;

#ifdef EH_DEBUG
    printf("[EH_ARENA] Node %d allocated: %dx%d = %zu bytes, "
           "Arena used: %zu/%zu\n",
           node_id, rows, cols, total_size,
           arena->used, arena->capacity);
#endif

    return node;
}

/* =================================================================
 * PART 4: ARENA RESET — $O(1)$
 * =================================================================
 *
 * This is the killer feature of the Arena Allocator:
 * Instead of freeing each individual node (O(n)), reset simply sets used = 0.
 * The entire memory block is instantly reusable.
 *
 * IMPORTANT NOTE: All pointers to the Arena after reset become dangling pointers.
 * The caller is responsible for ensuring no references remain before resetting.
 * ================================================================= */
void eh_arena_reset(EH_Arena *arena) {
    if (!arena) return;

#ifdef EH_DEBUG
    printf("[EH_ARENA] Reset: released %zu bytes (%d nodes, %d allocs)\n",
           arena->used, arena->node_count, arena->alloc_count);
#endif

    /*
     * Zero-fill the used block to prevent stale data from
     * corrupting new nodes in subsequent inference passes.
     */
    memset(arena->base, 0, arena->used);

    arena->used       = 0;
    arena->node_count = 0;
    /* alloc_count remains unchanged to track total allocations across sessions */
}

/* =================================================================
 * PART 5: STATS AND DESTRUCTION
 * ================================================================= */
void eh_arena_stats(const EH_Arena *arena) {
    if (!arena) return;
    printf("[EH_ARENA] Stats:\n");
    printf("  Capacity : %.2f MB\n",  (float)arena->capacity / (1024.f*1024.f));
    printf("  Used     : %.2f KB (%zu bytes, %.1f%%)\n",
           (float)arena->used / 1024.0f, arena->used,
           100.0f * (float)arena->used / (float)arena->capacity);
    printf("  Nodes    : %d live\n",  arena->node_count);
    printf("  Allocs   : %d total\n", arena->alloc_count);
}

void eh_arena_destroy(EH_Arena *arena) {
    if (!arena) return;
    eh_arena_stats(arena);
    free(arena->base);
    arena->base = NULL;
    free(arena);
    printf("[EH_ARENA] Fully released.\n");
}