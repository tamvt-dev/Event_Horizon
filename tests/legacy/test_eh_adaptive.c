/* ================================================================
 * test_eh_adaptive.c — Test Suite for Adaptive Mechanisms
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "hgn/legacy/eh_adaptive.h"
#include "hgn/eh_beam_search.h"

/* Test framework */
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  [FAIL] %s\n", msg); \
    } else { \
        fprintf(stderr, "  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, tol, msg) do { \
    g_tests_run++; \
    if (fabsf((a) - (b)) > (tol)) { \
        fprintf(stderr, "  [FAIL] %s (%.6f vs %.6f)\n", msg, (float)(a), (float)(b)); \
    } else { \
        fprintf(stderr, "  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } \
} while(0)

/* ----------------------------------------------------------------
 * Test 1: Cosine Similarity
 * ---------------------------------------------------------------- */
static void test_cosine_similarity(void)
{
    fprintf(stderr, "\n=== Test 1: Cosine Similarity ===\n");
    
    float a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float b[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float c[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float d[4] = {0.707f, 0.707f, 0.0f, 0.0f};
    
    /* Identical vectors → cos = 1.0 */
    float cos_ab = eh_adaptive_cosine_similarity(a, b, 4);
    TEST_ASSERT_FLOAT_EQ(cos_ab, 1.0f, 1e-6f, "cos(a, b) = 1.0");
    
    /* Orthogonal vectors → cos = 0.0 */
    float cos_ac = eh_adaptive_cosine_similarity(a, c, 4);
    TEST_ASSERT_FLOAT_EQ(cos_ac, 0.0f, 1e-6f, "cos(a, c) = 0.0");
    
    /* 45-degree angle → cos ≈ 0.707 */
    float cos_ad = eh_adaptive_cosine_similarity(a, d, 4);
    TEST_ASSERT_FLOAT_EQ(cos_ad, 0.707f, 0.01f, "cos(a, d) ≈ 0.707");
}

/* ----------------------------------------------------------------
 * Test 2: LCG Determinism
 * ---------------------------------------------------------------- */
static void test_lcg_determinism(void)
{
    fprintf(stderr, "\n=== Test 2: LCG Determinism ===\n");
    
    uint32_t seed = 12345u;
    
    /* Same seed → same sequence */
    uint32_t s1a = eh_adaptive_lcg_next(seed);
    uint32_t s1b = eh_adaptive_lcg_next(seed);
    TEST_ASSERT(s1a == s1b, "LCG: same seed → same output");
    
    /* Different seeds → different outputs */
    uint32_t s2 = eh_adaptive_lcg_next(seed + 1);
    TEST_ASSERT(s1a != s2, "LCG: different seeds → different outputs");
    
    /* Multiple steps */
    uint32_t state = seed;
    uint32_t seq1[5];
    for (int i = 0; i < 5; i++) {
        state = eh_adaptive_lcg_next(state);
        seq1[i] = state;
    }
    
    /* Replay → same sequence */
    state = seed;
    for (int i = 0; i < 5; i++) {
        state = eh_adaptive_lcg_next(state);
        TEST_ASSERT(state == seq1[i], "LCG: deterministic replay");
    }
}

/* ----------------------------------------------------------------
 * Test 3: Perturbation Generation
 * ---------------------------------------------------------------- */
static void test_perturbation_generation(void)
{
    fprintf(stderr, "\n=== Test 3: Perturbation Generation ===\n");
    
    float vec[128];
    uint32_t seed = 42u;
    float scale = 0.3f;
    
    eh_adaptive_generate_perturbation(seed, scale, vec, 128);
    
    /* Check range [-scale, +scale] */
    bool in_range = true;
    for (int i = 0; i < 128; i++) {
        if (vec[i] < -scale - 1e-6f || vec[i] > scale + 1e-6f) {
            in_range = false;
            break;
        }
    }
    TEST_ASSERT(in_range, "Perturbation in range [-0.3, +0.3]");
    
    /* Check determinism */
    float vec2[128];
    eh_adaptive_generate_perturbation(seed, scale, vec2, 128);
    
    bool same = true;
    for (int i = 0; i < 128; i++) {
        if (fabsf(vec[i] - vec2[i]) > 1e-6f) {
            same = false;
            break;
        }
    }
    TEST_ASSERT(same, "Perturbation deterministic");
}

/* ----------------------------------------------------------------
 * Test 4: Collapse Gating
 * ---------------------------------------------------------------- */
static void test_collapse_gating(void)
{
    fprintf(stderr, "\n=== Test 4: Collapse Gating ===\n");
    
    EH_AdaptiveContext ctx;
    eh_adaptive_init(&ctx, 0.85f, 2.5f, 0.3f);
    
    float h0[EH_HGN_EMBED_DIM];
    float h1[EH_HGN_EMBED_DIM];
    float h2[EH_HGN_EMBED_DIM];
    
    memset(h0, 0, sizeof(h0));
    memset(h1, 0, sizeof(h1));
    memset(h2, 0, sizeof(h2));
    
    /* h0: normalized vector  */
    float norm0 = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        h0[i] = (i < 20) ? 1.0f : 0.0f;
        norm0 += h0[i] * h0[i];
    }
    norm0 = sqrtf(norm0);
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) h0[i] /= norm0;
    
    /* h1: very similar to h0 (90% overlap) → should collapse */
    float norm1 = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        h1[i] = (i < 18) ? 1.0f : 0.0f;
        norm1 += h1[i] * h1[i];
    }
    norm1 = sqrtf(norm1);
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) h1[i] /= norm1;
    
    /* h2: completely different pattern → should expand */
    float norm2 = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        h2[i] = (i >= 50 && i < 70) ? 1.0f : 0.0f;
        norm2 += h2[i] * h2[i];
    }
    norm2 = sqrtf(norm2);
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) h2[i] /= norm2;
    
    /* Step 0: always EXPAND */
    EH_CollapseState s0 = eh_adaptive_gate(&ctx, h0);
    TEST_ASSERT(s0 == EH_STATE_EXPAND, "Step 0: EXPAND");
    
    /* Step 1: high similarity → COLLAPSE */
    EH_CollapseState s1 = eh_adaptive_gate(&ctx, h1);
    float cos_h0_h1 = eh_adaptive_cosine_similarity(h0, h1, EH_HGN_EMBED_DIM);
    fprintf(stderr, "  [DEBUG] cos(h0, h1) = %.3f\n", cos_h0_h1);
    TEST_ASSERT(s1 == EH_STATE_COLLAPSE, "Step 1: COLLAPSE (high similarity)");
    
    /* Step 2: low similarity → EXPAND */
    EH_CollapseState s2 = eh_adaptive_gate(&ctx, h2);
    float cos_h1_h2 = eh_adaptive_cosine_similarity(h1, h2, EH_HGN_EMBED_DIM);
    fprintf(stderr, "  [DEBUG] cos(h1, h2) = %.3f (threshold=%.2f)\n", cos_h1_h2, ctx.collapse_threshold);
    TEST_ASSERT(s2 == EH_STATE_EXPAND, "Step 2: EXPAND (low similarity)");
    
    /* Check stats */
    TEST_ASSERT(ctx.collapse_count == 1, "Collapse count = 1");
    TEST_ASSERT(ctx.expand_count == 2, "Expand count = 2");
}

