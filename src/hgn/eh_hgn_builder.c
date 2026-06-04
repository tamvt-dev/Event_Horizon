/* ================================================================
 * eh_hgn_builder.c — HGN Builder implementation
 * ================================================================ */

#include "../../include/hgn/eh_hgn_builder.h"
#include "../../include/hgn/eh_hgn_io.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * Internal Edge Structure (Dynamic List)
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t dst;
    float prior;
    float weight[EH_HGN_EMBED_DIM];
} BuilderEdge;

typedef struct {
    BuilderEdge *edges;   /* Dynamic array */
    uint32_t count;       /* Current edge count */
    uint32_t capacity;    /* Allocated capacity */
} BuilderNodeEdges;

/* ----------------------------------------------------------------
 * Builder Structure
 * ---------------------------------------------------------------- */
struct EH_HGN_Builder {
    uint32_t vocab_size;
    uint32_t max_fanout;
    
    /* Node embeddings: [vocab_size][128] */
    float (*embeddings)[EH_HGN_EMBED_DIM];
    bool *has_embedding;  /* Track which embeddings are set */
    
    /* Edges per node */
    BuilderNodeEdges *nodes;
    
    /* Total edge count */
    uint32_t total_edges;
};

/* ----------------------------------------------------------------
 * eh_hgn_builder_create
 * ---------------------------------------------------------------- */
EH_HGN_Builder *eh_hgn_builder_create(uint32_t vocab_size,
                                       uint32_t max_fanout)
{
    if (vocab_size == 0 || vocab_size > EH_HGN_VOCAB_SIZE ||
        max_fanout == 0 || max_fanout > EH_HGN_MAX_FANOUT)
    {
        fprintf(stderr, "[eh_hgn_builder] Invalid params: vocab=%u, fanout=%u\n",
                vocab_size, max_fanout);
        return NULL;
    }

    EH_HGN_Builder *b = calloc(1, sizeof(*b));
    if (!b) return NULL;

    b->vocab_size = vocab_size;
    b->max_fanout = max_fanout;
    b->total_edges = 0;

    /* Allocate embeddings */
    b->embeddings = calloc(vocab_size, sizeof(float[EH_HGN_EMBED_DIM]));
    b->has_embedding = calloc(vocab_size, sizeof(bool));
    
    /* Allocate node edges */
    b->nodes = calloc(vocab_size, sizeof(BuilderNodeEdges));

    if (!b->embeddings || !b->has_embedding || !b->nodes) {
        eh_hgn_builder_destroy(b);
        return NULL;
    }

    return b;
}

/* ----------------------------------------------------------------
 * eh_hgn_builder_destroy
 * ---------------------------------------------------------------- */
void eh_hgn_builder_destroy(EH_HGN_Builder *builder)
{
    if (!builder) return;

    /* Free edge arrays for each node */
    if (builder->nodes) {
        for (uint32_t i = 0; i < builder->vocab_size; i++) {
            free(builder->nodes[i].edges);
        }
        free(builder->nodes);
    }

    free(builder->embeddings);
    free(builder->has_embedding);
    free(builder);
}

/* ----------------------------------------------------------------
 * eh_hgn_builder_set_embedding
 * ---------------------------------------------------------------- */
