/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_engine.h
 * Description: Engine context definition and primary inference API.
 *              This is the highest-level orchestration layer, combining
 *              the DAG and Scoring Core to execute Beam Search.
 */

#ifndef EH_ENGINE_H
#define EH_ENGINE_H

#include <stdint.h>
#include "eh_dag.h"
#include "eh_scoring.h"
#include "eh_dynamic.h"

/* =========================================================
 * BEAM SEARCH CONSTANTS
 * ========================================================= */
#define EH_BEAM_WIDTH       2   /* Number of beams retained per step         */
#define EH_MAX_BEAM_DEPTH  16   /* Maximum depth of a beam path              */
#define EH_MAX_CANDIDATES   4   /* Maximum child nodes scanned per step      */

/* =========================================================
 * BEAM PATH STRUCTURE
 * Represents a single path through the DAG space being
 * tracked concurrently by the Beam Search algorithm.
 * ========================================================= */
typedef struct {
    EH_DAGNode *nodes[EH_MAX_BEAM_DEPTH]; /* Sequence of visited nodes    */
    int         depth;                    /* Current path depth           */
    float       cumulative_score;         /* Total accumulated score      */
} EH_BeamPath;

/* =========================================================
 * ENGINE PROFILING & STATISTICS
 * ========================================================= */
typedef struct {
    uint64_t total_inferences;
    uint64_t total_nodes_evaluated;       /* Total nodes activated */
    uint64_t collapsed_nodes_evaluated;   /* Nodes processed in collapsed O(N) mode */
    uint64_t dense_nodes_evaluated;       /* Nodes processed in dense O(N^2) mode */
    uint64_t total_flops_dense_expected;   /* FLOP count if all activated nodes were dense */
    uint64_t total_flops_actual;           /* Actual FLOP count executed */
    uint64_t beam_nodes_scanned;           /* Total child branches evaluated by Scoring Core */
    uint64_t beam_nodes_pruned;            /* Total child branches pruned during beam selection */
} EH_EngineStats;

/* =========================================================
 * ENGINE CONTEXT STRUCTURE
 * Holds the complete state of a single inference session.
 * ========================================================= */
typedef struct {
    EH_DAGNode        *root_node;      /* Root node of the DAG            */
    EH_ScoringCore    *router_core;    /* Initialized routing core        */
    float              critical_limit; /* R_I collapse threshold for DAG  */
    EH_DynamicContext *dynamic_ctx;    /* Dynamic collapse context        */
    EH_EngineStats     stats;          /* Performance & profiling stats   */
} EH_Context;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/*
 * Initialize the engine: bind the DAG root and Scoring Core into a Context.
 * Automatically traverses the entire DAG and evaluates collapse for each node.
 * Returns: new EH_Context pointer, or NULL on failure.
 */
EH_Context *eh_engine_setup(EH_DAGNode    *root,
                             EH_ScoringCore *core,
                             float          critical_limit);

/*
 * Initialize the engine with Dynamic Collapse mode enabled.
 */
EH_Context *eh_engine_setup_dynamic(EH_DAGNode    *root,
                                    EH_ScoringCore *core,
                                    float          critical_limit,
                                    float          low_thresh,
                                    float          high_thresh);

/*
 * Execute a single inference pass on the engine.
 *
 * input_vector  : input vector of size [input_dim]
 * input_dim     : dimensionality of the input vector
 * output_vector : output buffer allocated by the caller, size [output_dim]
 * output_dim    : desired dimensionality of the output vector
 *
 * Processing flow:
 *   1. Beam Search traverses the DAG; EH_BEAM_WIDTH best beams are retained.
 *   2. Each node is activated according to its is_collapsed state:
 *      - Collapsed  → O(N)   fill with distribution_mean
 *      - Not collapsed → O(N^2) dense matrix multiply
 *   3. The best final beam writes its result to output_vector.
 *
 * Returns: true if inference succeeded.
 */
bool eh_engine_inference(const EH_Context *ctx,
                         const float      *input_vector,
                         int               input_dim,
                         float            *output_vector,
                         int               output_dim);

/*
 * Shut down the engine and release all associated resources.
 * Includes: DAG graph, Scoring Core, and the Context struct itself.
 */
void eh_engine_shutdown(EH_Context *ctx);

/*
 * Reset all profiling stats counters inside the engine context.
 */
void eh_engine_reset_stats(EH_Context *ctx);

/*
 * Retrieve a copy of the current profiling statistics.
 */
EH_EngineStats eh_engine_get_stats(const EH_Context *ctx);

#endif /* EH_ENGINE_H */
