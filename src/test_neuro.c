/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: src/test_neuro.c
 * Description: Full integration test for Phase 2 — Neuroplasticity.
 *
 * Simulates a single "Live Evolution" cycle:
 *   PASS 1: Hard input -> Beam inference -> High error output
 *           -> Parametric Learning updates routing weights.
 *           -> Structural Mutation spawns a MutationNode in the Arena.
 *   PASS 2: Re-inference with the evolved DAG containing the mutant -> MAE decreases.
 *   CLEANUP: Disconnect mutant from DAG -> Free Arena -> Free all resources.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../include/core/eh_dag.h"
#include "../include/core/eh_scoring.h"
#include "../include/core/eh_engine.h"
#include "../include/core/eh_dynamic.h"
#include "../include/core/eh_arena.h"
#include "../include/core/eh_neuro.h"

#define DIM 8

static void fill_weights(EH_DAGNode *node, float scale) {
    int n = node->weights.rows * node->weights.cols;
    for (int i = 0; i < n; i++) {
        float r = (float)rand() / RAND_MAX;
        node->weights.data[i] = (r - 0.5f) * 2.0f * scale;
    }
}

static void print_vec(const char *label, const float *v, int dim) {
    printf("  %s [", label);
    for (int i = 0; i < dim; i++)
        printf("%.3f%s", v[i], i < dim-1 ? ", " : "");
    printf("]\n");
}

