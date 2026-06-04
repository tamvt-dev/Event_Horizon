/* ================================================================
 * tests/test_eh_hgn_builder.c — Unit test Builder API
 *
 * Tests:
 *   - Create/destroy builder
 *   - Set embeddings
 *   - Add edges (with validations)
 *   - Finalize → BaseDag conversion
 *   - Query functions
 *   - Error handling
 *   - Round-trip: Build → Finalize → Save → Load
 *
 * Compile from EventHorizon/ root:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       tests/test_eh_hgn_builder.c \
 *       src/hgn/eh_hgn_builder.c src/hgn/eh_hgn_io.c \
 *       src/hgn/eh_hgn_dag.c src/core/eh_arena.c \
 *       -o tests/test_eh_hgn_builder -lm && tests/test_eh_hgn_builder
 * ================================================================ */

#define _POSIX_C_SOURCE 200112L

#include "hgn/eh_hgn_builder.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
 * Test files
 * ---------------------------------------------------------------- */
static const char *FILE_BUILDER_TEST = "/tmp/test_builder.ehdag";

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    fprintf(stderr, "\n=== EH_HGN_Builder Tests ===\n\n");

    /* ---- T1: Create/destroy builder ---- */
    fprintf(stderr, "\n-- T1: Create/destroy --\n");
    EH_HGN_Builder *b = eh_hgn_builder_create(10, 4);
    ASSERT_NEQ((uintptr_t)b, 0u, "T1: create builder");
    ASSERT_EQ(eh_hgn_builder_edge_count(b), 0u, "T1: initial edge count");
    eh_hgn_builder_destroy(b);
    ASSERT(1, "T1: destroy no crash");

    /* ---- T2: Invalid creation ---- */
    fprintf(stderr, "\n-- T2: Invalid creation --\n");
    ASSERT_EQ((uintptr_t)eh_hgn_builder_create(0, 4), 0u, "T2: zero vocab");
    ASSERT_EQ((uintptr_t)eh_hgn_builder_create(10, 0), 0u, "T2: zero fanout");
    ASSERT_EQ((uintptr_t)eh_hgn_builder_create(100000, 4), 0u, "T2: vocab too large");

    /* ---- T3: Set embeddings ---- */
    fprintf(stderr, "\n-- T3: Set embeddings --\n");
    b = eh_hgn_builder_create(5, 4);
    ASSERT_NEQ((uintptr_t)b, 0u, "T3: create builder");

    float emb0[EH_HGN_EMBED_DIM];
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) emb0[i] = (float)i;

    EH_HGN_BuilderStatus rc = eh_hgn_builder_set_embedding(b, 0, emb0);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T3: set embedding node 0");
    ASSERT(eh_hgn_builder_has_embedding(b, 0), "T3: has embedding node 0");
    ASSERT(!eh_hgn_builder_has_embedding(b, 1), "T3: no embedding node 1");

    /* T3: OOB embedding */
    rc = eh_hgn_builder_set_embedding(b, 99, emb0);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_OOB, "T3: OOB embedding");

    /* T3: NULL checks */
    rc = eh_hgn_builder_set_embedding(NULL, 0, emb0);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_NULL, "T3: NULL builder");

    rc = eh_hgn_builder_set_embedding(b, 0, NULL);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_NULL, "T3: NULL embedding");

    /* ---- T4: Add edges ---- */
    fprintf(stderr, "\n-- T4: Add edges --\n");
    float w01[EH_HGN_EMBED_DIM];
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) w01[i] = 100.0f + (float)i;

    rc = eh_hgn_builder_add_edge(b, 0, 1, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T4: add edge 0→1");
    ASSERT_EQ(eh_hgn_builder_edge_count(b), 1u, "T4: edge count 1");
    ASSERT_EQ(eh_hgn_builder_node_fanout(b, 0), 1u, "T4: node 0 fanout 1");

    /* T4: Add second edge from same node */
    float w02[EH_HGN_EMBED_DIM];
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) w02[i] = 200.0f + (float)i;

    rc = eh_hgn_builder_add_edge(b, 0, 2, 0.7f, w02);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T4: add edge 0→2");
    ASSERT_EQ(eh_hgn_builder_edge_count(b), 2u, "T4: edge count 2");
    ASSERT_EQ(eh_hgn_builder_node_fanout(b, 0), 2u, "T4: node 0 fanout 2");

    /* T4: Add edge from different node */
    rc = eh_hgn_builder_add_edge(b, 1, 3, 0.9f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T4: add edge 1→3");
    ASSERT_EQ(eh_hgn_builder_edge_count(b), 3u, "T4: edge count 3");
    ASSERT_EQ(eh_hgn_builder_node_fanout(b, 1), 1u, "T4: node 1 fanout 1");

    /* ---- T5: Edge validation errors ---- */
    fprintf(stderr, "\n-- T5: Edge validation --\n");
    
    /* T5: Duplicate edge */
    rc = eh_hgn_builder_add_edge(b, 0, 1, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_DUP, "T5: duplicate edge 0→1");

    /* T5: OOB src */
    rc = eh_hgn_builder_add_edge(b, 99, 1, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_OOB, "T5: OOB src");

    /* T5: OOB dst */
    rc = eh_hgn_builder_add_edge(b, 0, 99, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_OOB, "T5: OOB dst");

    /* T5: Self-loop allowed */
    rc = eh_hgn_builder_add_edge(b, 2, 2, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T5: self-loop allowed");

    /* T5: Exceed fanout */
    rc = eh_hgn_builder_add_edge(b, 0, 3, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T5: edge 0→3");
    rc = eh_hgn_builder_add_edge(b, 0, 4, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T5: edge 0→4 (fanout=4)");
    rc = eh_hgn_builder_add_edge(b, 0, 0, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_FANOUT, "T5: exceed fanout");

    /* T5: NULL checks */
    rc = eh_hgn_builder_add_edge(NULL, 0, 1, 0.5f, w01);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_NULL, "T5: NULL builder");

    rc = eh_hgn_builder_add_edge(b, 1, 2, 0.5f, NULL);
    ASSERT_EQ(rc, EH_HGN_BUILDER_ERR_NULL, "T5: NULL weight");

    /* ---- T6: Finalize to BaseDag ---- */
    fprintf(stderr, "\n-- T6: Finalize --\n");
    EH_Arena *arena = eh_arena_create(8 * 1024 * 1024);
    ASSERT_NEQ((uintptr_t)arena, 0u, "T6: arena created");

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    rc = eh_hgn_builder_finalize(b, arena, &dag);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T6: finalize OK");

    /* T6: Verify dag structure */
    ASSERT_EQ(dag.vocab_size, 5u, "T6: vocab_size");
    ASSERT_EQ(dag.total_edges, 6u, "T6: total_edges (added 6 edges)");
    ASSERT_EQ(dag.embed_dim, 128u, "T6: embed_dim");
    ASSERT_EQ(dag.max_fanout, 4u, "T6: max_fanout");
    ASSERT_NEQ((uintptr_t)dag.node_embed, 0u, "T6: node_embed allocated");
    ASSERT_NEQ((uintptr_t)dag.node_adj, 0u, "T6: node_adj allocated");
    ASSERT_NEQ((uintptr_t)dag.edge_compact, 0u, "T6: edge_compact allocated");
    ASSERT_NEQ((uintptr_t)dag.weight_pool, 0u, "T6: weight_pool allocated");

    /* T6: Verify embeddings */
    ASSERT_FEQ(dag.node_embed[0].vec[0], 0.0f, "T6: embed[0][0]");
    ASSERT_FEQ(dag.node_embed[0].vec[127], 127.0f, "T6: embed[0][127]");

    /* T6: Verify CSR structure */
    ASSERT_EQ(dag.node_adj[0].edge_count, 4u, "T6: node 0 has 4 edges");
    ASSERT_EQ(dag.node_adj[1].edge_count, 1u, "T6: node 1 has 1 edge");
    ASSERT_EQ(dag.node_adj[2].edge_count, 1u, "T6: node 2 has 1 edge");
    ASSERT_EQ(dag.node_adj[3].edge_count, 0u, "T6: node 3 has 0 edges");

    /* T6: Verify edge data */
    const EH_HGN_EdgeCompact *e0 = eh_hgn_dag_edges_begin(&dag, 0);
    ASSERT_EQ(e0[0].dst, 1u, "T6: edge[0].dst == 1");
    ASSERT_FEQ(e0[0].prior, 0.5f, "T6: edge[0].prior");

    const float *w = eh_hgn_dag_edge_weight(&dag, &e0[0]);
    ASSERT_FEQ(w[0], 100.0f, "T6: weight[0][0]");
    ASSERT_FEQ(w[127], 227.0f, "T6: weight[0][127]");

    /* ---- T7: Save and load round-trip ---- */
    fprintf(stderr, "\n-- T7: Round-trip (Build → Save → Load) --\n");
    EH_HGN_IO_Status io_rc = eh_hgn_io_save(&dag, FILE_BUILDER_TEST);
    ASSERT_EQ(io_rc, EH_HGN_IO_OK, "T7: save OK");

    EH_Arena *arena2 = eh_arena_create(8 * 1024 * 1024);
    EH_HGN_BaseDag dag2;
    memset(&dag2, 0, sizeof(dag2));
    io_rc = eh_hgn_io_load(arena2, FILE_BUILDER_TEST, &dag2);
    ASSERT_EQ(io_rc, EH_HGN_IO_OK, "T7: load OK");

    /* T7: Verify loaded data matches */
    ASSERT_EQ(dag2.vocab_size, dag.vocab_size, "T7: vocab_size match");
    ASSERT_EQ(dag2.total_edges, dag.total_edges, "T7: total_edges match");

    ASSERT_FEQ(dag2.node_embed[0].vec[0], 0.0f, "T7: embed preserved");
    ASSERT_EQ(dag2.node_adj[0].edge_count, 4u, "T7: fanout preserved");

    const EH_HGN_EdgeCompact *e2 = eh_hgn_dag_edges_begin(&dag2, 0);
    ASSERT_EQ(e2[0].dst, 1u, "T7: edge dst preserved");
    ASSERT_FEQ(e2[0].prior, 0.5f, "T7: edge prior preserved");

    const float *w2 = eh_hgn_dag_edge_weight(&dag2, &e2[0]);
    ASSERT_FEQ(w2[0], 100.0f, "T7: weight preserved");

    /* ---- T8: Query functions ---- */
    fprintf(stderr, "\n-- T8: Query functions --\n");
    ASSERT_EQ(eh_hgn_builder_edge_count(NULL), 0u, "T8: NULL edge_count");
    ASSERT_EQ(eh_hgn_builder_node_fanout(NULL, 0), 0u, "T8: NULL node_fanout");
    ASSERT(!eh_hgn_builder_has_embedding(NULL, 0), "T8: NULL has_embedding");
    ASSERT_EQ(eh_hgn_builder_node_fanout(b, 99), 0u, "T8: OOB fanout");
    ASSERT(!eh_hgn_builder_has_embedding(b, 99), "T8: OOB has_embedding");

    /* ---- T9: Error strings ---- */
    fprintf(stderr, "\n-- T9: Error strings --\n");
    const char *msg;
    msg = eh_hgn_builder_strerror(EH_HGN_BUILDER_OK);
    ASSERT_NEQ((uintptr_t)strstr(msg, "Success"), 0u, "T9: OK message");

    msg = eh_hgn_builder_strerror(EH_HGN_BUILDER_ERR_OOM);
    ASSERT_NEQ((uintptr_t)strstr(msg, "memory"), 0u, "T9: OOM message");

    msg = eh_hgn_builder_strerror(EH_HGN_BUILDER_ERR_FANOUT);
    ASSERT_NEQ((uintptr_t)strstr(msg, "fanout"), 0u, "T9: fanout message");

    /* ---- T10: Empty graph ---- */
    fprintf(stderr, "\n-- T10: Empty graph --\n");
    EH_HGN_Builder *b_empty = eh_hgn_builder_create(3, 4);
    ASSERT_NEQ((uintptr_t)b_empty, 0u, "T10: create empty builder");

    EH_Arena *arena3 = eh_arena_create(1 * 1024 * 1024);
    EH_HGN_BaseDag dag_empty;
    rc = eh_hgn_builder_finalize(b_empty, arena3, &dag_empty);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T10: finalize empty OK");
    ASSERT_EQ(dag_empty.total_edges, 0u, "T10: zero edges");
    ASSERT_EQ(dag_empty.vocab_size, 3u, "T10: vocab preserved");

    eh_hgn_builder_destroy(b_empty);
    eh_arena_destroy(arena3);

    /* ---- T11: Large graph ---- */
    fprintf(stderr, "\n-- T11: Large graph --\n");
    EH_HGN_Builder *b_large = eh_hgn_builder_create(100, 16);
    ASSERT_NEQ((uintptr_t)b_large, 0u, "T11: create large builder");

    /* Add 100 edges */
    float w_test[EH_HGN_EMBED_DIM];
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) w_test[i] = 1.0f;

    uint32_t added = 0;
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t dst = (i + 1) % 100;
        rc = eh_hgn_builder_add_edge(b_large, i, dst, 0.5f, w_test);
        if (rc == EH_HGN_BUILDER_OK) added++;
    }
    ASSERT_EQ(added, 100u, "T11: added 100 edges");
    ASSERT_EQ(eh_hgn_builder_edge_count(b_large), 100u, "T11: edge count 100");

    EH_Arena *arena4 = eh_arena_create(16 * 1024 * 1024);
    EH_HGN_BaseDag dag_large;
    rc = eh_hgn_builder_finalize(b_large, arena4, &dag_large);
    ASSERT_EQ(rc, EH_HGN_BUILDER_OK, "T11: finalize large OK");
    ASSERT_EQ(dag_large.total_edges, 100u, "T11: large edges");

    eh_hgn_builder_destroy(b_large);
    eh_arena_destroy(arena4);

    /* Cleanup */
    eh_hgn_builder_destroy(b);
    eh_arena_destroy(arena);
    eh_arena_destroy(arena2);

    /* Summary */
    fprintf(stderr, "\n=== %d/%d passed ===\n\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
