/* ================================================================
 * eh_hgn_collapse.c — HGN Layer 2: Dynamic Collapse Gating impl
 *
 * Compile từ EventHorizon/:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I include \
 *       -c src/hgn/eh_hgn_collapse.c -o src/hgn/eh_hgn_collapse.o
 * ================================================================ */

#include "../../include/hgn/eh_hgn_collapse.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * AVX2 dot product nội bộ — không phụ thuộc eh_scoring.c
 * Dùng cho cosine similarity trên hot path gating
 * ---------------------------------------------------------------- */
/* dot128: scalar với double accumulator — gcc -O3 auto-vectorize
 * Tránh dùng AVX2 intrinsic trực tiếp để không phụ thuộc alignment
 * và không conflict với attribute target của eh_scoring.c          */
static float _dot128(const float * restrict a, const float * restrict b)
{
    double acc = 0.0;
    for (int i = 0; i < 128; i++) acc += (double)a[i] * (double)b[i];
    return (float)acc;
}

/* Norm bình phương của vector 128D                                */
static float _norm_sq128(const float *v)
{
    return _dot128(v, v);
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_ctx_init
 * ---------------------------------------------------------------- */
int eh_hgn_collapse_ctx_init(EH_HGN_CollapseCtx *ctx,
                              EH_Arena           *arena)
{
    if (!ctx || !arena) return -1;

    memset(ctx, 0, sizeof(*ctx));

    /* Cấp phát mutant_pool từ arena — lifetime = toàn inference pass */
    size_t pool_size = EH_HGN_MAX_MUTANTS * sizeof(EH_HGN_MutantNode);
    ctx->mutant_pool = (EH_HGN_MutantNode *)eh_arena_alloc(arena, pool_size);
    if (!ctx->mutant_pool) {
        fprintf(stderr, "[eh_hgn_collapse] Arena OOM for mutant_pool\n");
        return -1;
    }
    memset(ctx->mutant_pool, 0, pool_size);

    /* Copy defaults từ compile-time constants — cho phép override   */
    ctx->collapse_thresh = EH_HGN_COLLAPSE_THRESH;
    ctx->entropy_thresh  = EH_HGN_ENTROPY_THRESH;
    ctx->perturb_scale   = EH_HGN_PERTURB_SCALE;
    ctx->h_prev_valid    = false;

    return 0;
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_ctx_reset
 * ---------------------------------------------------------------- */
void eh_hgn_collapse_ctx_reset(EH_HGN_CollapseCtx *ctx)
{
    if (!ctx) return;

    /* Giữ lại: mutant_pool pointer, thresholds                     */
    /* Reset: state, stats, h_prev                                  */
    memset(ctx->h_prev, 0, sizeof(ctx->h_prev));
    ctx->h_prev_valid   = false;
    ctx->step_count     = 0;
    ctx->collapse_count = 0;
    ctx->expand_count   = 0;
    ctx->mutant_count   = 0;
    ctx->mutant_active  = 0;

    if (ctx->mutant_pool)
        memset(ctx->mutant_pool, 0,
               EH_HGN_MAX_MUTANTS * sizeof(EH_HGN_MutantNode));
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_gate
 *
 * Cosine similarity: cos(h_t, h_{t-1}) = dot / (|h_t| * |h_{t-1}|)
 * Dùng AVX2 dot product 2 lần (dot + 2 norms) — ~6 ns trên x86
 * ---------------------------------------------------------------- */
EH_HGN_CollapseDecision eh_hgn_collapse_gate(
        EH_HGN_CollapseCtx *ctx,
        const float        *h_t)
{
    EH_HGN_CollapseDecision decision;

    if (!ctx->h_prev_valid) {
        /* Step đầu tiên — chưa có reference vector → expand         */
        decision = EH_HGN_EXPAND;
    } else {
        /* Tính cosine similarity giữa h_t và h_prev                */
        float dot    = _dot128(h_t, ctx->h_prev);
        float norm_t = _norm_sq128(h_t);
        float norm_p = _norm_sq128(ctx->h_prev);

        float denom = sqrtf(norm_t * norm_p);

        /* Tránh chia cho 0 khi vector zero                         */
        float cosine = (denom > 1e-8f) ? (dot / denom) : 0.0f;

        decision = (cosine > ctx->collapse_thresh)
                 ? EH_HGN_COLLAPSE
                 : EH_HGN_EXPAND;
    }

    /* Cập nhật stats và lưu h_t cho step tiếp theo                */
    if (decision == EH_HGN_COLLAPSE) ctx->collapse_count++;
    else                             ctx->expand_count++;

    ctx->step_count++;
    memcpy(ctx->h_prev, h_t, EH_HGN_EMBED_DIM * sizeof(float));
    ctx->h_prev_valid = true;

    return decision;
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_mean_pool
 *
 * Tính mean của tất cả edge weight vectors của token_id.
 * O(K × D) với K ≤ 32, D = 128 — rất nhẹ.
 * Nếu node không có edges → trả về node embedding của token_id.
 * ---------------------------------------------------------------- */
void eh_hgn_collapse_mean_pool(
        const EH_HGN_BaseDag *dag,
        uint32_t              token_id,
        float                *out_vec)
{
    memset(out_vec, 0, EH_HGN_EMBED_DIM * sizeof(float));

    uint32_t count = eh_hgn_dag_fanout(dag, token_id);
    if (count == 0) {
        /* Sink node — trả về embedding của chính nó               */
        const float *nv = eh_hgn_dag_node_vec(dag, token_id);
        if (nv) memcpy(out_vec, nv, EH_HGN_EMBED_DIM * sizeof(float));
        return;
    }

    /* Tích lũy tất cả weight vectors của các edges                */
    EH_HGN_FOR_EDGES(dag, token_id, e) {
        const float *wv = eh_hgn_dag_edge_weight(dag, e);
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            out_vec[k] += wv[k];
    }

    /* Chia trung bình                                              */
    float inv_count = 1.0f / (float)count;
    for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
        out_vec[k] *= inv_count;
}

/* ----------------------------------------------------------------
 * eh_hgn_beam_entropy
 *
 * H = -Σ p_i * log(p_i) với p_i = softmax(scores)
 * Dùng log-sum-exp trick để tránh overflow:
 *   log(Σ exp(s_i)) = max + log(Σ exp(s_i - max))
 * ---------------------------------------------------------------- */
float eh_hgn_beam_entropy(const float *scores, uint32_t count)
{
    if (!scores || count == 0) return 0.0f;
    if (count == 1)            return 0.0f;  /* deterministic       */

    /* Tìm max để log-sum-exp                                       */
    float max_s = scores[0];
    for (uint32_t i = 1; i < count; i++)
        if (scores[i] > max_s) max_s = scores[i];

    /* Tính log-partition Z                                         */
    double Z = 0.0;
    for (uint32_t i = 0; i < count; i++)
        Z += exp((double)(scores[i] - max_s));
    (void)(log(Z));

    /* H = log_Z - (1/Z) * Σ scores[i] * exp(scores[i] - max)     */
    double H = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double p = exp((double)(scores[i] - max_s)) / Z;
        if (p > 1e-12) H -= p * log(p);
    }

    return (float)H;
}

/* ----------------------------------------------------------------
 * eh_hgn_mutant_spawn
 *
 * seed     = (uint32_t)(top_beam_score * 1000.0) XOR step_count
 * perturb  = LCG(seed) * perturb_scale cho mỗi chiều
 * mutant   = h_prev + perturb
 * ---------------------------------------------------------------- */
EH_HGN_MutantNode *eh_hgn_mutant_spawn(
        EH_HGN_CollapseCtx *ctx,
        uint32_t            src_token,
        uint32_t            dst_token,
        float               top_beam_score)
{
    if (!ctx || !ctx->mutant_pool) return NULL;

    /* Tìm slot trống trong pool                                    */
    EH_HGN_MutantNode *slot = NULL;
    for (uint32_t i = 0; i < EH_HGN_MAX_MUTANTS; i++) {
        if (!ctx->mutant_pool[i].active) {
            slot = &ctx->mutant_pool[i];
            break;
        }
    }
    if (!slot) {
        fprintf(stderr, "[eh_hgn_collapse] Mutant pool full (%u slots)\n",
                EH_HGN_MAX_MUTANTS);
        return NULL;
    }

    /* Seed từ top_beam_score XOR step_count → deterministic nhưng
     * khác nhau mỗi step và mỗi score                             */
    uint32_t score_bits;
    memcpy(&score_bits, &top_beam_score, sizeof(float));
    uint32_t seed = score_bits ^ ctx->step_count ^ (src_token << 16);

    /* Sinh perturbation vector bằng LCG, scale Xavier             */
    float scale = ctx->perturb_scale;
    for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++) {
        float perturb   = _eh_hgn_lcg_float(&seed) * scale;
        slot->vec[k]    = ctx->h_prev[k] + perturb;
    }

    slot->src_token    = src_token;
    slot->dst_token    = dst_token;
    slot->bridge_score = top_beam_score;
    slot->active       = true;

    ctx->mutant_active++;
    ctx->mutant_count++;

    return slot;
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_step_end: deactivate tất cả mutants sau 1 step
 * ---------------------------------------------------------------- */
void eh_hgn_collapse_step_end(EH_HGN_CollapseCtx *ctx)
{
    if (!ctx || !ctx->mutant_pool) return;
    for (uint32_t i = 0; i < EH_HGN_MAX_MUTANTS; i++)
        ctx->mutant_pool[i].active = false;
    ctx->mutant_active = 0;
}

/* ----------------------------------------------------------------
 * eh_hgn_collapse_dump_stats
 * ---------------------------------------------------------------- */
void eh_hgn_collapse_dump_stats(const EH_HGN_CollapseCtx *ctx)
{
    if (!ctx) { fprintf(stderr, "[eh_hgn_collapse] NULL ctx\n"); return; }

    uint32_t total = ctx->collapse_count + ctx->expand_count;
    float collapse_pct = total > 0
        ? 100.0f * (float)ctx->collapse_count / (float)total
        : 0.0f;

    fprintf(stderr,
        "=== EH_HGN_CollapseCtx Stats ===\n"
        "  steps      : %u\n"
        "  collapse   : %u (%.1f%% FLOPs saved)\n"
        "  expand     : %u\n"
        "  mutants    : %u spawned, %u active\n"
        "  thresh     : collapse=%.3f  entropy=%.3f\n"
        "================================\n",
        ctx->step_count,
        ctx->collapse_count, collapse_pct,
        ctx->expand_count,
        ctx->mutant_count, ctx->mutant_active,
        ctx->collapse_thresh, ctx->entropy_thresh
    );
}