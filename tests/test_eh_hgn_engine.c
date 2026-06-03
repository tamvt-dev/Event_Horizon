/* ================================================================
 * tests/test_eh_hgn_engine.c — Unit test Layer 4 (Unified Engine)
 *
 * Test toàn bộ pipeline: DAG → Collapse → Beam → Results
 *
 * Compile từ tests/:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I../include \
 *       test_eh_hgn_engine.c \
 *       ../src/hgn/eh_hgn_engine.c \
 *       ../src/hgn/eh_hgn_collapse.c \
 *       ../src/hgn/eh_hgn_dag.c \
 *       ../src/hgn/eh_beam_search.c \
 *       ../src/core/eh_arena.c \
 *       -o test_eh_hgn_engine -lm && ./test_eh_hgn_engine
 * ================================================================ */

#include "hgn/eh_hgn_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
#define ASSERT_GT(a,b,msg)  ASSERT((a)>(b), msg)
#define ASSERT_LE(a,b,msg)  ASSERT((a)<=(b), msg)

/* ----------------------------------------------------------------
 * Helper: tạo tiny test DAG (vocab=4, cấu trúc: 0→1, 0→2, 1→3, 2→3)
 * ---------------------------------------------------------------- */
static const char *DAG_BIN = "/tmp/test_engine_dag.bin";

