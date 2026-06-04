/* ================================================================
 * eh_hgn_builder.h — HGN Builder: Programmatic Graph Construction
 *
 * Purpose: Build HGN graphs from scratch for training/testing.
 * Allows adding nodes, edges, embeddings, and weights dynamically.
 *
 * Usage:
 *   1. Create builder with vocab_size and max_fanout
 *   2. Set node embeddings
 *   3. Add edges (src → dst with weight vectors)
 *   4. Finalize → converts to EH_HGN_BaseDag
 *   5. Save using eh_hgn_io_save()
 *
 * Example:
 *   EH_HGN_Builder *b = eh_hgn_builder_create(100, 32);
 *   eh_hgn_builder_set_embedding(b, 0, embed_vec);
 *   eh_hgn_builder_add_edge(b, 0, 1, 0.5f, weight_vec);
 *   eh_hgn_builder_finalize(b, arena, &dag);
 *   eh_hgn_io_save(&dag, "model.ehdag");
 *   eh_hgn_builder_destroy(b);
 * ================================================================ */

#ifndef EH_HGN_BUILDER_H
#define EH_HGN_BUILDER_H

#include "eh_hgn_dag.h"
#include "../../include/core/eh_arena.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Builder Status Codes
 * ---------------------------------------------------------------- */
typedef enum {
    EH_HGN_BUILDER_OK           = 0,  /* Success */
    EH_HGN_BUILDER_ERR_OOM      = 1,  /* Out of memory */
    EH_HGN_BUILDER_ERR_OOB      = 2,  /* Index out of bounds */
    EH_HGN_BUILDER_ERR_FANOUT   = 3,  /* Exceeds max_fanout */
    EH_HGN_BUILDER_ERR_NULL     = 4,  /* NULL parameter */
    EH_HGN_BUILDER_ERR_DUP      = 5,  /* Duplicate edge */
    EH_HGN_BUILDER_ERR_STATE    = 6,  /* Invalid state */
} EH_HGN_BuilderStatus;

/* ----------------------------------------------------------------
 * Builder Handle (Opaque)
 * ---------------------------------------------------------------- */
typedef struct EH_HGN_Builder EH_HGN_Builder;

/* ----------------------------------------------------------------
 * API: Create/Destroy Builder
 * ---------------------------------------------------------------- */

/* Create builder for graph construction.
 * 
 * Parameters:
 *   vocab_size:  Number of nodes (tokens)
 *   max_fanout:  Maximum outgoing edges per node
 * 
 * Returns:
 *   Builder handle on success
 *   NULL on failure (OOM or invalid parameters)
 * 
 * Notes:
 *   - Allocates with malloc (mutable during construction)
 *   - Must call eh_hgn_builder_destroy() to free
 */
EH_HGN_Builder *eh_hgn_builder_create(uint32_t vocab_size,
                                       uint32_t max_fanout);

/* Destroy builder and free all resources.
 * 
 * Safe to call with NULL builder.
 * After destroy, builder handle is invalid.
 */
void eh_hgn_builder_destroy(EH_HGN_Builder *builder);

/* ----------------------------------------------------------------
 * API: Set Node Embeddings
 * ---------------------------------------------------------------- */

/* Set embedding vector for a node.
 * 
 * Parameters:
 *   builder:   Builder handle
 *   token_id:  Node ID [0, vocab_size)
 *   embedding: Float vector of size EH_HGN_EMBED_DIM (128)
 * 
 * Returns:
 *   EH_HGN_BUILDER_OK on success
 *   Error code on failure
 * 
 * Notes:
 *   - Copies embedding data (caller retains ownership)
 *   - If not set, embedding defaults to zeros
 */
EH_HGN_BuilderStatus eh_hgn_builder_set_embedding(
    EH_HGN_Builder *builder,
    uint32_t token_id,
    const float *embedding
);

/* ----------------------------------------------------------------
 * API: Add Edges
 * ---------------------------------------------------------------- */

/* Add directed edge: src → dst.
 * 
 * Parameters:
 *   builder: Builder handle
 *   src:     Source node ID [0, vocab_size)
 *   dst:     Destination node ID [0, vocab_size)
 *   prior:   Prior probability (heuristic score)
 *   weight:  Weight vector of size EH_HGN_EMBED_DIM (128)
 * 
 * Returns:
 *   EH_HGN_BUILDER_OK on success
 *   EH_HGN_BUILDER_ERR_OOB if src/dst out of bounds
 *   EH_HGN_BUILDER_ERR_FANOUT if src exceeds max_fanout
 *   EH_HGN_BUILDER_ERR_DUP if edge already exists
 *   EH_HGN_BUILDER_ERR_NULL if builder/weight is NULL
 * 
 * Notes:
 *   - Copies weight data (caller retains ownership)
 *   - Duplicate edges (same src, dst) are rejected
 *   - Self-loops (src == dst) are allowed
 */
EH_HGN_BuilderStatus eh_hgn_builder_add_edge(
    EH_HGN_Builder *builder,
    uint32_t src,
    uint32_t dst,
    float prior,
    const float *weight
);

/* ----------------------------------------------------------------
 * API: Finalize Builder → BaseDag
 * ---------------------------------------------------------------- */

/* Finalize builder and convert to immutable BaseDag.
 * 
 * Parameters:
 *   builder: Builder handle
 *   arena:   Arena allocator for BaseDag memory
 *   dag:     Output BaseDag structure
 * 
 * Returns:
 *   EH_HGN_BUILDER_OK on success
 *   Error code on failure
 * 
 * Process:
 *   1. Constructs CSR (Compressed Sparse Row) format
 *   2. Allocates from arena (zero-copy)
 *   3. Populates dag structure
 * 
 * Notes:
 *   - Arena must have sufficient capacity
 *   - After finalize, builder is still valid (can finalize again)
 *   - dag points to arena memory (lifetime tied to arena)
 */
EH_HGN_BuilderStatus eh_hgn_builder_finalize(
    EH_HGN_Builder *builder,
    EH_Arena *arena,
    EH_HGN_BaseDag *dag
);

/* ----------------------------------------------------------------
 * API: Query Builder State
 * ---------------------------------------------------------------- */

/* Get number of edges added so far.
 * 
 * Returns:
 *   Total edge count
 *   0 if builder is NULL
 */
uint32_t eh_hgn_builder_edge_count(const EH_HGN_Builder *builder);

/* Get number of edges for a specific node.
 * 
 * Parameters:
 *   builder:  Builder handle
 *   token_id: Node ID [0, vocab_size)
 * 
 * Returns:
 *   Edge count for node
 *   0 if token_id out of bounds or builder is NULL
 */
uint32_t eh_hgn_builder_node_fanout(const EH_HGN_Builder *builder,
                                     uint32_t token_id);

/* Check if embedding is set for a node.
 * 
 * Returns:
 *   true if embedding was set
 *   false otherwise
 */
bool eh_hgn_builder_has_embedding(const EH_HGN_Builder *builder,
                                   uint32_t token_id);

/* ----------------------------------------------------------------
 * Utilities: Error Messages
 * ---------------------------------------------------------------- */

/* Get human-readable error message for status code. */
const char *eh_hgn_builder_strerror(EH_HGN_BuilderStatus status);

#ifdef __cplusplus
}
#endif

#endif /* EH_HGN_BUILDER_H */
