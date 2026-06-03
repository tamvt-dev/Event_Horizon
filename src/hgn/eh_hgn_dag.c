/* ================================================================
 * eh_hgn_dag.c — HGN Layer 1: Base DAG implementation
 *
 * Compile: gcc -O3 -std=c99 -mavx2 -Wall -Wextra \
 *              -I../../include -c eh_hgn_dag.c -o eh_hgn_dag.o
 * ================================================================ */

#define _POSIX_C_SOURCE 200112L

#include "../../include/hgn/eh_hgn_dag.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ----------------------------------------------------------------
 * eh_hgn_dag_load
 * ---------------------------------------------------------------- */
EH_HGN_Status eh_hgn_dag_load(EH_Arena       *arena,
                                const char     *path,
                                EH_HGN_BaseDag *out_dag)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[eh_hgn_dag] Cannot open '%s': %s\n",
                path, strerror(errno));
        return EH_HGN_ERR_IO;
    }

    /* -- Đọc và validate file header (32 bytes) -- */
    EH_HGN_DagFileHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_dag] Header read failed\n");
        fclose(fp); return EH_HGN_ERR_CORRUPT;
    }

    if (hdr.magic != EH_HGN_DAG_MAGIC) {
        fprintf(stderr, "[eh_hgn_dag] Bad magic: 0x%08X\n", hdr.magic);
        fclose(fp); return EH_HGN_ERR_MAGIC;
    }
    if (hdr.version != EH_HGN_DAG_VERSION) {
        fprintf(stderr, "[eh_hgn_dag] Version mismatch: %u\n", hdr.version);
        fclose(fp); return EH_HGN_ERR_VERSION;
    }
    if (hdr.vocab_size > EH_HGN_VOCAB_SIZE) {
        fprintf(stderr, "[eh_hgn_dag] vocab_size %u > %u\n",
                hdr.vocab_size, EH_HGN_VOCAB_SIZE);
        fclose(fp); return EH_HGN_ERR_OVERFLOW;
    }
    if (hdr.total_edges > EH_HGN_MAX_EDGES) {
        fprintf(stderr, "[eh_hgn_dag] total_edges %u > %u\n",
                hdr.total_edges, EH_HGN_MAX_EDGES);
        fclose(fp); return EH_HGN_ERR_OVERFLOW;
    }
    if (hdr.embed_dim != EH_HGN_EMBED_DIM) {
        fprintf(stderr, "[eh_hgn_dag] embed_dim %u != %u\n",
                hdr.embed_dim, EH_HGN_EMBED_DIM);
        fclose(fp); return EH_HGN_ERR_OVERFLOW;
    }
    if (hdr.max_fanout > EH_HGN_MAX_FANOUT) {
        fprintf(stderr, "[eh_hgn_dag] max_fanout %u > %u\n",
                hdr.max_fanout, EH_HGN_MAX_FANOUT);
        fclose(fp); return EH_HGN_ERR_OVERFLOW;
    }

    /* -- Tính kích thước từng vùng -- */
    size_t sz_embed   = (size_t)hdr.vocab_size  * sizeof(EH_HGN_NodeEmbed);
    size_t sz_adj     = (size_t)hdr.vocab_size  * sizeof(EH_HGN_NodeAdj);
    size_t sz_compact = (size_t)hdr.total_edges * sizeof(EH_HGN_EdgeCompact);
    size_t sz_weight  = (size_t)hdr.total_edges * sizeof(EH_HGN_EdgeWeight);

    /* -- Cấp phát từ arena (aligned 32-byte cho node_embed và weight_pool) -- */
    EH_HGN_NodeEmbed   *node_embed   = eh_arena_alloc(arena, sz_embed);
    EH_HGN_NodeAdj     *node_adj     = eh_arena_alloc(arena, sz_adj);
    EH_HGN_EdgeCompact *edge_compact = eh_arena_alloc(arena, sz_compact);
    EH_HGN_EdgeWeight  *weight_pool  = eh_arena_alloc(arena, sz_weight);

    if (!node_embed || !node_adj || !edge_compact || !weight_pool) {
        size_t need_mb = (sz_embed + sz_adj + sz_compact + sz_weight) >> 20;
        size_t avail   = (arena->capacity > arena->used)
                       ? (arena->capacity - arena->used) >> 20 : 0;
        fprintf(stderr, "[eh_hgn_dag] Arena OOM: need ~%zu MB, avail %zu MB\n",
                need_mb, avail);
        fclose(fp); return EH_HGN_ERR_ARENA;
    }

    /* -- fread zero-copy vào arena -- */
    if (fread(node_embed,   sz_embed,   1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_dag] node_embed read failed\n");
        fclose(fp); return EH_HGN_ERR_CORRUPT;
    }
    if (fread(node_adj,     sz_adj,     1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_dag] node_adj read failed\n");
        fclose(fp); return EH_HGN_ERR_CORRUPT;
    }
    if (fread(edge_compact, sz_compact, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_dag] edge_compact read failed\n");
        fclose(fp); return EH_HGN_ERR_CORRUPT;
    }

    /* -- Skip padding 32-byte trước weight_pool -- */
    long pos = ftell(fp);
    if (pos < 0) { fclose(fp); return EH_HGN_ERR_IO; }
    long aligned_pos = ((long)pos + 31L) & ~31L;
    if (aligned_pos != pos) {
        if (fseek(fp, aligned_pos, SEEK_SET) != 0) {
            fprintf(stderr, "[eh_hgn_dag] fseek padding failed\n");
            fclose(fp); return EH_HGN_ERR_IO;
        }
    }

    if (fread(weight_pool, sz_weight, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_dag] weight_pool read failed\n");
        fclose(fp); return EH_HGN_ERR_CORRUPT;
    }
    fclose(fp);

    /* -- CSR integrity check: O(V+E), chạy 1 lần sau load -- */
    for (uint32_t i = 0; i < hdr.vocab_size; i++) {
        const EH_HGN_NodeAdj *adj = &node_adj[i];

        if (adj->edge_offset > hdr.total_edges ||
            adj->edge_count  > EH_HGN_MAX_FANOUT ||
            (uint64_t)adj->edge_offset + adj->edge_count > hdr.total_edges)
        {
            fprintf(stderr, "[eh_hgn_dag] CSR corrupt at node %u\n", i);
            return EH_HGN_ERR_CORRUPT;
        }

        /* Validate từng edge: dst hợp lệ, weight_idx hợp lệ      */
        const EH_HGN_EdgeCompact *e     = edge_compact + adj->edge_offset;
        const EH_HGN_EdgeCompact *e_end = e + adj->edge_count;
        for (; e != e_end; e++) {
            if (e->dst >= hdr.vocab_size) {
                fprintf(stderr, "[eh_hgn_dag] Edge dst %u OOB at node %u\n",
                        e->dst, i);
                return EH_HGN_ERR_CORRUPT;
            }
            if (e->weight_idx >= hdr.total_edges) {
                fprintf(stderr, "[eh_hgn_dag] weight_idx %u OOB at node %u\n",
                        e->weight_idx, i);
                return EH_HGN_ERR_CORRUPT;
            }
        }
    }

    /* -- Ghi kết quả -- */
    out_dag->vocab_size   = hdr.vocab_size;
    out_dag->total_edges  = hdr.total_edges;
    out_dag->embed_dim    = hdr.embed_dim;
    out_dag->max_fanout   = hdr.max_fanout;
    out_dag->node_embed   = node_embed;
    out_dag->node_adj     = node_adj;
    out_dag->edge_compact = edge_compact;
    out_dag->weight_pool  = weight_pool;

    return EH_HGN_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_dag_dump_info
 * ---------------------------------------------------------------- */
