/* ================================================================
 * eh_hgn_collapse.h — HGN Layer 2: Dynamic Collapse Gating
 *
 * Hai cơ chế:
 *   1. Collapse Gating: cos(h_t, h_{t-1}) → quyết định COLLAPSE/EXPAND
 *   2. Mutant Node: beam entropy cao → sinh temporary node từ LCG seed
 *
 * Phụ thuộc: core/eh_arena.h, hgn/eh_hgn_dag.h
 * Môi trường: C99, gcc -O3 -mavx2
 * ================================================================ */

#ifndef EH_HGN_COLLAPSE_H
#define EH_HGN_COLLAPSE_H

#include "../../include/core/eh_arena.h"
#include "../../include/hgn/eh_hgn_dag.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Hyperparameters — điều chỉnh theo domain khi distill
 * ---------------------------------------------------------------- */

/* cos(h_t, h_{t-1}) > COLLAPSE_THRESH → context tuyến tính → collapse */
#define EH_HGN_COLLAPSE_THRESH   0.92f

/* beam entropy (nats) > ENTROPY_THRESH → OOD → kích hoạt mutant   */
#define EH_HGN_ENTROPY_THRESH    1.80f

/* Scale perturbation vector: Xavier-like = 1/sqrt(dim)            */
#define EH_HGN_PERTURB_SCALE     0.088388f   /* 1/sqrt(128)         */

/* Max mutant nodes sống đồng thời trong 1 inference step           */
#define EH_HGN_MAX_MUTANTS       4u

/* ----------------------------------------------------------------
 * Collapse decision enum
 * ---------------------------------------------------------------- */
typedef enum {
    EH_HGN_EXPAND   = 0,   /* full DAG traversal                   */
    EH_HGN_COLLAPSE = 1,   /* mean pooling O(N), bỏ qua DAG        */
} EH_HGN_CollapseDecision;

/* ----------------------------------------------------------------
 * EH_HGN_MutantNode: temporary bridging node, sống 1 step
 *
 * Nằm trong EH_Arena — reset O(1) sau mỗi context step.
 * Không có malloc, không có free riêng lẻ.
 * ---------------------------------------------------------------- */
typedef struct {
    float    vec[EH_HGN_EMBED_DIM];  /* h_{t-1} + perturbation      */
    uint32_t src_token;              /* token kích hoạt mutant       */
    uint32_t dst_token;              /* token đích được bridge tới   */
    float    bridge_score;           /* score tạm của nhánh này      */
    bool     active;                 /* còn sống trong step này không */
} __attribute__((aligned(32))) EH_HGN_MutantNode;

/* ----------------------------------------------------------------
 * EH_HGN_CollapseCtx: context state cho 1 inference pass
 *
 * Caller giữ 1 instance này suốt quá trình generate.
 * Reset bằng eh_hgn_collapse_ctx_reset() khi bắt đầu prompt mới.
 * ---------------------------------------------------------------- */
typedef struct {
    /* --- State từ step trước --- */
    float    h_prev[EH_HGN_EMBED_DIM];  /* h_{t-1}, aligned 32-byte */
    bool     h_prev_valid;              /* false ở step đầu tiên    */

    /* --- Running stats --- */
    uint32_t step_count;               /* số steps đã xử lý         */
    uint32_t collapse_count;           /* số lần đã collapse         */
    uint32_t expand_count;             /* số lần expand full         */
    uint32_t mutant_count;             /* số mutant nodes đã sinh    */

    /* --- Mutant pool: arena-backed, lifetime = 1 step --- */
    EH_HGN_MutantNode *mutant_pool;    /* trỏ vào EH_Arena           */
    uint32_t           mutant_active;  /* số mutants đang sống       */

    /* --- Thresholds (copy từ #define, cho phép override) --- */
    float    collapse_thresh;
    float    entropy_thresh;
    float    perturb_scale;
} __attribute__((aligned(32))) EH_HGN_CollapseCtx;

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

