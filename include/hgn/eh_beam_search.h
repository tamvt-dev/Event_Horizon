/* ================================================================
 * eh_beam_search.h — HGN Layer 3: Beam Search Autoregressive
 *
 * Beam Search với AVX2-accelerated scoring trên CSR DAG.
 * Autoregressive generation: mỗi step mở rộng K beams tốt nhất.
 *
 * EOS Support: Nodes không có edges (sink nodes) tự động được coi
 * là terminal. Khi tất cả beams đều terminal, generation kết thúc.
 * ================================================================ */

#ifndef EH_BEAM_SEARCH_H
#define EH_BEAM_SEARCH_H

#include "eh_hgn_dag.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define EH_BEAM_WIDTH       4       /* Số beams giữ lại mỗi step      */
#define EH_BEAM_MAX_LEN     64      /* Max sequence length             */
#define EH_BEAM_EOS_TOKEN   UINT32_MAX  /* Sentinel: explicit EOS token */

/* ----------------------------------------------------------------
 * Beam Path: một chuỗi token candidates
 * is_finished = true khi beam đã gặp node sink (0 edges) hoặc EOS.
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t tokens[EH_BEAM_MAX_LEN];   /* Token IDs trong sequence */
    uint32_t seq_len;                   /* Độ dài hiện tại          */
    float    score;                     /* Cumulative score          */
    bool     is_finished;               /* True nếu đã gặp EOS/sink */
} EH_HGN_BeamPath;

/* ----------------------------------------------------------------
 * Beam Tracker: quản lý tất cả beams đang active
 * ---------------------------------------------------------------- */
typedef struct {
    EH_HGN_BeamPath paths[EH_BEAM_WIDTH];  /* Active beams (kể cả finished) */
    uint32_t        active_paths;           /* Số beams hiện tại             */
    const EH_HGN_BaseDag *dag;              /* Reference to DAG              */
} EH_HGN_BeamTracker;

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

/* Initialize beam tracker với prompt sequence */
void eh_hgn_beam_init(EH_HGN_BeamTracker    *tracker,
                      const EH_HGN_BaseDag   *dag,
                      const uint32_t         *prompt,
                      uint32_t                prompt_len);

/* Thực hiện 1 bước autoregressive expansion
 * - Beams đã is_finished=true được giữ nguyên (không expand)
 * - Mở rộng beams còn active với các edges từ last token
 * - Score = parent_score + edge.prior + dot(edge_weight, context_vec)
 * - Giữ lại top-K beams (finished + unfinished, sort by score)
 * - Beams gặp sink node (fanout=0) tự động đặt is_finished=true
 *
 * Returns: số beams vẫn đang active (chưa finished). 0 = generation done.
 */
uint32_t eh_hgn_beam_step(EH_HGN_BeamTracker *tracker,
                           const EH_HGN_BaseDag *dag);

/* Lấy beam tốt nhất (highest score, bất kể finished hay không) */
const EH_HGN_BeamPath *eh_hgn_beam_get_best(const EH_HGN_BeamTracker *tracker);

/* Reset tracker để bắt đầu sequence mới */
void eh_hgn_beam_reset(EH_HGN_BeamTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif /* EH_BEAM_SEARCH_H */
