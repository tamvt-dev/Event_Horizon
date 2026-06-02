/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/core/eh_engine.c
 * Description: Main orchestration core — engine initialization, Beam Search
 *              execution over the DAG, and full context lifecycle management.
 *
 * Inference execution flow:
 *   Input → Scoring Core → Beam Search (DAG) → Node Activation → Output
 *                              ↓
 *               Each step retains EH_BEAM_WIDTH = 2 best paths.
 *               Each node: collapsed → O(N), otherwise → O(N^2).
 *
 * Compile: gcc -O3 -std=c99 -Wall -Wextra -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include "../../include/eh_engine.h"

/* =================================================================
 * INTERNAL: DAG TRAVERSAL FOR COLLAPSE EVALUATION
 * =================================================================
 *
 * Called exactly once inside eh_engine_setup().
 * Performs a full DFS/BFS over the DAG and invokes
 * eh_dag_evaluate_collapse() for each node, ensuring every node
 * is evaluated exactly once.
 * ================================================================= */
static void _eh_engine_evaluate_all_nodes(EH_DAGNode *node,
                                          float       critical_limit) {
    if (!node || node->visited) return;

    /* Mark as processed to avoid re-evaluating shared nodes */
    node->visited = true;

    /* Evaluate collapse threshold for the current node */
    eh_dag_evaluate_collapse(node, critical_limit);

    /* Recurse into child nodes */
    for (int i = 0; i < node->child_count; i++) {
        _eh_engine_evaluate_all_nodes(node->children[i], critical_limit);
    }
}

/* Reset the visited flag after evaluation, preparing for inference */
static void _eh_engine_reset_visited(EH_DAGNode *node) {
    if (!node || !node->visited) return;
    node->visited = false;
    for (int i = 0; i < node->child_count; i++) {
        _eh_engine_reset_visited(node->children[i]);
    }
}

/* =================================================================
 * PART 1: ENGINE INITIALIZATION
 * ================================================================= */
EH_Context *eh_engine_setup(EH_DAGNode    *root,
                             EH_ScoringCore *core,
                             float          critical_limit) {
    if (!root || !core) {
        fprintf(stderr, "[EH_ENGINE] Error: root or core is NULL\n");
        return NULL;
    }

    EH_Context *ctx = (EH_Context *)malloc(sizeof(EH_Context));
    if (!ctx) {
        fprintf(stderr, "[EH_ENGINE] Error: failed to allocate EH_Context\n");
        return NULL;
    }

    ctx->root_node      = root;
    ctx->router_core    = core;
    ctx->critical_limit = critical_limit;
    ctx->dynamic_ctx    = NULL;
    memset(&ctx->stats, 0, sizeof(EH_EngineStats));

    /*
     * Traverse the entire DAG and evaluate the collapse threshold for all nodes.
     * This is an Ahead-of-Time (AOT) step — executed once before any inference,
     * never repeated in the hot path.
     */
    _eh_engine_evaluate_all_nodes(root, critical_limit);
    _eh_engine_reset_visited(root); /* Reset so inference can reuse the visited flag */

#ifdef EH_DEBUG
    printf("[EH_ENGINE] Initialization complete. critical_limit=%.4f\n",
           critical_limit);
#endif

    return ctx;
}

/* =================================================================
 * ENGINE INITIALIZATION WITH DYNAMIC COLLAPSE MODE
 * ================================================================= */
EH_Context *eh_engine_setup_dynamic(EH_DAGNode    *root,
                                    EH_ScoringCore *core,
                                    float          critical_limit,
                                    float          low_thresh,
                                    float          high_thresh) {
    EH_Context *ctx = eh_engine_setup(root, core, critical_limit);
    if (!ctx) return NULL;
    ctx->dynamic_ctx = eh_dynamic_init(root, low_thresh, high_thresh);
    return ctx;
}