static int build_test_dag(void)
{
    const uint32_t V = 4, E = 4;
    FILE *fp = fopen(DAG_BIN, "wb");
    if (!fp) return -1;

    EH_HGN_DagFileHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = EH_HGN_DAG_MAGIC;
    hdr.version = EH_HGN_DAG_VERSION;
    hdr.vocab_size = V;
    hdr.total_edges = E;
    hdr.embed_dim = EH_HGN_EMBED_DIM;
    hdr.max_fanout = 2;
    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* Node embeddings: vec[k] = i + k*0.01 */
    EH_HGN_NodeEmbed ne;
    for (uint32_t i = 0; i < V; i++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ne.vec[k] = (float)i + (float)k * 0.01f;
        fwrite(&ne, sizeof(ne), 1, fp);
    }

    /* Node adjacency (CSR) */
    EH_HGN_NodeAdj adj[4] = {
        {0, 2},  /* Token 0 → 2 edges (offset 0) */
        {2, 1},  /* Token 1 → 1 edge  (offset 2) */
        {3, 1},  /* Token 2 → 1 edge  (offset 3) */
        {4, 0}   /* Token 3 → sink node          */
    };
    fwrite(adj, sizeof(EH_HGN_NodeAdj), 4, fp);

    /* Edge compact data */
    EH_HGN_EdgeCompact ec;
    memset(&ec, 0, sizeof(ec));
    
    /* Edge 0: 0→1 (high prior) */
    ec.dst = 1; ec.prior = 0.8f; ec.weight_idx = 0;
    fwrite(&ec, sizeof(ec), 1, fp);
    
    /* Edge 1: 0→2 (low prior) */
    ec.dst = 2; ec.prior = 0.2f; ec.weight_idx = 1;
    fwrite(&ec, sizeof(ec), 1, fp);
    
    /* Edge 2: 1→3 */
    ec.dst = 3; ec.prior = 0.9f; ec.weight_idx = 2;
    fwrite(&ec, sizeof(ec), 1, fp);
    
    /* Edge 3: 2→3 */
    ec.dst = 3; ec.prior = 0.1f; ec.weight_idx = 3;
    fwrite(&ec, sizeof(ec), 1, fp);

    /* Padding to 32-byte alignment */
    long pos = ftell(fp);
    long apos = (pos + 31L) & ~31L;
    if (apos != pos) {
        uint8_t pad[32] = {0};
        fwrite(pad, (size_t)(apos - pos), 1, fp);
    }

    /* Edge weights: vec[k] = e + k*0.01 */
    EH_HGN_EdgeWeight ew;
    for (uint32_t e = 0; e < E; e++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ew.vec[k] = (float)e + (float)k * 0.01f;
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
    fprintf(stderr, "\n=== EH_HGN_Engine Integration Tests ===\n\n");

    /* --- T0: Setup --- */
    fprintf(stderr, "-- T0: Setup --\n");
    ASSERT(build_test_dag() == 0, "T0: build test DAG");

    EH_Arena *arena = eh_arena_create(16 * 1024 * 1024);  /* 16MB */
    ASSERT(arena != NULL, "T0: arena create");

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    ASSERT(eh_hgn_dag_load(arena, DAG_BIN, &dag) == EH_HGN_OK,
           "T0: DAG load");

    /* --- T1: Default config --- */
    fprintf(stderr, "\n-- T1: Default config --\n");
    EH_HGN_EngineConfig cfg = eh_hgn_default_config();
    ASSERT(cfg.collapse_thresh > 0.9f, "T1: default collapse_thresh=0.92");
    ASSERT(cfg.entropy_thresh > 1.7f,  "T1: default entropy_thresh=1.80");
    ASSERT(cfg.enable_collapse == true, "T1: collapse enabled by default");
    ASSERT(cfg.enable_mutants == true,  "T1: mutants enabled by default");

    /* --- T2: Session init với prompt --- */
    fprintf(stderr, "\n-- T2: Session init --\n");
    EH_HGN_InferenceSession session;
    uint32_t prompt[1] = {0};  /* Start từ token 0 */
    
    int rc = eh_hgn_session_init(&session, &dag, arena, prompt, 1, NULL);
    ASSERT(rc == 0, "T2: session init OK");
    ASSERT(session.dag == &dag, "T2: DAG reference set");
    ASSERT(session.step_count == 0, "T2: step_count=0 initially");
    ASSERT(session.is_finished == false, "T2: not finished initially");

    /* --- T3: Get beams sau init (chỉ có prompt) --- */
    fprintf(stderr, "\n-- T3: Initial beams --\n");
    const EH_HGN_BeamPath *beams = eh_hgn_session_get_beams(&session);
    ASSERT(beams != NULL, "T3: get_beams returns non-NULL");
    ASSERT(beams[0].seq_len == 1, "T3: first beam has prompt");
    ASSERT(beams[0].tokens[0] == 0, "T3: prompt token=0");

    /* --- T4: First inference step --- */
    fprintf(stderr, "\n-- T4: First step --\n");
    uint32_t active = eh_hgn_session_step(&session);
    ASSERT(active > 0, "T4: beams still active after step");
    ASSERT(session.step_count == 1, "T4: step_count=1");
    ASSERT(session.is_finished == false, "T4: not finished yet");

    /* Beam đã expand từ token 0 → [1, 2] */
    const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
    ASSERT(best != NULL, "T4: get_best returns non-NULL");
    ASSERT(best->seq_len == 2, "T4: sequence grew to 2 tokens");
    
    /* Token thứ 2 có thể là 1 hoặc 2 (tùy vào beam scoring) */
    uint32_t selected_token = best->tokens[1];
    fprintf(stderr, "  [INFO] Selected token: %u (expected 1 or 2)\n", selected_token);
    ASSERT(selected_token == 1u || selected_token == 2u, "T4: selected valid token");

    /* --- T5: Second step (convergence tới sink token 3) --- */
    fprintf(stderr, "\n-- T5: Second step --\n");
    active = eh_hgn_session_step(&session);
    ASSERT(session.step_count == 2, "T5: step_count=2");

    best = eh_hgn_session_get_best(&session);
    ASSERT(best->seq_len == 3, "T5: sequence grew to 3 tokens");
    ASSERT_EQ(best->tokens[2], 3u, "T5: converged to sink token 3");

    /* --- T6: Third step (sink node → all beams finished) --- */
    fprintf(stderr, "\n-- T6: Terminal step --\n");
    active = eh_hgn_session_step(&session);
    ASSERT(active == 0, "T6: no active beams (all finished)");
    ASSERT(session.is_finished == true, "T6: generation finished");
    ASSERT(eh_hgn_session_is_done(&session), "T6: is_done() returns true");

    /* --- T7: Session reset với prompt mới --- */
    fprintf(stderr, "\n-- T7: Session reset --\n");
    uint32_t new_prompt[2] = {0, 1};
    eh_hgn_session_reset(&session, new_prompt, 2);
    ASSERT(session.step_count == 0, "T7: step_count reset to 0");
    ASSERT(session.is_finished == false, "T7: not finished after reset");

    beams = eh_hgn_session_get_beams(&session);
    ASSERT(beams[0].seq_len == 2, "T7: new prompt has 2 tokens");
    ASSERT(beams[0].tokens[0] == 0, "T7: prompt[0]=0");
    ASSERT(beams[0].tokens[1] == 1, "T7: prompt[1]=1");

    /* --- T8: Custom config (disable collapse + mutants) --- */
    fprintf(stderr, "\n-- T8: Custom config --\n");
    EH_HGN_EngineConfig custom_cfg = eh_hgn_default_config();
    custom_cfg.enable_collapse = false;
    custom_cfg.enable_mutants = false;
    custom_cfg.max_steps = 5;

    EH_HGN_InferenceSession session2;
    rc = eh_hgn_session_init(&session2, &dag, arena, prompt, 1, &custom_cfg);
    ASSERT(rc == 0, "T8: custom config session init OK");
    ASSERT(session2.config.enable_collapse == false, "T8: collapse disabled");
    ASSERT(session2.config.enable_mutants == false, "T8: mutants disabled");
    ASSERT(session2.config.max_steps == 5, "T8: max_steps=5");

    /* --- T9: Max steps limit --- */
    fprintf(stderr, "\n-- T9: Max steps enforcement --\n");
    for (uint32_t i = 0; i < 10; i++) {
        active = eh_hgn_session_step(&session2);
        if (session2.is_finished) break;
    }
    ASSERT_LE(session2.step_count, 5u, "T9: step_count ≤ max_steps");

    /* --- T10: Stats dump no crash --- */
    fprintf(stderr, "\n-- T10: Stats dump --\n");
    eh_hgn_session_dump_stats(&session);
    ASSERT(1, "T10: dump_stats no crash");
    
    eh_hgn_session_dump_beams(&session);
    ASSERT(1, "T10: dump_beams no crash");

    /* --- T11: Null safety --- */
    fprintf(stderr, "\n-- T11: Null safety --\n");
    ASSERT(eh_hgn_session_get_beams(NULL) == NULL, "T11: get_beams(NULL)");
    ASSERT(eh_hgn_session_get_best(NULL) == NULL, "T11: get_best(NULL)");
    eh_hgn_session_dump_stats(NULL);  /* should not crash */
    ASSERT(1, "T11: dump_stats(NULL) no crash");

    /* --- Summary --- */
    fprintf(stderr, "\n=== %d/%d passed ===\n\n", g_pass, g_run);

    eh_arena_destroy(arena);
    return (g_pass == g_run) ? 0 : 1;
}
