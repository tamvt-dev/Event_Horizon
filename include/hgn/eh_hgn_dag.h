/* ================================================================
 * eh_hgn_dag.h — HGN Layer 1: Base DAG (Compressed Sparse Row)
 *
 * Namespace  : EH_HGN_* (phân biệt với eh_dag.h của Game AI core)
 * Phụ thuộc  : core/eh_arena.h, core/eh_scoring.h
 * Môi trường : C99, gcc -O3 -mavx2
 *
 * Memory layout (Split — Option B):
 *   node_embed   [V × 512B,  align-32]  — vectors ngữ nghĩa
 *   node_adj     [V × 8B,    align-8 ]  — CSR offsets
 *   edge_compact [E × 12B,  align-4 ]  — dst + prior + weight_idx
 *   weight_pool  [E × 512B, align-32]  — transition weights AVX2
 *
 * Chiến lược scan hai pha:
 *   Pha 1: duyệt edge_compact (12B/edge) → filter by prior
 *   Pha 2: load weight_pool chỉ cho top candidates → AVX2 scoring
 * ================================================================ */

#ifndef EH_HGN_DAG_H
#define EH_HGN_DAG_H

#include "../../include/core/eh_arena.h"
#include "../../include/core/eh_scoring.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define EH_HGN_VOCAB_SIZE   32768u   /* 2^15 — fit L3 cache        */
#define EH_HGN_EMBED_DIM    128u     /* = EH_SCORE_DIM             */
#define EH_HGN_MAX_FANOUT   32u      /* K: max edges/node          */
#define EH_HGN_MAX_EDGES    (EH_HGN_VOCAB_SIZE * EH_HGN_MAX_FANOUT)

/* Compile-time guard: EMBED_DIM phải khớp với scoring core       */
_Static_assert(EH_HGN_EMBED_DIM == EH_SCORE_DIM,
               "EH_HGN_EMBED_DIM must equal EH_SCORE_DIM (128)");

/* ----------------------------------------------------------------
 * File format: ehdag.bin
 *
 * [EH_HGN_DagFileHeader  — 32 bytes                  ]
 * [EH_HGN_NodeEmbed × vocab_size — align-32           ]
 * [EH_HGN_NodeAdj   × vocab_size                      ]
 * [EH_HGN_EdgeCompact × total_edges                   ]
 * [padding đến 32-byte boundary                        ]
 * [EH_HGN_EdgeWeight × total_edges — align-32         ]
 * ---------------------------------------------------------------- */
#define EH_HGN_DAG_MAGIC    0x48474E44u   /* "HGND" little-endian  */
#define EH_HGN_DAG_VERSION  1u

/* ----------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------- */

/* Node embedding: vector 128D, aligned 32-byte cho _mm256_load_ps */
typedef struct {
    float vec[EH_HGN_EMBED_DIM];
} __attribute__((aligned(32))) EH_HGN_NodeEmbed;

/* CSR adjacency: offset + count cho edges của 1 node             */
typedef struct {
    uint32_t edge_offset;   /* index đầu trong edge_compact[]      */
    uint32_t edge_count;    /* số edges, ≤ EH_HGN_MAX_FANOUT       */
} EH_HGN_NodeAdj;

/* Edge compact: 12 bytes — dùng khi scan/filter (không cần weight)
 * Thứ tự field: dst trước để branch predict tốt khi filter dst   */
typedef struct {
    uint32_t dst;           /* token ID đích                       */
    float    prior;         /* scalar prior từ Teacher (co-occur)  */
    uint32_t weight_idx;    /* index vào weight_pool[]             */
} EH_HGN_EdgeCompact;       /* 12 bytes, no padding               */

/* Edge weight: 512 bytes, aligned 32-byte — chỉ load khi scoring */
typedef struct {
    float vec[EH_HGN_EMBED_DIM];
} __attribute__((aligned(32))) EH_HGN_EdgeWeight;

/* File header: 32 bytes chính xác                                */
typedef struct {
    uint32_t magic;         /* EH_HGN_DAG_MAGIC                    */
    uint32_t version;       /* EH_HGN_DAG_VERSION                  */
    uint32_t vocab_size;    /* số token thực tế ≤ EH_HGN_VOCAB_SIZE*/
    uint32_t total_edges;   /* số edges thực tế ≤ EH_HGN_MAX_EDGES */
    uint32_t embed_dim;     /* phải == EH_HGN_EMBED_DIM            */
    uint32_t max_fanout;    /* phải ≤ EH_HGN_MAX_FANOUT            */
    uint32_t reserved[2];   /* padding cho 32 bytes                */
} EH_HGN_DagFileHeader;     /* sizeof == 32 bytes                 */

_Static_assert(sizeof(EH_HGN_DagFileHeader) == 32,
               "EH_HGN_DagFileHeader must be 32 bytes");