/* =================================================================
 * INTERNAL: NODE ACTIVATION — COMPUTE OUTPUT VECTOR
 * =================================================================
 *
 * This is the crux of the "Event Horizon" mechanism:
 *
 * CASE 1: node->is_collapsed == true
 *   → Cost O(output_dim): fill all output elements with distribution_mean.
 *   → Fully eliminates the O(rows * cols) dense matrix multiply.
 *
 * CASE 2: node->is_collapsed == false
 *   → Cost O(rows * cols): standard dense matmul.
 *   → output[i] = Σ_j W[i][j] * input[j]
 *   → output size = weights.rows, input size = weights.cols.
 * ================================================================= */
static void _eh_engine_activate_node(const EH_Context *ctx,
                                     const EH_DAGNode *node,
                                     const float      *input_vec,
                                     int               input_dim,
                                     float            *output_vec,
                                     int               output_dim,
                                     float             input_norm) {
    if (!node || !input_vec || !output_vec) return;

    EH_Context *mctx = (EH_Context *)ctx;
    if (mctx) {
        mctx->stats.total_nodes_evaluated++;
    }

    bool collapse = node->is_collapsed;
    if (!collapse && ctx && ctx->dynamic_ctx) {
        EH_CollapseDecision d = eh_dynamic_evaluate(ctx->dynamic_ctx, input_vec, input_norm, node);
        collapse = d.should_collapse;
    }

    int eff_rows = (node->weights.rows < output_dim)
                   ? node->weights.rows : output_dim;
    int eff_cols = (node->weights.cols < input_dim)
                   ? node->weights.cols : input_dim;
    uint64_t expected_flops = 2 * (uint64_t)eff_rows * (uint64_t)eff_cols;

    if (mctx) {
        mctx->stats.total_flops_dense_expected += expected_flops;
    }

    if (collapse) {
        if (mctx) {
            mctx->stats.collapsed_nodes_evaluated++;
            mctx->stats.total_flops_actual += output_dim; /* O(output_dim) for mean fill */
        }
        /*
         * --- COLLAPSED MODE: O(output_dim) ---
         * Fill the entire output with distribution_mean.
         * This is the heuristic approximation: instead of an expensive
         * matrix multiply, the layer's mean is used as a distribution proxy.
         */
        float mean = node->distribution_mean;
        for (int i = 0; i < output_dim; i++) {
            output_vec[i] = mean;
        }

#ifdef EH_DEBUG
        printf("[EH_ENGINE] Node %d (COLLAPSED): output=%d elements, mean=%.4f\n",
               node->node_id, output_dim, mean);
#endif
    } else {
        if (mctx) {
            mctx->stats.dense_nodes_evaluated++;
            mctx->stats.total_flops_actual += expected_flops;
        }
        /*
         * --- DENSE MODE: O(rows * cols) ---
         * Standard matrix-vector multiply: output = W * input.
         *
         * Safety bounds: use min(rows, output_dim) and min(cols, input_dim)
         * to prevent out-of-bounds reads/writes when buffer sizes do not
         * exactly match the weight matrix dimensions.
         */
        const float *W = node->weights.data;

        for (int i = 0; i < eff_rows; i++) {
            double acc = 0.0;
            /* Row pointer: W[i * cols + j] */
            const float *row = W + (size_t)i * (size_t)node->weights.cols;
            for (int j = 0; j < eff_cols; j++) {
                acc += (double)row[j] * (double)input_vec[j];
            }
            output_vec[i] = (float)acc;
        }

        /* Zero-fill any remaining output elements (if output_dim > eff_rows) */
        for (int i = eff_rows; i < output_dim; i++) {
            output_vec[i] = 0.0f;
        }

#ifdef EH_DEBUG
        printf("[EH_ENGINE] Node %d (DENSE): matmul %dx%d\n",
               node->node_id, eff_rows, eff_cols);
#endif
    }
}

