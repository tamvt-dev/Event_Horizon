/* ================================================================
 * tests/test_eh_hgn_dag.c — Unit test Layer 1: EH_HGN_BaseDag
 *
 * Compile từ thư mục EventHorizon/:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra \
 *       -I include \
 *       tests/test_eh_hgn_dag.c src/hgn/eh_hgn_dag.c \
 *       -o tests/test_eh_hgn_dag -lm && tests/test_eh_hgn_dag
 * ================================================================ */

#define _POSIX_C_SOURCE 200112L

#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Arena globals for test
 * ---------------------------------------------------------------- */
static EH_Arena *g_arena_ptr = NULL;

static int arena_init(size_t cap)
{
    g_arena_ptr = eh_arena_create(cap);
    return (g_arena_ptr != NULL) ? 0 : -1;
}

static void arena_destroy(void) 
{ 
    if (g_arena_ptr) {
        eh_arena_destroy(g_arena_ptr);
        g_arena_ptr = NULL;
    }
}

/* ----------------------------------------------------------------
 * Test framework
 * ---------------------------------------------------------------- */
static int g_run = 0, g_pass = 0;

#define ASSERT(cond, msg) do {                                        \
    g_run++;                                                          \
    if (!(cond)) fprintf(stderr, "  [FAIL] %s (L%d)\n", msg, __LINE__);\
    else { fprintf(stderr, "  [PASS] %s\n", msg); g_pass++; }        \
} while(0)

#define ASSERT_EQ(a,b,msg)  ASSERT((a)==(b), msg)
#define ASSERT_NEQ(a,b,msg) ASSERT((a)!=(b), msg)
#define ASSERT_FEQ(a,b,msg) ASSERT(fabsf((a)-(b)) < 1e-4f, msg)

/* ----------------------------------------------------------------
 * Tạo dummy ehdag.bin: vocab=8, edges=16, fanout=2
 * ---------------------------------------------------------------- */
static const char *BIN = "/tmp/test_eh_hgn_dag.bin";