EH_HGN_BuilderStatus eh_hgn_builder_set_embedding(
    EH_HGN_Builder *builder,
    uint32_t token_id,
    const float *embedding)
{
    if (!builder || !embedding) return EH_HGN_BUILDER_ERR_NULL;
    if (token_id >= builder->vocab_size) return EH_HGN_BUILDER_ERR_OOB;

    memcpy(builder->embeddings[token_id], embedding, 
           EH_HGN_EMBED_DIM * sizeof(float));
    builder->has_embedding[token_id] = true;

    return EH_HGN_BUILDER_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_builder_add_edge
 * ---------------------------------------------------------------- */
EH_HGN_BuilderStatus eh_hgn_builder_add_edge(
    EH_HGN_Builder *builder,
    uint32_t src,
    uint32_t dst,
    float prior,
    const float *weight)
{
    if (!builder || !weight) return EH_HGN_BUILDER_ERR_NULL;
    if (src >= builder->vocab_size || dst >= builder->vocab_size) {
        return EH_HGN_BUILDER_ERR_OOB;
    }

    BuilderNodeEdges *node = &builder->nodes[src];

    /* Check fanout limit */
    if (node->count >= builder->max_fanout) {
        return EH_HGN_BUILDER_ERR_FANOUT;
    }

    /* Check for duplicate edge */
    for (uint32_t i = 0; i < node->count; i++) {
        if (node->edges[i].dst == dst) {
            return EH_HGN_BUILDER_ERR_DUP;
        }
    }

    /* Grow edge array if needed */
    if (node->count >= node->capacity) {
        uint32_t new_cap = (node->capacity == 0) ? 4 : (node->capacity * 2);
        if (new_cap > builder->max_fanout) new_cap = builder->max_fanout;

        BuilderEdge *new_edges = realloc(node->edges, 
                                         new_cap * sizeof(BuilderEdge));
        if (!new_edges) return EH_HGN_BUILDER_ERR_OOM;

        node->edges = new_edges;
        node->capacity = new_cap;
    }

    /* Add edge */
    BuilderEdge *e = &node->edges[node->count];
    e->dst = dst;
    e->prior = prior;
    memcpy(e->weight, weight, EH_HGN_EMBED_DIM * sizeof(float));

    node->count++;
    builder->total_edges++;

    return EH_HGN_BUILDER_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_builder_finalize
 * ---------------------------------------------------------------- */
EH_HGN_BuilderStatus eh_hgn_builder_finalize(
    EH_HGN_Builder *builder,
    EH_Arena *arena,
    EH_HGN_BaseDag *dag)
{
    if (!builder || !arena || !dag) return EH_HGN_BUILDER_ERR_NULL;

    uint32_t V = builder->vocab_size;
    uint32_t E = builder->total_edges;

    /* Calculate sizes */
    size_t sz_embed   = (size_t)V * sizeof(EH_HGN_NodeEmbed);
    size_t sz_adj     = (size_t)V * sizeof(EH_HGN_NodeAdj);
    size_t sz_compact = (E > 0) ? ((size_t)E * sizeof(EH_HGN_EdgeCompact)) : 1;
    size_t sz_weight  = (E > 0) ? ((size_t)E * sizeof(EH_HGN_EdgeWeight)) : 1;

    /* Allocate from arena */
    EH_HGN_NodeEmbed   *node_embed   = eh_arena_alloc(arena, sz_embed);
    EH_HGN_NodeAdj     *node_adj     = eh_arena_alloc(arena, sz_adj);
    EH_HGN_EdgeCompact *edge_compact = (E > 0) ? eh_arena_alloc(arena, sz_compact) : NULL;
    EH_HGN_EdgeWeight  *weight_pool  = (E > 0) ? eh_arena_alloc(arena, sz_weight) : NULL;

    if (!node_embed || !node_adj) {
        return EH_HGN_BUILDER_ERR_OOM;
    }
    
    /* For E > 0, check edge allocations */
    if (E > 0 && (!edge_compact || !weight_pool)) {
        return EH_HGN_BUILDER_ERR_OOM;
    }

    /* Populate node embeddings */
    for (uint32_t i = 0; i < V; i++) {
        memcpy(node_embed[i].vec, builder->embeddings[i],
               EH_HGN_EMBED_DIM * sizeof(float));
    }

    /* Build CSR: node_adj and edge arrays */
    uint32_t edge_offset = 0;
    for (uint32_t i = 0; i < V; i++) {
        BuilderNodeEdges *n = &builder->nodes[i];

        node_adj[i].edge_offset = edge_offset;
        node_adj[i].edge_count  = n->count;

        /* Copy edges for this node */
        for (uint32_t j = 0; j < n->count; j++) {
            BuilderEdge *src_edge = &n->edges[j];
            uint32_t idx = edge_offset + j;

            edge_compact[idx].dst        = src_edge->dst;
            edge_compact[idx].prior      = src_edge->prior;
            edge_compact[idx].weight_idx = idx;

            memcpy(weight_pool[idx].vec, src_edge->weight,
                   EH_HGN_EMBED_DIM * sizeof(float));
        }

        edge_offset += n->count;
    }

    /* Populate dag */
    dag->vocab_size   = V;
    dag->total_edges  = E;
    dag->embed_dim    = EH_HGN_EMBED_DIM;
    dag->max_fanout   = builder->max_fanout;
    dag->node_embed   = node_embed;
    dag->node_adj     = node_adj;
    dag->edge_compact = edge_compact;
    dag->weight_pool  = weight_pool;

    return EH_HGN_BUILDER_OK;
}

/* ----------------------------------------------------------------
 * Query Functions
 * ---------------------------------------------------------------- */
uint32_t eh_hgn_builder_edge_count(const EH_HGN_Builder *builder)
{
    return builder ? builder->total_edges : 0;
}

uint32_t eh_hgn_builder_node_fanout(const EH_HGN_Builder *builder,
                                     uint32_t token_id)
{
    if (!builder || token_id >= builder->vocab_size) return 0;
    return builder->nodes[token_id].count;
}

bool eh_hgn_builder_has_embedding(const EH_HGN_Builder *builder,
                                   uint32_t token_id)
{
    if (!builder || token_id >= builder->vocab_size) return false;
    return builder->has_embedding[token_id];
}

/* ----------------------------------------------------------------
 * eh_hgn_builder_strerror
 * ---------------------------------------------------------------- */
const char *eh_hgn_builder_strerror(EH_HGN_BuilderStatus status)
{
    switch (status) {
        case EH_HGN_BUILDER_OK:          return "Success";
        case EH_HGN_BUILDER_ERR_OOM:     return "Out of memory";
        case EH_HGN_BUILDER_ERR_OOB:     return "Index out of bounds";
        case EH_HGN_BUILDER_ERR_FANOUT:  return "Exceeds max fanout";
        case EH_HGN_BUILDER_ERR_NULL:    return "NULL parameter";
        case EH_HGN_BUILDER_ERR_DUP:     return "Duplicate edge";
        case EH_HGN_BUILDER_ERR_STATE:   return "Invalid state";
        default:                         return "Unknown error";
    }
}
