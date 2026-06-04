/* ================================================================
 * eh_hgn_dag.c — HGN Layer 1: Base DAG implementation
 *
 * Binary I/O moved to eh_hgn_io.c for separation of concerns.
 * This file now only contains DAG-specific utilities.
 *
 * Compile: gcc -O3 -std=c99 -mavx2 -Wall -Wextra \
 *              -I../../include -c eh_hgn_dag.c -o eh_hgn_dag.o
 * ================================================================ */

#include "../../include/hgn/eh_hgn_dag.h"
#include "../../include/hgn/eh_hgn_io.h"

#include <stdio.h>

/* ----------------------------------------------------------------
 * eh_hgn_dag_load - Wrapper for backward compatibility
 * ---------------------------------------------------------------- */
EH_HGN_Status eh_hgn_dag_load(EH_Arena       *arena,
                                const char     *path,
                                EH_HGN_BaseDag *out_dag)
{
    /* Delegate to I/O layer */
    EH_HGN_IO_Status io_status = eh_hgn_io_load(arena, path, out_dag);
    
    /* Map I/O status to DAG status (same enum values) */
    return (EH_HGN_Status)io_status;
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