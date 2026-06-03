/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_learning.h
 * Description: API for online learning and DAG structural mutation management.
 */

#ifndef EH_LEARNING_H
#define EH_LEARNING_H

#include "eh_dag.h"
#include "eh_scoring.h"
#include "eh_engine.h"
#include <stdint.h>

/* Memory Arena for static allocation of mutation nodes */
typedef struct {
    uint8_t *buffer;    /* Pre-allocated flat memory region */
    size_t   capacity;  /* Maximum capacity (bytes)         */
    size_t   offset;    /* Current allocation offset        */
} EH_MemoryArena;

/* EventHorizon learning context */
typedef struct {
    EH_MemoryArena *arena;           /* Arena managing dynamic node memory     */
    float           learning_rate;   /* Learning rate η for the Scoring Core   */
    float           mutation_thresh; /* Error threshold that triggers mutation  */
} EH_LearningContext;

/* =================================================================
 * PUBLIC API
 * ================================================================= */

/* Initialize the flat memory arena and the learning context */
EH_LearningContext *eh_learning_init(size_t arena_size, float lr, float mutation_thresh);

/*
 * Parameter Learning: update the Scoring Core via the Delta Rule.
 * feedback_signal: > 0 for a good result (reward), < 0 for a wrong result (penalty).
 */
void eh_learning_reinforce_path(const EH_LearningContext *lctx,
                                EH_ScoringCore           *score_core,
                                const float              *input_vector,
                                const EH_BeamPath        *chosen_path,
                                float                     feedback_signal);

/*
 * Structural Learning: mutate a new node and restructure DAG topology.
 * If error_signal > mutation_thresh, the system branches into new knowledge.
 */
bool eh_learning_mutate_graph(EH_LearningContext *lctx,
                              EH_DAGNode         *parent_node,
                              const float        *error_vector,
                              int                 dim);

/* Release the learning context resources */
void eh_learning_free(EH_LearningContext *lctx);

#endif /* EH_LEARNING_H */