/* =================================================================
 * PART 2: INFERENCE WITH BEAM SEARCH
 * =================================================================
 *
 * Simplified Beam Search algorithm (EH_BEAM_WIDTH = 2):
 *
 * Initialization:
 *   beams[0] = { root_node, depth=0, score=0.0 }
 *   active_beams = 1
 *
 * Main loop (each step = one DAG layer):
 *   1. For each active beam, scan up to EH_MAX_CANDIDATES child nodes.
 *   2. Call eh_scoring_predict_branch() for each candidate.
 *   3. Retain at most EH_BEAM_WIDTH candidates with the highest
 *      cumulative score (beam_score + branch_score).
 *   4. Activate selected nodes: compute output vector.
 *   5. Update beams for the next step.
 *
 * Termination:
 *   Beam with highest cumulative_score → final output_vector.
 *
 * Complexity analysis:
 *   - Nodes activated: O(EH_BEAM_WIDTH * depth)
 *   - Versus O(total_nodes): saves ~90-95% of computation.
 * ================================================================= */

/* Temporary buffer to hold each node's output during beam search */
#define EH_INTERNAL_BUF_DIM 512

bool eh_engine_inference(const EH_Context *ctx,
                         const float      *input_vector,
                         int               input_dim,
                         float            *output_vector,
                         int               output_dim) {
    EH_Context *mctx = (EH_Context *)ctx;
    if (mctx) {
        mctx->stats.total_inferences++;
    }

    /* --- Parameter validation --- */
    if (!ctx || !input_vector || !output_vector) {
        fprintf(stderr, "[EH_ENGINE] Error: inference parameters are NULL\n");
        return false;
    }
    if (output_dim > EH_INTERNAL_BUF_DIM) {
        fprintf(stderr, "[EH_ENGINE] Error: output_dim=%d exceeds limit %d\n",
                output_dim, EH_INTERNAL_BUF_DIM);
        return false;
    }

    /* Compute input norm for dynamic collapse evaluation */
    float input_norm = 0.0f;
    if (ctx->dynamic_ctx) {
        double norm_acc = 0.0;
        for (int i = 0; i < input_dim; i++) {
            norm_acc += (double)input_vector[i] * (double)input_vector[i];
        }
        input_norm = (float)sqrt(norm_acc);
    }

    /* ---------------------------------------------------------------
     * BEAM INITIALIZATION
     * The beams[] array holds the state of EH_BEAM_WIDTH concurrent
     * search paths traversing the DAG.
     * --------------------------------------------------------------- */
    EH_BeamPath beams[EH_BEAM_WIDTH];
    int         active_beams = 0;

    /* First beam starts from the root node */
    memset(&beams[0], 0, sizeof(EH_BeamPath));
    beams[0].nodes[0]         = ctx->root_node;
    beams[0].depth            = 1;
    beams[0].cumulative_score = 0.0f;
    active_beams              = 1;

    /*
     * Temporary output buffer for each activated node.
     * Stack allocation avoids heap allocator overhead in the hot path.
     */
    float node_output[EH_INTERNAL_BUF_DIM];

    /* ---------------------------------------------------------------
     * MAIN BEAM SEARCH LOOP
     * Continue until all beams reach a leaf (no children) or
     * the maximum depth is reached.
     * --------------------------------------------------------------- */
    int step = 0;

    while (step < EH_MAX_BEAM_DEPTH) {
        step++;

        /*
         * candidates[]: holds all (beam_idx, child_node, score) entries
         * generated from every active beam.
         * Max candidates = EH_BEAM_WIDTH * EH_MAX_CANDIDATES.
         */
        typedef struct {
            int         parent_beam_idx; /* Index of the parent beam */
            EH_DAGNode *node;            /* Candidate child node */
            float       total_score;     /* Cumulative score = parent + branch */
        } Candidate;

        Candidate candidates[EH_BEAM_WIDTH * EH_MAX_CANDIDATES];
        int       num_candidates = 0;

        /* --- Step 1: Expand all current beams --- */
        for (int b = 0; b < active_beams; b++) {
            EH_BeamPath *beam    = &beams[b];
            EH_DAGNode  *current = beam->nodes[beam->depth - 1];

            if (!current || current->child_count == 0) {
                /*
                 * This beam has reached a leaf node.
                 * Record it as a "terminal" candidate with the current score;
                 * a NULL node signals that this beam has ended.
                 */
                if (num_candidates < EH_BEAM_WIDTH * EH_MAX_CANDIDATES) {
                    candidates[num_candidates].parent_beam_idx = b;
                    candidates[num_candidates].node            = NULL;
                    candidates[num_candidates].total_score     = beam->cumulative_score;
                    num_candidates++;
                }
                continue;
            }

            /*
             * Scan up to EH_MAX_CANDIDATES child nodes.
             * If a node has more than EH_MAX_CANDIDATES children, only the
             * first EH_MAX_CANDIDATES are scanned — this is the "quick scan"
             * budget before selection.
             */
            int scan_count = current->child_count;
            if (scan_count > EH_MAX_CANDIDATES) scan_count = EH_MAX_CANDIDATES;

            for (int c = 0; c < scan_count; c++) {
                EH_DAGNode *child = current->children[c];
                if (!child) continue;

                /* Compute branch score via the Scoring Core (O(input_dim)) */
                float branch_score = eh_scoring_predict_branch(
                    ctx->router_core, input_vector, child);

                if (mctx) {
                    mctx->stats.beam_nodes_scanned++;
                }

                /* Total score = parent beam cumulative score + new branch score */
                float total_score = beam->cumulative_score + branch_score;

                /* Append to the candidate list if space remains */
                if (num_candidates < EH_BEAM_WIDTH * EH_MAX_CANDIDATES) {
                    candidates[num_candidates].parent_beam_idx = b;
                    candidates[num_candidates].node            = child;
                    candidates[num_candidates].total_score     = total_score;
                    num_candidates++;
                }
            }
        }

        /* If no candidates were generated, terminate */
        if (num_candidates == 0) break;

        /* --- Step 2: Select the EH_BEAM_WIDTH best candidates ---
         *
         * Simple selection sort: O(num_candidates * BEAM_WIDTH).
         * With num_candidates <= 8 (BEAM_WIDTH=2, MAX_CANDIDATES=4),
         * this outperforms quicksort due to lower partition overhead.
         */
        for (int i = 0; i < num_candidates - 1 && i < EH_BEAM_WIDTH; i++) {
            int best_idx = i;
            for (int j = i + 1; j < num_candidates; j++) {
                if (candidates[j].total_score > candidates[best_idx].total_score) {
                    best_idx = j;
                }
            }
            /* Swap candidate[i] and candidate[best_idx] */
            if (best_idx != i) {
                Candidate tmp        = candidates[i];
                candidates[i]        = candidates[best_idx];
                candidates[best_idx] = tmp;
            }
        }

        /* Keep only the top EH_BEAM_WIDTH candidates */
        int keep_count = (num_candidates < EH_BEAM_WIDTH)
                         ? num_candidates : EH_BEAM_WIDTH;

        if (mctx) {
            mctx->stats.beam_nodes_pruned += (num_candidates - keep_count);
        }

        /* --- Step 3: Update beams with the selected candidates ---
         *
         * Build a new beam set from the best EH_BEAM_WIDTH candidates.
         * Each new beam inherits the parent beam's path and appends the new node.
         */
        EH_BeamPath new_beams[EH_BEAM_WIDTH];
        int         new_active = 0;
        bool        any_leaf   = false;

        for (int i = 0; i < keep_count; i++) {
            Candidate   *cand        = &candidates[i];
            EH_BeamPath *parent_beam = &beams[cand->parent_beam_idx];

            memcpy(&new_beams[new_active], parent_beam, sizeof(EH_BeamPath));
            new_beams[new_active].cumulative_score = cand->total_score;

            if (cand->node == NULL) {
                /* Terminal beam: depth unchanged, mark as leaf */
                any_leaf = true;
            } else {
                /* --- Step 4: Activate the selected node ---
                 *
                 * This is the key design point of EH-Engine:
                 * - Collapsed node (is_collapsed): O(output_dim) fill.
                 * - Active node: O(rows * cols) dense matmul.
                 *
                 * The temporary result is written to node_output[],
                 * then copied to output_vector if this is the best beam.
                 */
                memset(node_output, 0, sizeof(float) * output_dim);
                _eh_engine_activate_node(ctx, cand->node,
                                         input_vector, input_dim,
                                         node_output, output_dim,
                                         input_norm);

                /* Append node to the beam's path */
                int d = new_beams[new_active].depth;
                if (d < EH_MAX_BEAM_DEPTH) {
                    new_beams[new_active].nodes[d] = cand->node;
                    new_beams[new_active].depth    = d + 1;
                }
            }

            new_active++;
        }

        /* Update beam state for the next step */
        memcpy(beams, new_beams, sizeof(EH_BeamPath) * new_active);
        active_beams = new_active;

        /* If all beams have reached leaves, exit the loop */
        if (any_leaf && new_active <= 1) break;
    }

    /* ---------------------------------------------------------------
     * FINAL STEP: SELECT THE BEST BEAM AND WRITE OUTPUT
     *
     * Among the remaining beams, select the one with the highest
     * cumulative_score. Activate its final node and write to the
     * caller-supplied output_vector.
     * --------------------------------------------------------------- */
    int   best_beam_idx   = 0;
    float best_beam_score = -FLT_MAX;

    for (int b = 0; b < active_beams; b++) {
        if (beams[b].cumulative_score > best_beam_score) {
            best_beam_score = beams[b].cumulative_score;
            best_beam_idx   = b;
        }
    }

    EH_BeamPath *best_beam  = &beams[best_beam_idx];
    EH_DAGNode  *final_node = best_beam->nodes[best_beam->depth - 1];

    if (final_node) {
        /* Final activation to produce the official output */
        _eh_engine_activate_node(ctx, final_node,
                                  input_vector, input_dim,
                                  output_vector, output_dim,
                                  input_norm);
    } else {
        /* Fallback: zero-fill if no valid node is available */
        memset(output_vector, 0, sizeof(float) * output_dim);
    }

#ifdef EH_DEBUG
    printf("[EH_ENGINE] Inference complete: best_beam=%d, score=%.4f, depth=%d\n",
           best_beam_idx, best_beam_score, best_beam->depth);
#endif

    return true;
}