/* ----------------------------------------------------------------
 * Test 5: Beam Entropy
 * ---------------------------------------------------------------- */
static void test_beam_entropy(void)
{
    fprintf(stderr, "\n=== Test 5: Beam Entropy ===\n");
    
    EH_HGN_BeamTracker tracker;
    memset(&tracker, 0, sizeof(tracker));
    
    /* Uniform distribution (4 beams with same score) → max entropy */
    tracker.active_paths = 4;
    for (int i = 0; i < 4; i++) {
        tracker.paths[i].score = 0.0f;
    }
    
    float h1 = eh_adaptive_beam_entropy(&tracker);
    fprintf(stderr, "  [INFO] Uniform entropy: %.3f\n", h1);
    TEST_ASSERT(h1 > 1.0f, "Uniform → high entropy");
    
    /* One dominant beam → low entropy */
    tracker.paths[0].score = 10.0f;
    tracker.paths[1].score = 0.0f;
    tracker.paths[2].score = 0.0f;
    tracker.paths[3].score = 0.0f;
    
    float h2 = eh_adaptive_beam_entropy(&tracker);
    fprintf(stderr, "  [INFO] Dominant entropy: %.3f\n", h2);
    TEST_ASSERT(h2 < 0.5f, "Dominant → low entropy");
    TEST_ASSERT(h2 < h1, "Dominant < Uniform");
}

/* ----------------------------------------------------------------
 * Test 6: Mutant Spawning
 * ---------------------------------------------------------------- */
