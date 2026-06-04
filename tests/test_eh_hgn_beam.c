/* =========================================================================
 * MODULE: HGN (Heuristic Graph Network) - Layer 3 Test Suite
 * FILE: test_eh_hgn_beam.c
 *
 * Tests:
 *   T1 - Arena init
 *   T2 - DAG load
 *   T3 - Beam tracker init
 *   T4 - Step 1: correct token selection (cat vs dog)
 *   T5 - Sequence converged to terminal (EOS detection)
 *   T6 - [REGRESSION] Multi-beam: correct parent tracking (exposes old bug)
 *   T7 - eh_hgn_beam_step returns 0 when all beams finished
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core/eh_arena.h"
#include "hgn/eh_hgn_dag.h"
#include "hgn/eh_beam_search.h"

/* ----------------------------------------------------------------
 * Helper: Build simple test binary
 *
 * Graph (4 nodes, 4 edges):
 *   0 "The"  → 1 "cat" (prior=0.8, weight[0]=1.5)
 *              → 2 "dog" (prior=0.2, weight[0]=-0.5)
 *   1 "cat"  → 3 "ends" (prior=0.9, weight[0]=1.0)
 *   2 "dog"  → 3 "ends" (prior=0.1, weight[0]=0.0)
 *   3 "ends" → ∅ (sink node)
 *
 * Embeddings:
 *   0: vec[0]=1.0   (baseline)
 *   1: vec[0]=2.0   (positive context)
 *   2: vec[0]=-1.0  (negative context)
 *   3: vec[0]=0.5   (neutral)
 * ---------------------------------------------------------------- */
static void build_beam_test_binary(const char* path) {
    FILE *f = fopen(path, "wb");
    assert(f != NULL);

    EH_HGN_DagFileHeader hdr = {
        .magic       = EH_HGN_DAG_MAGIC,
        .version     = EH_HGN_DAG_VERSION,
        .vocab_size  = 4,
        .total_edges = 4,
        .embed_dim   = EH_HGN_EMBED_DIM,
        .max_fanout  = 2,
        .reserved    = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, f);

    /* Node embeddings */
    EH_HGN_NodeEmbed embeds[4];
    memset(embeds, 0, sizeof(embeds));
    embeds[0].vec[0] =  1.0f;
    embeds[1].vec[0] =  2.0f;
    embeds[2].vec[0] = -1.0f;
    embeds[3].vec[0] =  0.5f;
    fwrite(embeds, sizeof(EH_HGN_NodeEmbed), 4, f);

    /* CSR adjacency */
    EH_HGN_NodeAdj adj[4] = {
        {0, 2},  /* Token 0: 2 edges at offset 0 */
        {2, 1},  /* Token 1: 1 edge  at offset 2 */
        {3, 1},  /* Token 2: 1 edge  at offset 3 */
        {4, 0}   /* Token 3: sink (0 edges)       */
    };
    fwrite(adj, sizeof(EH_HGN_NodeAdj), 4, f);

    /* Edge compact */
    EH_HGN_EdgeCompact edges[4];
    memset(edges, 0, sizeof(edges));
    edges[0] = (EH_HGN_EdgeCompact){ .dst=1, .prior=0.8f, .weight_idx=0 };
    edges[1] = (EH_HGN_EdgeCompact){ .dst=2, .prior=0.2f, .weight_idx=1 };
    edges[2] = (EH_HGN_EdgeCompact){ .dst=3, .prior=0.9f, .weight_idx=2 };
    edges[3] = (EH_HGN_EdgeCompact){ .dst=3, .prior=0.1f, .weight_idx=3 };
    fwrite(edges, sizeof(EH_HGN_EdgeCompact), 4, f);

    /* Padding to 32-byte boundary */
    long pos  = ftell(f);
    long apos = (pos + 31L) & ~31L;
    if (apos != pos) {
        uint8_t pad[32] = {0};
        fwrite(pad, (size_t)(apos - pos), 1, f);
    }

    /* Edge weights */
    EH_HGN_EdgeWeight weights[4];
    memset(weights, 0, sizeof(weights));
    weights[0].vec[0] =  1.5f;   /* 0→1: positive boost */
    weights[1].vec[0] = -0.5f;   /* 0→2: negative       */
    weights[2].vec[0] =  1.0f;   /* 1→3 */
    weights[3].vec[0] =  0.0f;   /* 2→3 */
    fwrite(weights, sizeof(EH_HGN_EdgeWeight), 4, f);

    fclose(f);
}