void eh_hgn_dag_dump_info(const EH_HGN_BaseDag *dag)
{
    if (!dag) { fprintf(stderr, "[eh_hgn_dag] NULL dag\n"); return; }

    uint32_t fanout_min = EH_HGN_MAX_FANOUT;
    uint32_t fanout_max = 0;
    uint64_t fanout_sum = 0;
    uint32_t sink_nodes = 0;

    for (uint32_t i = 0; i < dag->vocab_size; i++) {
        uint32_t cnt = dag->node_adj[i].edge_count;
        if (cnt == 0)         sink_nodes++;
        if (cnt < fanout_min) fanout_min = cnt;
        if (cnt > fanout_max) fanout_max = cnt;
        fanout_sum += cnt;
    }

    double avg = dag->vocab_size ? (double)fanout_sum / dag->vocab_size : 0.0;

    /* Tính RAM thực tế (bytes → MB với >> 20)                     */
    size_t b_embed   = (size_t)dag->vocab_size  * sizeof(EH_HGN_NodeEmbed);
    size_t b_adj     = (size_t)dag->vocab_size  * sizeof(EH_HGN_NodeAdj);
    size_t b_compact = (size_t)dag->total_edges * sizeof(EH_HGN_EdgeCompact);
    size_t b_weight  = (size_t)dag->total_edges * sizeof(EH_HGN_EdgeWeight);

    fprintf(stderr,
        "=== EH_HGN_BaseDag ===\n"
        "  vocab_size   : %u\n"
        "  total_edges  : %u\n"
        "  embed_dim    : %u\n"
        "  max_fanout   : %u (cap K)\n"
        "  fan-out      : min=%u  max=%u  avg=%.2f\n"
        "  sink nodes   : %u\n"
        "  RAM embed    : %zu MB\n"
        "  RAM adj      : %zu KB\n"
        "  RAM compact  : %zu MB\n"
        "  RAM weight   : %zu MB\n"
        "  RAM total    : ~%zu MB\n"
        "======================\n",
        dag->vocab_size, dag->total_edges, dag->embed_dim, dag->max_fanout,
        fanout_min, fanout_max, avg, sink_nodes,
        b_embed   >> 20,
        b_adj     >> 10,
        b_compact >> 20,
        b_weight  >> 20,
        (b_embed + b_adj + b_compact + b_weight) >> 20
    );
}