static void test_mutant_spawning(void)
{
    fprintf(stderr, "\n=== Test 6: Mutant Spawning ===\n");
    
    EH_AdaptiveContext ctx;
    eh_adaptive_init(&ctx, 0.85f, 1.0f, 0.3f);  /* Low entropy threshold */
    
    /* Setup tracker with high entropy */
    EH_HGN_BeamTracker tracker;
    memset(&tracker, 0, sizeof(tracker));
    tracker.active_paths = 4;
    for (int i = 0; i < 4; i++) {
        tracker.paths[i].score = (float)i * 0.1f;  /* Spread scores */
    }
    
    float h_current[EH_HGN_EMBED_DIM];
    memset(h_current, 0, sizeof(h_current));
    h_current[0] = 1.0f;
    
    /* Try spawn mutant */
    uint32_t mutant_id = eh_adaptive_try_spawn_mutant(&ctx, &tracker, h_current);
    
    TEST_ASSERT(mutant_id != UINT32_MAX, "Mutant spawned (high entropy)");
    TEST_ASSERT(ctx.mutant_count == 1, "Mutant count = 1");
    TEST_ASSERT(ctx.total_mutants_spawned == 1, "Total spawned = 1");
    
    /* Retrieve mutant vector */
    const float *mutant_vec = eh_adaptive_get_mutant_vec(&ctx, mutant_id);
    TEST_ASSERT(mutant_vec != NULL, "Mutant vector retrieved");
    
    /* Check perturbation applied */
    float diff = fabsf(mutant_vec[0] - h_current[0]);
    TEST_ASSERT(diff > 0.0f, "Mutant != original (perturbation applied)");
    fprintf(stderr, "  [INFO] Perturbation magnitude: %.3f\n", diff);
    
    /* Reset mutants */
    eh_adaptive_reset_mutants(&ctx);
    TEST_ASSERT(ctx.mutant_count == 0, "Mutants reset");
    
    const float *after_reset = eh_adaptive_get_mutant_vec(&ctx, mutant_id);
    TEST_ASSERT(after_reset == NULL, "Mutant invalidated after reset");
}

/* ----------------------------------------------------------------
 * Test 7: Mutant Pool Limit
 * ---------------------------------------------------------------- */
static void test_mutant_pool_limit(void)
{
    fprintf(stderr, "\n=== Test 7: Mutant Pool Limit ===\n");
    
    EH_AdaptiveContext ctx;
    eh_adaptive_init(&ctx, 0.85f, 0.1f, 0.3f);  /* Very low threshold */
    
    EH_HGN_BeamTracker tracker;
    memset(&tracker, 0, sizeof(tracker));
    tracker.active_paths = 4;
    for (int i = 0; i < 4; i++) {
        tracker.paths[i].score = (float)i;
    }
    
    float h[EH_HGN_EMBED_DIM];
    memset(h, 0, sizeof(h));
    h[0] = 1.0f;
    
    /* Spawn until pool is full */
    uint32_t spawned = 0;
    for (int i = 0; i < EH_MAX_MUTANTS + 5; i++) {
        ctx.step_counter = i;  /* Change seed */
        uint32_t id = eh_adaptive_try_spawn_mutant(&ctx, &tracker, h);
        if (id != UINT32_MAX) spawned++;
    }
    
    TEST_ASSERT(spawned == EH_MAX_MUTANTS, "Spawned exactly MAX_MUTANTS");
    TEST_ASSERT(ctx.mutant_count == EH_MAX_MUTANTS, "Pool at capacity");
    fprintf(stderr, "  [INFO] Pool saturated at %u mutants\n", EH_MAX_MUTANTS);
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int main(void)
{
    fprintf(stderr, "\n╔══════════════════════════════════════╗\n");
    fprintf(stderr, "║  EH_Adaptive Mechanisms Test Suite  ║\n");
    fprintf(stderr, "╚══════════════════════════════════════╝\n");
    
    test_cosine_similarity();
    test_lcg_determinism();
    test_perturbation_generation();
    test_collapse_gating();
    test_beam_entropy();
    test_mutant_spawning();
    test_mutant_pool_limit();
    
    fprintf(stderr, "\n╔══════════════════════════════════════╗\n");
    fprintf(stderr, "║  Result: %2d/%2d tests passed (%.0f%%)  ║\n",
            g_tests_passed, g_tests_run,
            g_tests_run ? 100.0f * g_tests_passed / g_tests_run : 0.0f);
    fprintf(stderr, "╚══════════════════════════════════════╝\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