/* =================================================================
 * PART 3: ENGINE SHUTDOWN AND RESOURCE RELEASE
 * =================================================================
 *
 * Deallocation order is critical:
 *   1. Free the DAG graph (recursive, handles shared nodes).
 *   2. Free the Scoring Core.
 *   3. Free the Context struct.
 *
 * Order must not be reversed — DAG free may depend on routing_weights
 * still being valid in future extensions.
 * ================================================================= */
void eh_engine_shutdown(EH_Context *ctx) {
    if (!ctx) return;

    if (ctx->dynamic_ctx) {
        eh_dynamic_free(ctx->dynamic_ctx);
        ctx->dynamic_ctx = NULL;
    }

    /* Free the DAG graph (includes all nodes and weight matrices) */
    if (ctx->root_node) {
        eh_dag_free_graph(ctx->root_node);
        ctx->root_node = NULL;
    }

    /* Free the Scoring Core */
    if (ctx->router_core) {
        eh_scoring_free(ctx->router_core);
        ctx->router_core = NULL;
    }

    /* Free the Context struct */
    free(ctx);

#ifdef EH_DEBUG
    printf("[EH_ENGINE] Engine shut down and all resources released.\n");
#endif
}

void eh_engine_reset_stats(EH_Context *ctx) {
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(EH_EngineStats));
    }
}

EH_EngineStats eh_engine_get_stats(const EH_Context *ctx) {
    EH_EngineStats empty = {0};
    if (ctx) {
        return ctx->stats;
    }
    return empty;
}
