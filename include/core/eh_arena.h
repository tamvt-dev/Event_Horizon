/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_arena.h
 * Description: Memory Arena / Slab Allocator for Direction 2 — Neuroplasticity.
 *
 * Issues with malloc() in hot paths:
 *   When Beam Search continuously generates and destroys mutation nodes during inference,
 *   calling malloc()/free() thousands of times causes:
 *   1. Heap fragmentation — memory gets broken into small fragments
 *   2. Lock contention — glibc malloc uses internal mutexes
 *   3. Latency spikes — each malloc can take 100-1000ns
 *
 * Solution — Memory Arena:
 *   Allocate ONE large flat memory block (EH_ARENA_SIZE) during startup.
 *   All allocation requests in hot paths are simplified to pointer offset addition:
 *     ptr = arena->base + arena->used;
 *     arena->used += size;
 *   → O(1), lock-free, fragmentation-free.
 *
 *   Reset the entire Arena after each inference pass by setting used = 0.
 *   No need to free individual nodes.
 *
 * Arena Memory Layout:
 *   [ EH_DAGNode_0 | weights_0 | EH_DAGNode_1 | weights_1 | ... | unused ]
 *    ^base                                                         ^base+used
 */

#ifndef EH_ARENA_H
#define EH_ARENA_H

#include <stddef.h>
#include <stdbool.h>
#include "eh_dag.h"

/* =================================================================
 * ARENA CONSTANTS
 * ================================================================= */
#define EH_ARENA_SIZE        (16 * 1024 * 1024)  /* 16 MB static RAM */
#define EH_ARENA_ALIGN       16                   /* 16-byte alignment (AVX-safe) */
#define EH_ARENA_MAX_NODES   512                  /* Maximum number of mutation nodes */

/* =================================================================
 * ARENA STRUCTS
 * ================================================================= */
typedef struct {
    char  *base;          /* Pointer to pre-allocated memory block */
    size_t capacity;      /* Total capacity (bytes) */
    size_t used;          /* Total used bytes */
    int    node_count;    /* Number of mutation nodes currently alive in arena */
    int    alloc_count;   /* Total allocation count from startup (stats) */
} EH_Arena;

/* =================================================================
 * PUBLIC API
 * ================================================================= */

/*
 * Initialize Arena: allocate static memory block exactly once.
 * Returns: pointer to new EH_Arena, or NULL on failure.
 */
EH_Arena *eh_arena_create(size_t capacity);

/*
 * Allocate `size` bytes from Arena with EH_ARENA_ALIGN alignment.
 * HOT PATH — pointer addition only, O(1), lock-free.
 * Returns: pointer to memory region, or NULL if Arena is full.
 */
void *eh_arena_alloc(EH_Arena *arena, size_t size);

/*
 * Allocate and initialize a new EH_DAGNode from Arena.
 * Node + weight data are contiguous in Arena.
 * Returns: pointer to the new node in Arena.
 */
EH_DAGNode *eh_arena_alloc_node(EH_Arena *arena,
                                int       node_id,
                                int       rows,
                                int       cols);

/*
 * Reset Arena to empty state — O(1), without calling free().
 * All old data is invalidated but physical memory is preserved.
 * Call after each inference pass if the Arena needs to be reused.
 */
void eh_arena_reset(EH_Arena *arena);

/*
 * Print Arena usage statistics (used for debug/benchmarking).
 */
void eh_arena_stats(const EH_Arena *arena);

/*
 * Free the entire Arena (called only once during shutdown).
 */
void eh_arena_destroy(EH_Arena *arena);

#endif /* EH_ARENA_H */