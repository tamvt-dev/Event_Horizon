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
#define EH_BEAM_WIDTH       8       /* Số beams giữ lại mỗi step      */
#define EH_BEAM_MAX_LEN     64      /* Max sequence length             */
#define EH_BEAM_EOS_TOKEN   UINT32_MAX  /* Sentinel: explicit EOS token */

/* Decoding parameters - now runtime-tunable! */
extern float eh_beam_rep_penalty;      /* Additive repetition penalty (default: 5.0) */
extern float eh_beam_temperature;      /* Temperature for diversity (default: 0.6) */

#define EH_BEAM_CYCLE_PENALTY   80.0f   /* Additive cycle penalty (subtracted) */
#define EH_BEAM_REP_WINDOW      8       /* Look back N tokens for repetition   */
#define EH_BEAM_CONTEXT_WINDOW  12      /* Use N recent tokens for context     */

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
    EH_HGN_BeamPath *paths;                 /* Active beams (dynamically allocated)  */
    uint32_t        beam_width;             /* Cấu hình beam width tại runtime       */
    uint32_t        active_paths;           /* Số beams hiện tại             */
    const EH_HGN_BaseDag *dag;              /* Reference to DAG              */
    float           context_vec[EH_HGN_EMBED_DIM]; /* Pre-computed stable context vector */
    const uint8_t   *node_domains;          /* Mapping of node (pair) ID to DomainID */
    uint32_t        target_domain;          /* Target DomainID for guiding search */
} EH_HGN_BeamTracker;

/* ----------------------------------------------------------------
 * EH-G3: Query Context for advanced scoring
 * 
 * Enables:
 *   1. Full question embedding (for contrastive training)
 *   2. Hub-aware node frequency normalization
 *   3. Extensible for future: entity linking, semantic routing, etc.
 * ---------------------------------------------------------------- */
typedef struct {
    float   query_embedding[EH_HGN_EMBED_DIM];   /* Full question vector    */
    float   hub_penalty;                         /* Penalty for high-fanout */
    uint32_t max_node_fanout;                    /* Track largest hub       */
} EH_QueryContext;

/* ----------------------------------------------------------------
 * Global Question Embedding for Attention (EH-G2/G3)
 * 
 * External applications can set this to enable attention-based scoring.
 * When set, beam search will use this embedding as the "query" vector
 * instead of just recent token context.
 * 
 * Mixing ratios:
 *   eh_beam_attention_mix = 0.0 → pure local (EH-G1)
 *   eh_beam_attention_mix = 0.3 → 70% local + 30% global (default)
 *   eh_beam_attention_mix = 0.5 → 50% local + 50% global (balanced)
 *   eh_beam_attention_mix = 1.0 → pure global (experimental)
 * ---------------------------------------------------------------- */
extern float eh_beam_question_embedding[EH_HGN_EMBED_DIM];
extern int eh_beam_use_question_embedding;
extern float eh_beam_attention_mix;  /* Default: 0.3 */

/* ----------------------------------------------------------------
 * EH-G3: Hub Penalty & Query Context
 * ---------------------------------------------------------------- */
typedef struct {
    float hub_penalty;          /* Penalty strength for hub nodes (default: 2.0) */
    uint32_t hub_threshold;     /* Fanout threshold to consider "hub" (default: 10) */
    uint32_t max_node_fanout;   /* Max fanout in DAG (cached for normalization) */
} EH_BeamQueryContext;

extern int eh_beam_use_hub_penalty;
extern EH_BeamQueryContext eh_beam_query_ctx;

/* ----------------------------------------------------------------
 * EH-G3: Load Pre-trained Query Embeddings
 * Load embeddings from binary file for all vocabulary tokens.
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------- */
int eh_beam_load_query_embeddings(const char *path);

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

/* Initialize beam tracker với prompt sequence */
void eh_hgn_beam_init(EH_HGN_BeamTracker    *tracker,
                      const EH_HGN_BaseDag   *dag,
                      const uint32_t         *prompt,
                      uint32_t                prompt_len,
                      EH_HGN_BeamPath        *paths_buffer,
                      uint32_t                beam_width);

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

/* ----------------------------------------------------------------
 * EH-G3: Load query embeddings from binary file
 * 
 * File format (little-endian):
 *   [uint32_t] vocab_size
 *   [uint32_t] embed_dim
 *   [floats] embeddings (vocab_size * embed_dim values)
 * 
 * Returns: 0 on success, -1 on error
 * Side effects: populates eh_beam_question_embedding if successful
 * ---------------------------------------------------------------- */
int eh_beam_load_query_embeddings(const char *bin_file);

#ifdef __cplusplus
}
#endif

#endif /* EH_BEAM_SEARCH_H */