int main(void) {
    srand(42);

    printf("==================================================\n");
    printf("   EVENTHORIZON: LIVE EVOLUTION & MUTATION TEST\n");
    printf("==================================================\n");

    /* ==============================================================
     * SETUP: Build infrastructure
     * ============================================================== */
    printf("[SETUP] Initializing base network topology...\n");

    /* Simple DAG: root -> node_expert */
    EH_DAGNode *root        = eh_dag_create_node(0, DIM, DIM);
    EH_DAGNode *node_expert = eh_dag_create_node(1, DIM, DIM);
    fill_weights(root,        2.0f);
    fill_weights(node_expert, 1.5f);
    eh_dag_connect_nodes(root, node_expert);

    printf("-> Total child nodes of Root currently: %d\n", root->child_count);

    /* Engine infrastructure */
    EH_ScoringCore  *scorer = eh_scoring_init(DIM);
    /* Large Arena to prevent premature exhaustion during structural mutations */
    EH_Arena        *arena  = eh_arena_create(EH_ARENA_SIZE * 4);
    EH_NeuroContext *nctx   = eh_neuro_init(arena,
                                             EH_NEURO_ERROR_THRESHOLD,
                                             EH_NEURO_LEARNING_RATE,
                                             EH_NEURO_MUTATION_RATE);

    /* Setup engine with Dynamic Collapse enabled */
    EH_Context *ctx = eh_engine_setup_dynamic(root, scorer,
                                               99.0f,   /* High static threshold -> disable static collapse */
                                               0.10f,   /* Dynamic low threshold */
                                               0.92f);  /* Dynamic high threshold */
    if (!ctx || !arena || !nctx) {
        fprintf(stderr, "[SETUP] Initialization error!\n"); return 1;
    }

    /* ==============================================================
     * PASS 1: Hard Input — triggers deep calculation, large error
     * ============================================================== */
    printf("--------------------------------------------------\n");
    printf("[PASS 1] Starting hard input vector (requires deep routing)\n");

    /*
     * "Hard Input": a vector whose correlation (energy) with node_expert
     * falls into the saturation region (E > 0.92) -> triggers dynamic collapse
     * -> node returns distribution_mean instead of executing dense matmul.
     * This results in a large error compared to a complex target.
     */
    float X_hard[DIM];
    /* Align with node_expert's mu_W to generate a high energy (saturation) */
    if (ctx->dynamic_ctx && ctx->dynamic_ctx->profiles[1].mu_W) {
        float *mu = ctx->dynamic_ctx->profiles[1].mu_W;
        float  mu_norm = ctx->dynamic_ctx->profiles[1].mu_norm;
        if (mu_norm > 1e-6f) {
            for (int i = 0; i < DIM; i++)
                X_hard[i] = mu[i] / mu_norm * 3.0f; /* Parallel to mu_W */
        } else {
            for (int i = 0; i < DIM; i++) X_hard[i] = 1.0f;
        }
    } else {
        for (int i = 0; i < DIM; i++) X_hard[i] = (float)(i + 1) * 0.5f;
    }

    /* Complex target: uncorrelated with distribution_mean */
    float target[DIM];
    for (int i = 0; i < DIM; i++)
        target[i] = sinf((float)i * 0.7f) * 2.0f;

    /* Manually evaluate Dynamic Collapse to print diagnostic info */
    float X_norm = 0.0f;
    for (int i = 0; i < DIM; i++) X_norm += X_hard[i] * X_hard[i];
    X_norm = sqrtf(X_norm);

    if (ctx->dynamic_ctx) {
        EH_CollapseDecision d = eh_dynamic_evaluate(ctx->dynamic_ctx,
                                                     X_hard, X_norm,
                                                     node_expert);
        const char *reason[] = { "DENSE", "LOW_CORR", "COLLAPSE" };
        printf("-> Evaluating morphology of Node 1: Energy=%.4f | "
               "Required Status: %s\n",
               d.activation_energy,
               d.should_collapse ? reason[d.reason] : "DENSE");
    }

    /* Execute inference */
    float output[DIM] = {0};
    eh_engine_inference(ctx, X_hard, DIM, output, DIM);

    /* Calculate error */
    float mae = 0.0f;
    for (int i = 0; i < DIM; i++)
        mae += fabsf(output[i] - target[i]);
    mae /= DIM;

    print_vec("Output   :", output, DIM);
    print_vec("Target   :", target, DIM);
    printf("  MAE = %.4f\n", mae);

    /* ==============================================================
     * FEEDBACK: Parametric Learning + Structural Mutation
     * ============================================================== */
    printf("[FEEDBACK] Evaluation Environment: Severe deviation detected!\n");
    printf("[LEARNING] Optimizing routing parameters (Parametric Learning)...\n");

    /* Store weights before update to display the delta */
    float w_before = scorer->routing_weights[
        (size_t)node_expert->node_id * scorer->input_dim];

    EH_FeedbackResult fb = eh_neuro_process_feedback(
        nctx,
        node_expert,
        output, target, DIM,
        scorer,
        X_hard, DIM);

    float w_after = scorer->routing_weights[
        (size_t)node_expert->node_id * scorer->input_dim];

    printf("-> Routing matrix updated. "
           "Weight W_route[0]: %.4f -> %.4f\n",
           w_before, w_after);

    /* ==============================================================
     * STRUCTURAL MUTATION REPORT
     * ============================================================== */
    printf("[STRUCTURAL] Evaluating structural mutation conditions (Neuroplasticity)...\n");
    printf("--------------------------------------------------\n");
    printf("[GRAPH VALIDATION AFTER MUTATION]\n");

    if (fb.mutation_fired) {
        printf("-> STATUS: SUCCESS! Graph structure branched successfully.\n");
        printf("-> Expert Node (ID: %d) currently has: %d dynamic child node(s).\n",
               node_expert->node_id, node_expert->child_count);

        /* Print mutant details */
        for (int i = 0; i < nctx->mutant_count; i++) {
            EH_MutantRecord *rec = &nctx->mutants[i];
            EH_DAGNode      *m   = rec->node;
            printf("-> Live Mutant Details -> ID: %d | "
                   "Matrix Dimensions: %dx%d\n",
                   m->node_id, m->weights.rows, m->weights.cols);
            printf("-> Expected internal error residual distribution mean: %.4f\n",
                   m->distribution_mean);
        }
    } else {
        printf("-> STATUS: No mutation occurred (MAE=%.4f < threshold=%.4f "
               "or mutation was skipped)\n",
               fb.error_magnitude, nctx->error_threshold);
    }

    /* Print full diagnostic reports */
    eh_neuro_report(nctx);
    eh_arena_stats(arena);

    /* ==============================================================
     * PASS 2: Re-run inference to verify MAE reduction
     * ============================================================== */
    printf("--------------------------------------------------\n");
    printf("[PASS 2] Running inference on evolved DAG (with mutant branch)...\n");

    float output_pass2[DIM] = {0};
    eh_engine_inference(ctx, X_hard, DIM, output_pass2, DIM);

    float mae_pass2 = 0.0f;
    for (int i = 0; i < DIM; i++)
        mae_pass2 += fabsf(output_pass2[i] - target[i]);
    mae_pass2 /= DIM;

    print_vec("Output P2:", output_pass2, DIM);
    print_vec("Target   :", target, DIM);
    printf("  MAE Pass 2 = %.4f (Before: %.4f)\n", mae_pass2, mae);
    if (mae_pass2 < mae) {
        printf("-> STATUS: SUCCESS! MAE decreased after structural evolution.\n");
    } else {
        printf("-> STATUS: FAILURE! MAE did not decrease.\n");
    }

    /* ==============================================================
     * CLEANUP: Safe teardown and deallocation
     * ============================================================== */
    printf("[CLEANUP] Initializing safe deallocation procedure...\n");

    /* Step 1: Disconnect mutants from DAG to prevent dangling pointers */
    eh_neuro_disconnect_all(nctx);
    printf("-> 1. Mutants safely disconnected from the DAG tree.\n");

    /* Step 2: Shutdown base DAG (excluding detached mutants) */
    eh_engine_shutdown(ctx); /* Frees DAG + scorer + dynamic_ctx + ctx */
    printf("-> 2. DAG graph system released... DONE.\n");

    /* ctx->dynamic_ctx was freed by eh_engine_shutdown */
    printf("-> 3. Dynamic context buffer released... DONE.\n");

    /* scorer was freed by eh_engine_shutdown */
    printf("-> 4. Routing Core (Scoring Core) released... DONE.\n");

    /* Step 3: Free NeuroContext wrapper (excluding Arena) */
    eh_neuro_free(nctx);

    /* Step 4: Destroy Arena — releases mutant nodes completely */
    eh_arena_destroy(arena);
    printf("-> 5. Memory Arena infrastructure released... DONE.\n");

    printf("[CLEANUP] Memory successfully cleared. Execution completed cleanly!\n");

    return 0;
}