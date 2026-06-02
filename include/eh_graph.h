/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_graph.h
 * Description: Cache-optimized flat graph memory layout system.
 */

#ifndef EH_GRAPH_H
#define EH_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "eh_arena.h"

#define EH_INVALID_IDX 0xFFFFFFFF

/* Flat node structure — exactly 16 bytes (cache-line aligned) */
typedef struct {
    uint32_t node_id;            /* Token or state identifier                        */
    float    probabilistic_score;/* Probabilistic score from the neural circuit      */
    uint32_t first_edge_idx;     /* Index into the flat Edge Array                   */
    uint16_t edge_count;         /* Out-degree (number of outgoing edges)            */
    uint16_t flags;              /* State flags (Visited, Active, etc.)              */
} EH_GraphNode;

/* Flat edge structure — exactly 16 bytes (padded for alignment) */
typedef struct {
    uint32_t target_node_idx;  /* Index of the destination node in the Node Array  */
    float    weight;           /* Transition probability weight                     */
    uint32_t next_edge_idx;    /* Index of the next edge (flat linked-list)         */
    uint32_t padding;          /* Preserves 16-byte alignment for CPU cache lines  */
} EH_GraphEdge;

/* Arena-integrated graph manager */
typedef struct {
    EH_Arena *node_arena;
    EH_Arena *edge_arena;

    EH_GraphNode *nodes;       /* Direct pointer into node_arena memory */
    EH_GraphEdge *edges;       /* Direct pointer into edge_arena memory */

    uint32_t node_count;
    uint32_t edge_count;

    uint32_t node_capacity;
    uint32_t edge_capacity;
} EH_Graph;

/* --- GRAPH SYSTEM API --- */
EH_Graph* eh_graph_create(EH_Arena *node_arena, EH_Arena *edge_arena, uint32_t max_nodes, uint32_t max_edges);
uint32_t  eh_graph_add_node(EH_Graph *graph, uint32_t node_id, float score);
bool      eh_graph_add_edge(EH_Graph *graph, uint32_t src_idx, uint32_t dest_idx, float weight);
void      eh_graph_traverse_neighbors(const EH_Graph *graph, uint32_t node_idx, void (*callback)(uint32_t src, uint32_t dest, float combined_score));

#endif /* EH_GRAPH_H */