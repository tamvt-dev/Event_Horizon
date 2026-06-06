/* ================================================================
 * eh_hgn_engine.c — HGN Layer 4: Unified Inference Engine impl
 *
 * Compile:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I include \
 *       -c src/hgn/eh_hgn_engine.c -o src/hgn/eh_hgn_engine.o
 * ================================================================ */

#include "../../include/hgn/eh_hgn_engine.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * eh_hgn_session_init
 * ---------------------------------------------------------------- */
int eh_hgn_session_init(
        EH_HGN_InferenceSession *session,
        const EH_HGN_BaseDag    *dag,
        EH_Arena                *arena,
        const uint32_t          *prompt,
        uint32_t                 prompt_len,
        const EH_HGN_EngineConfig *config)
{
    if (!session || !dag || !arena || !prompt || prompt_len == 0) {
        fprintf(stderr, "[eh_hgn_engine] Invalid arguments to session_init\n");
        return -1;
    }

    memset(session, 0, sizeof(*session));

    /* Layer 1: DAG reference (read-only) */
    session->dag = dag;

    /* Layer 2: Collapse context */
    if (eh_hgn_collapse_ctx_init(&session->collapse_ctx, arena) != 0) {
        fprintf(stderr, "[eh_hgn_engine] Failed to init collapse context\n");
        return -1;
    }

    /* Config: use defaults nếu NULL */
    if (config) {
        session->config = *config;
    } else {
        session->config = eh_hgn_default_config();
    }
    if (session->config.beam_width == 0) {
        session->config.beam_width = 8;
    }

    /* Allocate paths buffer from arena */
    EH_HGN_BeamPath *paths_buf = eh_arena_alloc(arena, session->config.beam_width * sizeof(EH_HGN_BeamPath));
    if (!paths_buf) {
        fprintf(stderr, "[eh_hgn_engine] Failed to allocate beam paths buffer from arena\n");
        return -1;
    }

    /* Layer 3: Beam tracker */
    eh_hgn_beam_init(&session->beam_tracker, dag, prompt, prompt_len, paths_buf, session->config.beam_width);

    /* Apply custom thresholds vào collapse context */
    session->collapse_ctx.collapse_thresh = session->config.collapse_thresh;
    session->collapse_ctx.entropy_thresh  = session->config.entropy_thresh;
    session->collapse_ctx.perturb_scale   = session->config.perturb_scale;

    /* Init state */
    session->step_count  = 0;
    session->is_finished = false;

    return 0;
}

/* ----------------------------------------------------------------
 * eh_hgn_session_reset
 * ---------------------------------------------------------------- */
void eh_hgn_session_reset(
        EH_HGN_InferenceSession *session,
        const uint32_t          *prompt,
        uint32_t                 prompt_len)
{
    if (!session || !prompt || prompt_len == 0) return;

    /* Reset collapse context (giữ pool, xóa state) */
    eh_hgn_collapse_ctx_reset(&session->collapse_ctx);

    /* Reset beam tracker với prompt mới */
    eh_hgn_beam_init(&session->beam_tracker, session->dag, prompt, prompt_len,
                     session->beam_tracker.paths, session->beam_tracker.beam_width);

    /* Reset session state */
    session->step_count  = 0;
    session->is_finished = false;
    memset(session->h_current, 0, sizeof(session->h_current));
    memset(session->h_pooled,  0, sizeof(session->h_pooled));
}

/* ----------------------------------------------------------------
 * eh_hgn_session_step: Core inference pipeline
 * ---------------------------------------------------------------- */
