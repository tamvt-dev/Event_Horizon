/* ================================================================
 * eh_hgn_engine.h — HGN Layer 4: Unified Inference Engine
 *
 * Wiring layer kết nối tất cả HGN components:
 *   - Layer 1: CSR DAG (eh_hgn_dag)
 *   - Layer 2: Collapse Gating (eh_hgn_collapse)
 *   - Layer 3: Beam Search (eh_beam_search)
 *
 * Mỗi inference step:
 *   1. Beam expansion với AVX2 scoring
 *   2. Collapse gating quyết định COLLAPSE/EXPAND
 *   3. Mutant spawning nếu beam entropy cao
 *   4. Trả về toàn bộ beam array (caller tự chọn top-1)
 *
 * API Design: Stateful session-based, zero malloc per step.
 * ================================================================ */

#ifndef EH_HGN_ENGINE_H
#define EH_HGN_ENGINE_H

#include "eh_hgn_dag.h"
#include "eh_hgn_collapse.h"
#include "eh_beam_search.h"
#include "../../include/core/eh_arena.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Engine Configuration
 * ---------------------------------------------------------------- */
typedef struct {
    /* Collapse gating thresholds */
    float collapse_thresh;      /* Default: 0.92  */
    float entropy_thresh;       /* Default: 1.80  */
    float perturb_scale;        /* Default: 0.088 */
    
    /* Beam search config */
    uint32_t beam_width;        /* Beam search width (default: 8) */
    uint32_t max_steps;         /* Max generation steps (0=unlimited) */
    bool     enable_collapse;   /* Enable/disable collapse gating     */
    bool     enable_mutants;    /* Enable/disable mutant nodes        */
} EH_HGN_EngineConfig;

/* ----------------------------------------------------------------
 * Inference Session: toàn bộ state cho 1 generation pass
 *
 * Caller khởi tạo session với prompt, sau đó gọi step() lặp lại
 * cho đến khi generation hoàn thành hoặc max_steps đạt.
 * ---------------------------------------------------------------- */
typedef struct {
    /* --- Layer references --- */
    const EH_HGN_BaseDag   *dag;            /* Layer 1: CSR graph     */
    EH_HGN_CollapseCtx      collapse_ctx;   /* Layer 2: Gating state  */
    EH_HGN_BeamTracker      beam_tracker;   /* Layer 3: Beam state    */
    
    /* --- Session config --- */
    EH_HGN_EngineConfig     config;
    
    /* --- Generation state --- */
    uint32_t                step_count;     /* Số steps đã thực hiện  */
    bool                    is_finished;    /* All beams terminated?  */
    
    /* --- Temp workspace (aligned) --- */
    float                   h_current[EH_HGN_EMBED_DIM] __attribute__((aligned(32)));
    float                   h_pooled[EH_HGN_EMBED_DIM]  __attribute__((aligned(32)));
} __attribute__((aligned(64))) EH_HGN_InferenceSession;

/* ----------------------------------------------------------------
 * API: Session Lifecycle
 * ---------------------------------------------------------------- */

/* Khởi tạo engine session với prompt sequence.
 * Arena phải đủ lớn cho DAG + collapse context (ước tính ~DAG size + 16KB).
 * 
 * Returns: 0 on success, -1 on error.
 */
int eh_hgn_session_init(
        EH_HGN_InferenceSession *session,
        const EH_HGN_BaseDag    *dag,
        EH_Arena                *arena,
        const uint32_t          *prompt,
        uint32_t                 prompt_len,
        const EH_HGN_EngineConfig *config);  /* NULL = use defaults */

/* Reset session để bắt đầu prompt mới (giữ nguyên config và arena) */
void eh_hgn_session_reset(
        EH_HGN_InferenceSession *session,
        const uint32_t          *prompt,
        uint32_t                 prompt_len);

/* ----------------------------------------------------------------
 * API: Inference Step
 * ---------------------------------------------------------------- */

/* Thực hiện 1 bước autoregressive generation:
 * 
 * Pipeline:
 *   1. Lấy last token của beam paths hiện tại
 *   2. Tính h_current từ node embedding
 *   3. Collapse gate: quyết định COLLAPSE vs EXPAND
 *      - COLLAPSE: dùng mean_pool → h_pooled
 *      - EXPAND:   full DAG traversal với beam expansion
 *   4. Beam expansion với AVX2 scoring
 *   5. Tính beam entropy → spawn mutants nếu > threshold
 *   6. Top-K selection → update beam_tracker
 *   7. Cleanup mutants sau mỗi step
 * 
 * Returns:
 *   - Số beams còn active (chưa finished)
 *   - 0 = tất cả beams đã terminate (generation done)
 *   - session->is_finished được set = true khi return 0
 */
uint32_t eh_hgn_session_step(EH_HGN_InferenceSession *session);

/* ----------------------------------------------------------------
 * API: Results Access
 * ---------------------------------------------------------------- */

/* Lấy toàn bộ beam array hiện tại (K=4 beams).
 * Caller tự chọn beam tốt nhất dựa trên score hoặc heuristic khác.
 * 
 * Returns: pointer to EH_HGN_BeamPath[EH_BEAM_WIDTH], never NULL.
 */
const EH_HGN_BeamPath *eh_hgn_session_get_beams(
        const EH_HGN_InferenceSession *session);

/* Lấy beam tốt nhất (highest score).
 * Shortcut cho eh_hgn_session_get_beams()[0] sau khi sort.
 * 
 * Returns: pointer to best beam, NULL nếu session chưa init.
 */
const EH_HGN_BeamPath *eh_hgn_session_get_best(
        const EH_HGN_InferenceSession *session);

/* Kiểm tra xem generation đã hoàn thành chưa */
static inline bool eh_hgn_session_is_done(
        const EH_HGN_InferenceSession *session)
{
    return session->is_finished;
}

/* ----------------------------------------------------------------
 * API: Stats & Debugging
 * ---------------------------------------------------------------- */

/* Dump toàn bộ stats của session (collapse counts, mutant counts, etc.) */
void eh_hgn_session_dump_stats(const EH_HGN_InferenceSession *session);

/* Dump current beam states với scores và sequences */
void eh_hgn_session_dump_beams(const EH_HGN_InferenceSession *session);

/* ----------------------------------------------------------------
 * Utilities: Default Config
 * ---------------------------------------------------------------- */

/* Tạo config mặc định với các thresholds chuẩn */
static inline EH_HGN_EngineConfig eh_hgn_default_config(void)
{
    EH_HGN_EngineConfig cfg = {
        .collapse_thresh  = EH_HGN_COLLAPSE_THRESH,   /* 0.92  */
        .entropy_thresh   = EH_HGN_ENTROPY_THRESH,    /* 1.80  */
        .perturb_scale    = EH_HGN_PERTURB_SCALE,     /* 0.088 */
        .beam_width       = 8,                        /* default beam width */
        .max_steps        = 0,                        /* unlimited */
        .enable_collapse  = true,
        .enable_mutants   = true,
    };
    return cfg;
}

#ifdef __cplusplus
}
#endif

#endif /* EH_HGN_ENGINE_H */