/* ----------------------------------------------------------------
 * EH_HGN_BaseDag: handle toàn bộ đồ thị
 * Sau load(), tất cả con trỏ trỏ vào EH_Arena — không có heap    */
typedef struct {
    uint32_t            vocab_size;
    uint32_t            total_edges;
    uint32_t            embed_dim;
    uint32_t            max_fanout;

    EH_HGN_NodeEmbed   *node_embed;    /* [vocab_size]             */
    EH_HGN_NodeAdj     *node_adj;      /* [vocab_size]             */
    EH_HGN_EdgeCompact *edge_compact;  /* [total_edges]            */
    EH_HGN_EdgeWeight  *weight_pool;   /* [total_edges]            */
} EH_HGN_BaseDag;

/* ----------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------- */
typedef enum {
    EH_HGN_OK           = 0,
    EH_HGN_ERR_IO       = 1,   /* không mở được file              */
    EH_HGN_ERR_MAGIC    = 2,   /* sai magic                       */
    EH_HGN_ERR_VERSION  = 3,   /* version không tương thích       */
    EH_HGN_ERR_OVERFLOW = 4,   /* vocab/edges vượt compile limits */
    EH_HGN_ERR_ARENA    = 5,   /* arena OOM                       */
    EH_HGN_ERR_CORRUPT  = 6,   /* dữ liệu không nhất quán         */
} EH_HGN_Status;

/* ----------------------------------------------------------------
 * API — Load / Query
 * ---------------------------------------------------------------- */

/* Nạp Base DAG từ binary file vào arena.
 * Zero-copy: fread thẳng vào arena, không malloc/memcpy thêm.
 * Trả về EH_HGN_OK nếu thành công.                               */
EH_HGN_Status eh_hgn_dag_load(EH_Arena           *arena,
                               const char         *path,
                               EH_HGN_BaseDag     *out_dag);

/* Dump thống kê DAG ra stderr (fan-out distribution, RAM usage)  */
void eh_hgn_dag_dump_info(const EH_HGN_BaseDag *dag);

/* ----------------------------------------------------------------
 * Inline accessors — hot path, zero function call overhead
 * ---------------------------------------------------------------- */

/* Embedding vector của token_id. NULL nếu OOB.                   */
static inline const float *
eh_hgn_dag_node_vec(const EH_HGN_BaseDag *dag, uint32_t token_id)
{
    if (__builtin_expect(token_id >= dag->vocab_size, 0)) return NULL;
    return dag->node_embed[token_id].vec;
}

/* Iterator: đầu danh sách edge compact của node_id.              */
static inline const EH_HGN_EdgeCompact *
eh_hgn_dag_edges_begin(const EH_HGN_BaseDag *dag, uint32_t node_id)
{
    if (__builtin_expect(node_id >= dag->vocab_size, 0)) return NULL;
    return dag->edge_compact + dag->node_adj[node_id].edge_offset;
}

/* Iterator: past-the-end của edge list node_id.                  */
static inline const EH_HGN_EdgeCompact *
eh_hgn_dag_edges_end(const EH_HGN_BaseDag *dag, uint32_t node_id)
{
    if (__builtin_expect(node_id >= dag->vocab_size, 0)) return NULL;
    const EH_HGN_NodeAdj *adj = &dag->node_adj[node_id];
    return dag->edge_compact + adj->edge_offset + adj->edge_count;
}

/* Fan-out thực tế của node_id.                                   */
static inline uint32_t
eh_hgn_dag_fanout(const EH_HGN_BaseDag *dag, uint32_t node_id)
{
    if (__builtin_expect(node_id >= dag->vocab_size, 0)) return 0;
    return dag->node_adj[node_id].edge_count;
}

/* Weight vector của một edge — chỉ gọi sau khi filter xong.
 * Dùng với eh_scoring_dot_avx2() từ core/eh_scoring.h           */
static inline const float *
eh_hgn_dag_edge_weight(const EH_HGN_BaseDag    *dag,
                        const EH_HGN_EdgeCompact *edge)
{
    return dag->weight_pool[edge->weight_idx].vec;
}

/* ----------------------------------------------------------------
 * Convenience macro: duyệt toàn bộ edges của node_id
 *
 * Dùng:
 *   EH_HGN_FOR_EDGES(dag, node_id, e) {
 *       // e là const EH_HGN_EdgeCompact*
 *   }
 * ---------------------------------------------------------------- */
#define EH_HGN_FOR_EDGES(dag, node_id, e)                          \
    for (const EH_HGN_EdgeCompact                                   \
            *e       = eh_hgn_dag_edges_begin((dag), (node_id)),    \
            *_e_end_ = eh_hgn_dag_edges_end((dag), (node_id));      \
         e != NULL && e != _e_end_;                                 \
         e++)

#ifdef __cplusplus
}
#endif

#endif /* EH_HGN_DAG_H */