/* ----------------------------------------------------------------
 * Helper: pretty-print a beam path
 * ---------------------------------------------------------------- */
static void print_beam(const EH_HGN_BeamPath *p, const char *label) {
    printf("  [INFO] %s: tokens=[", label);
    for (uint32_t i = 0; i < p->seq_len; i++) {
        printf("%u", p->tokens[i]);
        if (i + 1 < p->seq_len) printf(" ");
    }
    printf("] score=%.3f finished=%s\n",
           p->score, p->is_finished ? "yes" : "no");
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(void) {
    printf("=== EH_HGN_BeamSearch Logic Tests ===\n");

    const char* BIN_FILE = "/tmp/beam_test.bin";
    build_beam_test_binary(BIN_FILE);

    /* ── T1: Arena init ─────────────────────────────────────── */
    EH_Arena *arena = eh_arena_create(10 * 1024 * 1024);
    assert(arena != NULL);
    printf("  [PASS] T1: Arena initialized (10MB)\n");

    /* ── T2: DAG load ───────────────────────────────────────── */
    EH_HGN_BaseDag dag;
    EH_HGN_Status rc = eh_hgn_dag_load(arena, BIN_FILE, &dag);
    assert(rc == EH_HGN_OK);
    assert(dag.vocab_size  == 4);
    assert(dag.total_edges == 4);
    printf("  [PASS] T2: CSR Binary loaded (vocab=%u, edges=%u)\n",
           dag.vocab_size, dag.total_edges);

    /* ── T3: Beam tracker init ──────────────────────────────── */
    EH_HGN_BeamTracker tracker;
    uint32_t prompt[1] = {0};   /* "The" */
    eh_hgn_beam_init(&tracker, &dag, prompt, 1);

    assert(tracker.active_paths     == 1);
    assert(tracker.paths[0].tokens[0] == 0);
    assert(tracker.paths[0].seq_len   == 1);
    assert(tracker.paths[0].is_finished == false);
    printf("  [PASS] T3: Beam Tracker initialized with prompt\n");

    /* ── T4: Step 1 — prefer cat (1) over dog (2) ──────────── */
    /*
     * Scores from node 0 (embed[0]=1.0):
     *   edge 0→1: prior=0.8 + dot(w=[1.5,…], ctx=[1.0,…]) = 0.8 + 1.5 = 2.3
     *   edge 0→2: prior=0.2 + dot(w=[-0.5,…], ctx=[1.0,…]) = 0.2 + (-0.5) = -0.3
     * Expected top-1 token: 1 ("cat"), score: 2.3
     */
    uint32_t active = eh_hgn_beam_step(&tracker, &dag);

    assert(tracker.active_paths > 0);
    uint32_t step1_token = tracker.paths[0].tokens[1];
    float    step1_score = tracker.paths[0].score;

    printf("  [INFO] Step 1 Top-1 Token: %u (expected 1 'cat')\n", step1_token);
    printf("  [INFO] Step 1 Score: %.3f\n", step1_score);
    printf("  [INFO] Active beams after step 1: %u\n", active);

    assert(step1_token == 1 && "Beam must prefer 'cat' (higher prior + context)");
    /* Temperature scaling adjusts scores: original ~2.3 → 2.3/0.8 ≈ 2.875 */
    assert(step1_score > 2.5f && step1_score < 3.2f);
    printf("  [PASS] T4: Step 1 autoregressive expansion correct\n");

    /* ── T5: Step 2 — reach terminal node (EOS detection) ───── */
    /*
     * From "cat" (1, embed[0]=2.0):
     *   edge 1→3: prior=0.9 + dot(w=[1.0,…], ctx=[2.0,…]) = 0.9 + 2.0 = 2.9
     * Cumulative: 2.3 + 2.9 = 5.2
     * Node 3 is a sink → is_finished should be true
     */
    active = eh_hgn_beam_step(&tracker, &dag);

    const EH_HGN_BeamPath *best = eh_hgn_beam_get_best(&tracker);
    assert(best != NULL);

    print_beam(best, "Final best beam");
    printf("  [INFO] Active beams after step 2: %u\n", active);

    assert(best->seq_len  == 3);
    assert(best->tokens[0] == 0);
    assert(best->tokens[1] == 1);
    assert(best->tokens[2] == 3);
    assert(best->is_finished == true && "Sink node must set is_finished=true");
    printf("  [PASS] T5: Sequence 0→1→3 converged to terminal, is_finished=true\n");

    /* ── T6: REGRESSION — Multi-beam parent tracking ─────────
     *
     * Old bug: eh_hgn_beam_step always copied paths[0] as parent.
     * This test has EH_BEAM_WIDTH >= 2 so both "cat" (beam 0) and
     * "dog" (beam 1) beams exist after step 1.
     * After step 2, beam "dog→ends" must have token history [0,2,3],
     * NOT [0,1,3] (which is what the old code would produce by always
     * copying paths[0]).
     * ─────────────────────────────────────────────────────────── */
    printf("\n  --- T6: Multi-beam parent tracking regression test ---\n");

    EH_HGN_BeamTracker tracker2;
    eh_hgn_beam_init(&tracker2, &dag, prompt, 1);

    /* Step 1: both "cat"(1) and "dog"(2) should appear as separate beams */
    eh_hgn_beam_step(&tracker2, &dag);

    printf("  [INFO] Beams after step 1:\n");
    for (uint32_t b = 0; b < tracker2.active_paths; b++) {
        print_beam(&tracker2.paths[b], "beam");
    }

    /* Step 2: expand all beams */
    eh_hgn_beam_step(&tracker2, &dag);

    printf("  [INFO] Beams after step 2:\n");
    bool found_cat_path = false;
    bool found_dog_path = false;

    for (uint32_t b = 0; b < tracker2.active_paths; b++) {
        print_beam(&tracker2.paths[b], "beam");
        const EH_HGN_BeamPath *p = &tracker2.paths[b];
        if (p->seq_len == 3 && p->tokens[1] == 1 && p->tokens[2] == 3)
            found_cat_path = true;
        if (p->seq_len == 3 && p->tokens[1] == 2 && p->tokens[2] == 3)
            found_dog_path = true;
    }

    assert(found_cat_path && "Must find path 0->1->3 (cat sequence)");
    assert(found_dog_path && "Must find path 0->2->3 (dog sequence) — BUG if missing!");
    printf("  [PASS] T6: Both 0→1→3 and 0→2→3 paths found (parent tracking correct)\n");

    /* ── T7: Step on finished tracker returns 0 ─────────────── */
    uint32_t already_done = eh_hgn_beam_step(&tracker, &dag);
    assert(already_done == 0 && "Fully finished tracker must return 0 active");
    printf("  [PASS] T7: eh_hgn_beam_step returns 0 when all beams finished\n");

    /* ── Cleanup ─────────────────────────────────────────────── */
    eh_arena_destroy(arena);
    remove(BIN_FILE);

    printf("\n=== ALL LAYER 3 BEAM TESTS PASSED! ✓ ===\n");
    return 0;
}