/* Khởi tạo CollapseCtx, cấp phát mutant_pool từ arena.
 * Trả về 0 nếu thành công.                                         */
int eh_hgn_collapse_ctx_init(EH_HGN_CollapseCtx *ctx,
                              EH_Arena           *arena);

/* Reset state giữa các prompt (giữ pool, xóa h_prev và stats)     */
void eh_hgn_collapse_ctx_reset(EH_HGN_CollapseCtx *ctx);

/* ----------------------------------------------------------------
 * Core: quyết định COLLAPSE hay EXPAND cho h_t
 *
 * Logic:
 *   if (!h_prev_valid) → EXPAND (step đầu, chưa có reference)
 *   cos = dot(h_t, h_prev) / (|h_t| * |h_prev|)
 *   cos > collapse_thresh → COLLAPSE
 *   else                  → EXPAND
 *
 * Side effect: lưu h_t vào ctx->h_prev cho step tiếp theo.
 * ---------------------------------------------------------------- */
EH_HGN_CollapseDecision eh_hgn_collapse_gate(
        EH_HGN_CollapseCtx *ctx,
        const float        *h_t);        /* vector hiện tại dim=128 */

/* ----------------------------------------------------------------
 * Collapse path: mean pooling O(N) thay DAG traversal
 *
 * Tính trung bình embedding của tất cả neighbor edges từ token_id,
 * ghi kết quả vào out_vec[128].
 * Dùng khi CollapseDecision == EH_HGN_COLLAPSE.
 * ---------------------------------------------------------------- */
void eh_hgn_collapse_mean_pool(
        const EH_HGN_BaseDag *dag,
        uint32_t              token_id,
        float                *out_vec);  /* [EH_HGN_EMBED_DIM]      */

/* ----------------------------------------------------------------
 * Beam entropy: đo độ phân vân của beam distribution
 *
 * H = -Σ p_i * log(p_i) với p_i = softmax(scores[i])
 * Trả về entropy (nats). Nếu > entropy_thresh → kích hoạt mutant.
 * ---------------------------------------------------------------- */
float eh_hgn_beam_entropy(const float *scores, uint32_t count);

/* ----------------------------------------------------------------
 * Mutant Node: sinh và đăng ký vào ctx->mutant_pool
 *
 * seed    = (uint32_t)(top_score * 1000) XOR step_count
 * perturb = LCG(seed) → vec[128] ∈ [-scale, +scale]
 * mutant  = h_prev + perturb
 *
 * Trả về con trỏ tới mutant node vừa sinh, NULL nếu pool đầy.
 * Lifetime: tự xóa khi eh_hgn_collapse_step_end() được gọi.
 * ---------------------------------------------------------------- */
EH_HGN_MutantNode *eh_hgn_mutant_spawn(
        EH_HGN_CollapseCtx *ctx,
        uint32_t            src_token,
        uint32_t            dst_token,
        float               top_beam_score);

/* Kết thúc 1 step: deactivate toàn bộ mutant nodes hiện tại       */
void eh_hgn_collapse_step_end(EH_HGN_CollapseCtx *ctx);

/* Dump stats ra stderr                                             */
void eh_hgn_collapse_dump_stats(const EH_HGN_CollapseCtx *ctx);

/* ----------------------------------------------------------------
 * Internal: LCG pseudo-random cho perturbation (không dùng rand())
 *
 * Dùng Numerical Recipes constants:
 *   x_{n+1} = 1664525 * x_n + 1013904223  (mod 2^32)
 * Trả về float ∈ [-1, 1] từ 1 LCG step.
 * ---------------------------------------------------------------- */
static inline float _eh_hgn_lcg_float(uint32_t *state)
{
    *state = 1664525u * (*state) + 1013904223u;
    /* Map [0, 2^32) → [-1, 1] */
    return (float)(int32_t)(*state) / 2147483648.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* EH_HGN_COLLAPSE_H */