uint32_t eh_hgn_session_step(EH_HGN_InferenceSession *session)
{
    if (!session || session->is_finished) return 0;

    /* Check max_steps limit */
    if (session->config.max_steps > 0 &&
        session->step_count >= session->config.max_steps) {
        session->is_finished = true;
        return 0;
    }

    /* --- Step 1: Lấy current beam state --- */
    if (session->beam_tracker.active_paths == 0) {
        session->is_finished = true;
        return 0;
    }

    /* Snapshot last_token TRƯỚC khi gọi beam_step().
     * Sau beam_step(), paths[] bị overwrite → mọi pointer vào
     * paths[] đều stale. Chỉ dùng giá trị scalar đã copy ra. */
    uint32_t pre_step_seq_len = session->beam_tracker.paths[0].seq_len;
    if (pre_step_seq_len == 0) {
        session->is_finished = true;
        return 0;
    }
    uint32_t last_token = session->beam_tracker.paths[0].tokens[pre_step_seq_len - 1];
    const float *node_emb = eh_hgn_dag_node_vec(session->dag, last_token);
    if (!node_emb) {
        fprintf(stderr, "[eh_hgn_engine] Invalid last_token=%u\n", last_token);
        session->is_finished = true;
        return 0;
    }

    /* Copy node embedding vào h_current */
    memcpy(session->h_current, node_emb, EH_HGN_EMBED_DIM * sizeof(float));

    /* --- Step 2: Collapse Gating (nếu enabled) --- */
    EH_HGN_CollapseDecision decision = EH_HGN_EXPAND;
    if (session->config.enable_collapse) {
        decision = eh_hgn_collapse_gate(&session->collapse_ctx, session->h_current);

        if (decision == EH_HGN_COLLAPSE) {
            /* Fast path: mean pooling O(N) thay vì full DAG */
            eh_hgn_collapse_mean_pool(session->dag, last_token, session->h_pooled);
            
            /* NOTE: Trong collapse mode, ta có thể inject h_pooled vào beam scoring
             * Hiện tại beam_step() tự handle edges, nên ta chỉ log collapse decision.
             * Production implementation có thể override scoring vector ở đây. */
        }
    }

    /* --- Step 3: Beam expansion (Layer 3) --- */
    uint32_t active_beams = eh_hgn_beam_step(&session->beam_tracker, session->dag);

    /* --- Step 4: Mutant spawning (nếu enabled và entropy cao) --- */
    if (session->config.enable_mutants && active_beams > 0) {
        /* Thu thập scores của tất cả beams hiện tại */
        float scores[session->beam_tracker.beam_width];
        uint32_t beam_count = 0;
        for (uint32_t i = 0; i < session->beam_tracker.beam_width && i < session->beam_tracker.active_paths; i++) {
            scores[beam_count++] = session->beam_tracker.paths[i].score;
        }

        if (beam_count > 1) {
            float entropy = eh_hgn_beam_entropy(scores, beam_count);

            if (entropy > session->collapse_ctx.entropy_thresh) {
                /* src = token trước expansion (đã snapshot).
                 * dst = token mới nhất của best beam SAU expansion.
                 * paths[0] ở đây là NEW best beam (đã sort sau beam_step). */
                uint32_t src = last_token;
                const EH_HGN_BeamPath *new_best = &session->beam_tracker.paths[0];
                uint32_t dst = (new_best->seq_len > 0)
                             ? new_best->tokens[new_best->seq_len - 1]
                             : 0;
                float top_score = scores[0];

                EH_HGN_MutantNode *mutant = eh_hgn_mutant_spawn(
                    &session->collapse_ctx, src, dst, top_score);

                /* Mutant được sinh ra nhưng không được inject vào beam ngay lập tức.
                 * Trong production, mutant->vec có thể được dùng như alternative context
                 * cho edge scoring trong step tiếp theo. */
                (void)mutant;  /* suppress unused warning */
            }
        }
    }

    /* --- Step 5: Cleanup mutants sau mỗi step (lifetime=1) --- */
    eh_hgn_collapse_step_end(&session->collapse_ctx);

    /* --- Step 6: Update session state --- */
    session->step_count++;

    /* Check termination: tất cả beams đã finished? */
    if (active_beams == 0) {
        session->is_finished = true;
        return 0;
    }

    return active_beams;
}

/* ----------------------------------------------------------------
 * eh_hgn_session_get_beams
 * ---------------------------------------------------------------- */
const EH_HGN_BeamPath *eh_hgn_session_get_beams(
        const EH_HGN_InferenceSession *session)
{
    if (!session) return NULL;
    return session->beam_tracker.paths;
}

/* ----------------------------------------------------------------
 * eh_hgn_session_get_best
 * ---------------------------------------------------------------- */
const EH_HGN_BeamPath *eh_hgn_session_get_best(
        const EH_HGN_InferenceSession *session)
{
    if (!session) return NULL;
    return eh_hgn_beam_get_best(&session->beam_tracker);
}

/* ----------------------------------------------------------------
 * eh_hgn_session_dump_stats
 * ---------------------------------------------------------------- */
void eh_hgn_session_dump_stats(const EH_HGN_InferenceSession *session)
{
    if (!session) {
        fprintf(stderr, "[eh_hgn_engine] NULL session\n");
        return;
    }

    fprintf(stderr, "\n=== EH_HGN_InferenceSession Stats ===\n");
    fprintf(stderr, "  Steps executed    : %u\n", session->step_count);
    fprintf(stderr, "  Generation done   : %s\n", session->is_finished ? "YES" : "NO");
    fprintf(stderr, "  Active beams      : %u\n", session->beam_tracker.active_paths);
    fprintf(stderr, "  Collapse enabled  : %s\n", session->config.enable_collapse ? "YES" : "NO");
    fprintf(stderr, "  Mutants enabled   : %s\n", session->config.enable_mutants ? "YES" : "NO");
    fprintf(stderr, "  Max steps limit   : %u (0=unlimited)\n", session->config.max_steps);
    fprintf(stderr, "=====================================\n");

    /* Dump collapse context stats */
    eh_hgn_collapse_dump_stats(&session->collapse_ctx);
}

/* ----------------------------------------------------------------
 * eh_hgn_session_dump_beams
 * ---------------------------------------------------------------- */
void eh_hgn_session_dump_beams(const EH_HGN_InferenceSession *session)
{
    if (!session) {
        fprintf(stderr, "[eh_hgn_engine] NULL session\n");
        return;
    }

    fprintf(stderr, "\n=== Current Beam States ===\n");
    for (uint32_t i = 0; i < session->beam_tracker.active_paths && i < session->beam_tracker.beam_width; i++) {
        const EH_HGN_BeamPath *beam = &session->beam_tracker.paths[i];
        fprintf(stderr, "  Beam %u: score=%.3f len=%u finished=%s\n",
                i, beam->score, beam->seq_len, beam->is_finished ? "YES" : "NO");
        fprintf(stderr, "    Tokens: [");
        for (uint32_t j = 0; j < beam->seq_len && j < 16; j++) {  /* limit to first 16 */
            fprintf(stderr, "%u%s", beam->tokens[j], (j < beam->seq_len - 1) ? ", " : "");
        }
        if (beam->seq_len > 16) fprintf(stderr, ", ...");
        fprintf(stderr, "]\n");
    }
    fprintf(stderr, "===========================\n");
}