static int build_dummy(void)
{
    const uint32_t V = 8, E = 16, K = 2;

    FILE *fp = fopen(BIN, "wb");
    if (!fp) return -1;

    /* Header */
    EH_HGN_DagFileHeader hdr = {
        .magic       = EH_HGN_DAG_MAGIC,
        .version     = EH_HGN_DAG_VERSION,
        .vocab_size  = V,
        .total_edges = E,
        .embed_dim   = EH_HGN_EMBED_DIM,
        .max_fanout  = K,
        .reserved    = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* node_embed: vec[k] = i*128 + k */
    EH_HGN_NodeEmbed ne;
    for (uint32_t i = 0; i < V; i++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ne.vec[k] = (float)(i * EH_HGN_EMBED_DIM + k);
        fwrite(&ne, sizeof(ne), 1, fp);
    }

    /* node_adj: CSR liên tục, 2 edges/node */
    EH_HGN_NodeAdj adj;
    for (uint32_t i = 0; i < V; i++) {
        adj.edge_offset = i * K;
        adj.edge_count  = K;
        fwrite(&adj, sizeof(adj), 1, fp);
    }

    /* edge_compact: dst=(i+1)%V và (i+2)%V */
    EH_HGN_EdgeCompact ec;
    for (uint32_t i = 0; i < V; i++) {
        ec.dst        = (i + 1) % V;
        ec.prior      = 0.1f * (float)(i * K);
        ec.weight_idx = i * K;
        fwrite(&ec, sizeof(ec), 1, fp);

        ec.dst        = (i + 2) % V;
        ec.prior      = 0.1f * (float)(i * K + 1);
        ec.weight_idx = i * K + 1;
        fwrite(&ec, sizeof(ec), 1, fp);
    }

    /* Padding đến 32-byte boundary */
    long pos = ftell(fp);
    long apos = (pos + 31L) & ~31L;
    if (apos != pos) {
        uint8_t pad[32] = {0};
        fwrite(pad, (size_t)(apos - pos), 1, fp);
    }

    /* weight_pool: vec[k] = e*128 + k + 1000 */
    EH_HGN_EdgeWeight ew;
    for (uint32_t e = 0; e < E; e++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ew.vec[k] = (float)(e * EH_HGN_EMBED_DIM + k + 1000);
        fwrite(&ew, sizeof(ew), 1, fp);
    }

    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    fprintf(stderr, "\n=== EH_HGN_BaseDag Tests ===\n\n");

    ASSERT(build_dummy() == 0,        "T0: build dummy bin");
    ASSERT(arena_init(4*1024*1024)==0, "T0: arena 4MB init");

    /* T1: Load */
    fprintf(stderr, "\n-- T1: Load --\n");
    EH_HGN_BaseDag dag; memset(&dag, 0, sizeof(dag));
    EH_HGN_Status rc = eh_hgn_dag_load(g_arena_ptr, BIN, &dag);
    ASSERT_EQ(rc, EH_HGN_OK, "T1: load returns EH_HGN_OK");

    /* T2: Fields */
    fprintf(stderr, "\n-- T2: Header fields --\n");
    ASSERT_EQ(dag.vocab_size,  8u,  "T2: vocab_size");
    ASSERT_EQ(dag.total_edges, 16u, "T2: total_edges");
    ASSERT_EQ(dag.embed_dim,  128u, "T2: embed_dim");
    ASSERT_EQ(dag.max_fanout,  2u,  "T2: max_fanout");
    ASSERT_NEQ((uintptr_t)dag.node_embed,  0u, "T2: node_embed ptr");
    ASSERT_NEQ((uintptr_t)dag.edge_compact,0u, "T2: edge_compact ptr");
    ASSERT_NEQ((uintptr_t)dag.weight_pool, 0u, "T2: weight_pool ptr");

    /* T3: Edge iteration node 0 */
    fprintf(stderr, "\n-- T3: Edge iteration --\n");
    const EH_HGN_EdgeCompact *b = eh_hgn_dag_edges_begin(&dag, 0);
    const EH_HGN_EdgeCompact *e = eh_hgn_dag_edges_end(&dag, 0);
    ASSERT_EQ((uint32_t)(e - b), 2u,  "T3: fanout==2");
    ASSERT_EQ(b[0].dst, 1u,           "T3: edge[0].dst==1");
    ASSERT_EQ(b[1].dst, 2u,           "T3: edge[1].dst==2");
    ASSERT_EQ(b[0].weight_idx, 0u,    "T3: edge[0].weight_idx==0");
    ASSERT_EQ(b[1].weight_idx, 1u,    "T3: edge[1].weight_idx==1");
    ASSERT_FEQ(b[0].prior, 0.0f,      "T3: edge[0].prior==0.0");

    /* T4: node_vec */
    fprintf(stderr, "\n-- T4: node_vec --\n");
    const float *nv = eh_hgn_dag_node_vec(&dag, 3);
    ASSERT_NEQ((uintptr_t)nv, 0u,         "T4: node_vec(3)!=NULL");
    ASSERT_FEQ(nv[0],   384.0f,           "T4: nv[0]==384");
    ASSERT_FEQ(nv[127], 511.0f,           "T4: nv[127]==511");
    ASSERT_EQ((uintptr_t)eh_hgn_dag_node_vec(&dag, 8),  0u, "T4: OOB==NULL");
    ASSERT_EQ((uintptr_t)eh_hgn_dag_node_vec(&dag, 99999u), 0u, "T4: OOB2==NULL");

    /* T5: edge_weight */
    fprintf(stderr, "\n-- T5: edge_weight --\n");
    const float *ew = eh_hgn_dag_edge_weight(&dag, &b[1]);
    ASSERT_NEQ((uintptr_t)ew, 0u,    "T5: weight!=NULL");
    ASSERT_FEQ(ew[0],   1128.0f,     "T5: weight[1][0]==1128");
    ASSERT_FEQ(ew[127], 1255.0f,     "T5: weight[1][127]==1255");

    /* T6: EH_HGN_FOR_EDGES macro */
    fprintf(stderr, "\n-- T6: FOR_EDGES macro --\n");
    uint32_t cnt = 0;
    EH_HGN_FOR_EDGES(&dag, 5, edge) { cnt++; (void)edge; }
    ASSERT_EQ(cnt, 2u, "T6: FOR_EDGES count==2 for node 5");

    /* T7: full graph count */
    fprintf(stderr, "\n-- T7: Full graph iteration --\n");
    uint32_t total = 0;
    for (uint32_t i = 0; i < dag.vocab_size; i++) {
        EH_HGN_FOR_EDGES(&dag, i, ep) { total++; (void)ep; }
    }
    ASSERT_EQ(total, 16u, "T7: total edges==16");

    /* T8: fanout helper */
    fprintf(stderr, "\n-- T8: fanout() --\n");
    ASSERT_EQ(eh_hgn_dag_fanout(&dag, 0), 2u, "T8: fanout(0)==2");
    ASSERT_EQ(eh_hgn_dag_fanout(&dag, 7), 2u, "T8: fanout(7)==2");
    ASSERT_EQ(eh_hgn_dag_fanout(&dag, 99), 0u,"T8: fanout(OOB)==0");

    /* T9: OOB iterators */
    fprintf(stderr, "\n-- T9: OOB iterators --\n");
    ASSERT_EQ((uintptr_t)eh_hgn_dag_edges_begin(&dag, 8),  0u, "T9: begin OOB");
    ASSERT_EQ((uintptr_t)eh_hgn_dag_edges_end(&dag,   8),  0u, "T9: end OOB");

    /* T10: dump_info */
    fprintf(stderr, "\n-- T10: dump_info --\n");
    eh_hgn_dag_dump_info(&dag);
    ASSERT(1, "T10: dump_info no crash");
    eh_hgn_dag_dump_info(NULL);
    ASSERT(1, "T10: dump_info(NULL) no crash");

    /* Summary */
    fprintf(stderr, "\n=== %d/%d passed ===\n\n", g_pass, g_run);
    arena_destroy();
    return (g_pass == g_run) ? 0 : 1;
}