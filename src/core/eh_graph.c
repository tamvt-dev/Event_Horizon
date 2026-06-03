/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_graph.c
 */

#include "../../include/core/eh_graph.h"
#include <string.h>

EH_Graph* eh_graph_create(EH_Arena *node_arena, EH_Arena *edge_arena, uint32_t max_nodes, uint32_t max_edges) {
    /* Allocate the graph header directly on the Node Arena for centralized management */
    EH_Graph *graph = (EH_Graph*)eh_arena_alloc(node_arena, sizeof(EH_Graph));
    if (!graph) return NULL;

    graph->node_arena = node_arena;
    graph->edge_arena = edge_arena;

    /* Commit contiguous flat arrays from the Arena */
    graph->nodes = (EH_GraphNode*)eh_arena_alloc(node_arena, max_nodes * sizeof(EH_GraphNode));
    graph->edges = (EH_GraphEdge*)eh_arena_alloc(edge_arena, max_edges * sizeof(EH_GraphEdge));

    if (!graph->nodes || !graph->edges) return NULL;

    graph->node_count    = 0;
    graph->edge_count    = 0;
    graph->node_capacity = max_nodes;
    graph->edge_capacity = max_edges;

    return graph;
}

uint32_t eh_graph_add_node(EH_Graph *graph, uint32_t node_id, float score) {
    if (graph->node_count >= graph->node_capacity) return EH_INVALID_IDX;

    uint32_t     current_idx = graph->node_count++;
    EH_GraphNode *node       = &graph->nodes[current_idx];

    node->node_id             = node_id;
    node->probabilistic_score = score;
    node->first_edge_idx      = EH_INVALID_IDX; /* No adjacent edges yet */
    node->edge_count          = 0;
    node->flags               = 0;

    return current_idx;
}

bool eh_graph_add_edge(EH_Graph *graph, uint32_t src_idx, uint32_t dest_idx, float weight) {
    if (src_idx >= graph->node_count || dest_idx >= graph->node_count) return false;
    if (graph->edge_count >= graph->edge_capacity) return false;

    uint32_t     new_edge_idx = graph->edge_count++;
    EH_GraphEdge *edge        = &graph->edges[new_edge_idx];

    edge->target_node_idx = dest_idx;
    edge->weight          = weight;

    /*
     * Prepend the new edge to the source node's flat linked-list.
     * O(1) insertion that maintains a singly-linked edge chain per node.
     */
    EH_GraphNode *src_node    = &graph->nodes[src_idx];
    edge->next_edge_idx       = src_node->first_edge_idx;
    src_node->first_edge_idx  = new_edge_idx;
    src_node->edge_count++;

    return true;
}

void eh_graph_traverse_neighbors(const EH_Graph *graph, uint32_t node_idx, void (*callback)(uint32_t src, uint32_t dest, float combined_score)) {
    if (node_idx >= graph->node_count) return;

    const EH_GraphNode *node           = &graph->nodes[node_idx];
    uint32_t            current_edge_idx = node->first_edge_idx;

    /*
     * Linear scan over a contiguous edge array — CPU hardware prefetch
     * maximally efficient due to sequential memory access pattern.
     */
    while (current_edge_idx != EH_INVALID_IDX) {
        const EH_GraphEdge *edge = &graph->edges[current_edge_idx];

        float combined_score = node->probabilistic_score * edge->weight;
        if (callback) {
            callback(node_idx, edge->target_node_idx, combined_score);
        }

        current_edge_idx = edge->next_edge_idx;
    }
}