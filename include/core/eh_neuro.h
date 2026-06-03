/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_neuro.h
 * Description: Neuroplasticity mechanism — self-mutating DAG during inference.
 *
 * Philosophy:
 *   The biological brain does not "freeze" after learning. Synapses are
 *   strengthened or weakened with each signal. Direction 2 simulates this:
 *   when a beam path receives "large deviation" feedback, the system does
 *   not merely record it — it physically materializes that error into a
 *   new Mutation Node in the DAG, while simultaneously updating routing
 *   weights so that future beams learn to avoid or traverse the new node.
 *
 * Mutation Node lifecycle:
 *   1. DETECT:  Beam path completes; feedback score < error_threshold.
 *   2. CREATE:  Spawn a MutationNode from the Arena (no malloc).
 *   3. CONNECT: Attach MutationNode to the base DAG (parent → mutant).
 *   4. LEARN:   Update routing weights to bias beams toward the mutant.
 *   5. PERSIST: MutationNode survives in the DAG for subsequent inferences.
 *   6. EVICT:   Arena reset purges all mutants after a session ends.
 *
 * Learning parameters:
 *   error_threshold: if |output - target| > threshold → trigger mutation
 *   learning_rate:   routing weight update step size (gradient step)
 *   mutation_rate:   probability of spawning a mutant (prevents node explosion)
 */

#ifndef EH_NEURO_H
#define EH_NEURO_H

#include <stdbool.h>
#include "eh_dag.h"
#include "eh_arena.h"
#include "eh_scoring.h"

/* =================================================================
 * NEUROPLASTICITY CONSTANTS
 * ================================================================= */
#define EH_NEURO_ERROR_THRESHOLD  0.15f  /* Minimum deviation to trigger mutation  */
#define EH_NEURO_LEARNING_RATE    0.01f  /* Routing weight update learning rate    */
#define EH_NEURO_MUTATION_RATE    0.80f  /* Probability of spawning a mutant       */
#define EH_NEURO_MUTANT_ID_BASE   100    /* node_id namespace for mutants          */
#define EH_NEURO_MAX_MUTANTS      64     /* Maximum mutants per session            */

/* =================================================================
 * MUTANT STATUS
 * ================================================================= */
typedef enum {
    EH_MUTANT_HEALTHY  = 0,  /* Actively participating in routing        */
    EH_MUTANT_PRUNED   = 1,  /* Eliminated (consistently low score)      */
    EH_MUTANT_FROZEN   = 2   /* No longer learning but still routed      */
} EH_MutantStatus;

/* =================================================================
 * MUTATION NODE METADATA
 * Stores supplementary information about a mutation node, including
 * its parent, error history, and current status.
 * ================================================================= */
typedef struct {
    EH_DAGNode    *node;              /* Pointer to the node in the Arena          */
    EH_DAGNode    *parent;            /* Parent node that spawned this mutant      */
    int            generation;        /* Generation depth (0=root, 1=lv1 mutant)  */
    float          error_at_birth;    /* Error magnitude at the time of creation  */
    float          cumulative_error;  /* Total accumulated error across inferences*/
    int            activation_count;  /* Number of times a beam has passed through*/
    EH_MutantStatus status;
} EH_MutantRecord;

/* =================================================================
 * NEURO CONTEXT — MANAGES THE ENTIRE NEUROPLASTICITY STATE
 * ================================================================= */
typedef struct {
    EH_Arena       *arena;                          /* Arena backing mutant memory  */
    EH_MutantRecord mutants[EH_NEURO_MAX_MUTANTS];  /* Mutant registry              */
    int             mutant_count;                   /* Current live mutant count    */
    int             next_node_id;                   /* Auto-incrementing ID counter */
    float           error_threshold;
    float           learning_rate;
    float           mutation_rate;
    int             total_mutations;  /* Cumulative mutation count (stats) */
    int             total_pruned;     /* Cumulative pruned count   (stats) */
} EH_NeuroContext;

/* =================================================================
 * FEEDBACK RESULT
 * ================================================================= */
typedef struct {
    float error_magnitude;   /* Average |output - target|                  */
    bool  mutation_fired;    /* Whether a mutant was spawned               */
    int   mutant_id;         /* node_id of the new mutant (-1 if none)     */
    bool  weights_updated;   /* Whether routing weights were updated       */
    float weight_delta;      /* Largest weight delta applied               */
} EH_FeedbackResult;

/* =================================================================
 * PUBLIC API
 * ================================================================= */

/*
 * Initialize a NeuroContext with an Arena and learning parameters.
 */
EH_NeuroContext *eh_neuro_init(EH_Arena *arena,
                               float     error_threshold,
                               float     learning_rate,
                               float     mutation_rate);

/*
 * Process feedback after an inference pass.
 *
 * parent_node  : the last node of the beam path under evaluation
 * output_vec   : actual inference output vector
 * target_vec   : expected output (ground truth or proxy)
 * vec_dim      : dimensionality of both vectors
 * scorer       : Scoring Core to update routing weights
 * input_vector : original input (used to compute routing gradient)
 * input_dim    : dimensionality of the input
 *
 * Processing flow:
 *   1. Compute error = ||output - target|| / dim
 *   2. If error < threshold → do nothing, return negative result
 *   3. Parametric Learning: update routing_weights via gradient
 *   4. Structural Mutation: if error > threshold AND rand < mutation_rate
 *      → spawn MutationNode from Arena, attach to parent_node
 *   5. Record MutantRecord
 */
EH_FeedbackResult eh_neuro_process_feedback(
    EH_NeuroContext *nctx,
    EH_DAGNode      *parent_node,
    const float     *output_vec,
    const float     *target_vec,
    int              vec_dim,
    EH_ScoringCore  *scorer,
    const float     *input_vector,
    int              input_dim);

/*
 * Disconnect and mark low-performing mutants for pruning.
 * Call periodically to prevent unbounded DAG growth.
 * prune_threshold: mutants with cumulative_error/activation > threshold are pruned.
 */
int eh_neuro_prune(EH_NeuroContext *nctx, float prune_threshold);

/*
 * Print a full status report of the mutant registry.
 */
void eh_neuro_report(const EH_NeuroContext *nctx);

/*
 * Safely disconnect all mutants from the DAG before shutdown.
 * MUST be called BEFORE eh_arena_destroy() to prevent dangling pointers
 * in base DAG nodes that point into Arena memory.
 */
void eh_neuro_disconnect_all(EH_NeuroContext *nctx);

/*
 * Free the NeuroContext (does NOT free the Arena — Arena has its own lifecycle).
 */
void eh_neuro_free(EH_NeuroContext *nctx);

#endif /* EH_NEURO_H */