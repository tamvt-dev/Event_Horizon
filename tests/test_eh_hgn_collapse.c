/* ================================================================
 * tests/test_eh_hgn_collapse.c — Unit test Layer 2
 *
 * Compile từ EventHorizon/tests/:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I../include \
 *       test_eh_hgn_collapse.c \
 *       ../src/hgn/eh_hgn_collapse.c \
 *       ../src/hgn/eh_hgn_dag.c \
 *       ../src/core/eh_arena.c \
 *       -o test_eh_hgn_collapse -lm && ./test_eh_hgn_collapse
 * ================================================================ */

#include "hgn/eh_hgn_collapse.h"

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
#define ASSERT_FEQ(a,b,eps,msg) ASSERT(fabsf((a)-(b)) < (eps), msg)
#define ASSERT_GT(a,b,msg)  ASSERT((a)>(b), msg)
#define ASSERT_LT(a,b,msg)  ASSERT((a)<(b), msg)

/* ----------------------------------------------------------------
 * Helper: tạo vector normalized dim=128
 * ---------------------------------------------------------------- */
static void make_vec(float *v, float base, float step)
{
    float norm = 0.0f;
    for (uint32_t i = 0; i < EH_HGN_EMBED_DIM; i++) {
        v[i] = base + step * (float)i;
        norm += v[i] * v[i];
    }
    norm = sqrtf(norm);
    if (norm > 1e-8f)
        for (uint32_t i = 0; i < EH_HGN_EMBED_DIM; i++) v[i] /= norm;
}

/* ----------------------------------------------------------------
 * Helper: tạo dummy DAG nhỏ trong arena (vocab=4, fanout=2)
 * ---------------------------------------------------------------- */
static const char *DAG_BIN = "/tmp/test_collapse_dag.bin";

static int build_dag_bin(void)
{
    const uint32_t V = 4, E = 8, K = 2;
    FILE *fp = fopen(DAG_BIN, "wb");
    if (!fp) return -1;

    EH_HGN_DagFileHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = EH_HGN_DAG_MAGIC; hdr.version = EH_HGN_DAG_VERSION;
    hdr.vocab_size = V; hdr.total_edges = E;
    hdr.embed_dim = EH_HGN_EMBED_DIM; hdr.max_fanout = K;
    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* node_embed: vec[k] = i + k*0.01 */
    EH_HGN_NodeEmbed ne;
    for (uint32_t i = 0; i < V; i++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ne.vec[k] = (float)i + (float)k * 0.01f;
        fwrite(&ne, sizeof(ne), 1, fp);
    }

    /* node_adj */
    EH_HGN_NodeAdj adj;
    for (uint32_t i = 0; i < V; i++) {
        adj.edge_offset = i * K; adj.edge_count = K;
        fwrite(&adj, sizeof(adj), 1, fp);
    }

    /* edge_compact */
    EH_HGN_EdgeCompact ec;
    for (uint32_t i = 0; i < V; i++) {
        ec.dst=(i+1)%V; ec.prior=0.8f; ec.weight_idx=i*K;
        fwrite(&ec, sizeof(ec), 1, fp);
        ec.dst=(i+2)%V; ec.prior=0.2f; ec.weight_idx=i*K+1;
        fwrite(&ec, sizeof(ec), 1, fp);
    }

    /* Padding */
    long pos = ftell(fp), apos = (pos+31L)&~31L;
    if (apos != pos) { uint8_t p[32]={0}; fwrite(p,(size_t)(apos-pos),1,fp); }

    /* weight_pool: vec[k] = e + k*0.01 */
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
    fprintf(stderr, "\n=== EH_HGN_CollapseGating Tests ===\n\n");

    ASSERT(build_dag_bin() == 0, "T0: build dummy DAG bin");

    EH_Arena *arena = eh_arena_create(8 * 1024 * 1024); /* 8MB */
    ASSERT(arena != NULL, "T0: arena create");

    /* Load DAG */
    EH_HGN_BaseDag dag; memset(&dag, 0, sizeof(dag));
    ASSERT(eh_hgn_dag_load(arena, DAG_BIN, &dag) == EH_HGN_OK,
           "T0: DAG load");

    /* ---- T1: ctx init ---- */
    fprintf(stderr, "\n-- T1: ctx init --\n");
    EH_HGN_CollapseCtx ctx __attribute__((aligned(32)));
    ASSERT(eh_hgn_collapse_ctx_init(&ctx, arena) == 0, "T1: ctx init OK");
    ASSERT(ctx.mutant_pool  != NULL,  "T1: mutant_pool allocated");
    ASSERT(ctx.h_prev_valid == false, "T1: h_prev_valid=false initially");
    ASSERT_FEQ(ctx.collapse_thresh, EH_HGN_COLLAPSE_THRESH, 1e-6f,
               "T1: collapse_thresh default");

    /* ---- T2: step đầu luôn EXPAND (chưa có h_prev) ---- */
    fprintf(stderr, "\n-- T2: First step always EXPAND --\n");
    float h0[EH_HGN_EMBED_DIM] __attribute__((aligned(32)));
    make_vec(h0, 1.0f, 0.01f);
    EH_HGN_CollapseDecision d = eh_hgn_collapse_gate(&ctx, h0);
    ASSERT_EQ(d, EH_HGN_EXPAND, "T2: first step = EXPAND");
    ASSERT(ctx.h_prev_valid == true,  "T2: h_prev_valid=true after step");
    ASSERT_EQ(ctx.expand_count, 1u,   "T2: expand_count==1");
    ASSERT_EQ(ctx.step_count,   1u,   "T2: step_count==1");

    /* ---- T3: vector giống hệt → COLLAPSE ---- */
    fprintf(stderr, "\n-- T3: Identical vector → COLLAPSE --\n");
    /* h_prev = h0, dùng lại h0 → cosine = 1.0 > 0.92              */
    d = eh_hgn_collapse_gate(&ctx, h0);
    ASSERT_EQ(d, EH_HGN_COLLAPSE, "T3: same vector = COLLAPSE");
    ASSERT_EQ(ctx.collapse_count, 1u, "T3: collapse_count==1");

    /* ---- T4: vector ngược chiều → EXPAND ---- */
    fprintf(stderr, "\n-- T4: Orthogonal vector → EXPAND --\n");
    float h_ortho[EH_HGN_EMBED_DIM] __attribute__((aligned(32)));
    /* Vector trực giao với h0: đảo dấu một nửa chiều              */
    memcpy(h_ortho, h0, sizeof(h0));
    for (uint32_t i = 0; i < EH_HGN_EMBED_DIM / 2; i++) h_ortho[i] *= -1.0f;
    /* Re-normalize */
    float n = 0.0f;
    for (uint32_t i = 0; i < EH_HGN_EMBED_DIM; i++) n += h_ortho[i]*h_ortho[i];
    n = sqrtf(n);
    for (uint32_t i = 0; i < EH_HGN_EMBED_DIM; i++) h_ortho[i] /= n;

    d = eh_hgn_collapse_gate(&ctx, h_ortho);
    ASSERT_EQ(d, EH_HGN_EXPAND, "T4: orthogonal vector = EXPAND");

    /* ---- T5: mean_pool output ---- */
    fprintf(stderr, "\n-- T5: mean_pool --\n");
    float pooled[EH_HGN_EMBED_DIM];
    eh_hgn_collapse_mean_pool(&dag, 0, pooled);

    /* Tự tính expected: mean của weight_pool[0] và weight_pool[1]  */
    float expected[EH_HGN_EMBED_DIM];
    const float *w0 = eh_hgn_dag_edge_weight(&dag, eh_hgn_dag_edges_begin(&dag, 0));
    const float *w1 = eh_hgn_dag_edge_weight(&dag, eh_hgn_dag_edges_begin(&dag, 0) + 1);
    for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
        expected[k] = (w0[k] + w1[k]) * 0.5f;

    float max_diff = 0.0f;
    for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++) {
        float d2 = fabsf(pooled[k] - expected[k]);
        if (d2 > max_diff) max_diff = d2;
    }
    ASSERT(max_diff < 1e-5f, "T5: mean_pool matches manual calculation");

    /* ---- T6: beam_entropy ---- */
    fprintf(stderr, "\n-- T6: beam_entropy --\n");
    /* Uniform distribution → entropy = log(N) */
    float uniform[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float H_uniform = eh_hgn_beam_entropy(uniform, 4);
    ASSERT_FEQ(H_uniform, logf(4.0f), 1e-4f, "T6: uniform entropy=log(4)");

    /* Deterministic (score cực cao vs cực thấp) → entropy ≈ 0      */
    float deterministic[2] = {100.0f, -100.0f};
    float H_det = eh_hgn_beam_entropy(deterministic, 2);
    ASSERT_LT(H_det, 0.01f, "T6: near-deterministic entropy ≈ 0");

    /* High entropy signal (uniform) > entropy_thresh mặc định?
     * log(4) ≈ 1.386, entropy_thresh = 1.80 → uniform 4 beams chưa trigger
     * Dùng uniform 8 beams: log(8) ≈ 2.079 > 1.80                 */
    float uniform8[8] = {1,1,1,1,1,1,1,1};
    float H8 = eh_hgn_beam_entropy(uniform8, 8);
    ASSERT_GT(H8, ctx.entropy_thresh, "T6: 8-beam uniform > entropy_thresh");

    /* ---- T7: mutant_spawn ---- */
    fprintf(stderr, "\n-- T7: mutant_spawn --\n");
    /* Reset ctx để có h_prev hợp lệ                                */
    eh_hgn_collapse_ctx_reset(&ctx);
    eh_hgn_collapse_gate(&ctx, h0);   /* step 1: h_prev = h0       */

    EH_HGN_MutantNode *m = eh_hgn_mutant_spawn(&ctx, 0, 1, 2.5f);
    ASSERT(m != NULL,          "T7: mutant spawned");
    ASSERT(m->active == true,  "T7: mutant active");
    ASSERT_EQ(m->src_token, 0u,"T7: src_token==0");
    ASSERT_EQ(m->dst_token, 1u,"T7: dst_token==1");
    ASSERT_EQ(ctx.mutant_active, 1u, "T7: mutant_active==1");
    ASSERT_EQ(ctx.mutant_count,  1u, "T7: mutant_count==1");

    /* Vector mutant phải khác h_prev (đã perturb)                  */
    float diff_norm = 0.0f;
    for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++) {
        float d2 = m->vec[k] - ctx.h_prev[k];
        diff_norm += d2 * d2;
    }
    ASSERT_GT(sqrtf(diff_norm), 1e-6f, "T7: mutant vec != h_prev");

    /* Spawn đủ EH_HGN_MAX_MUTANTS slots                            */
    for (uint32_t i = 1; i < EH_HGN_MAX_MUTANTS; i++)
        eh_hgn_mutant_spawn(&ctx, i, i+1, 1.0f + (float)i);
    ASSERT_EQ(ctx.mutant_active, EH_HGN_MAX_MUTANTS, "T7: pool full");

    /* Pool đầy → spawn thêm trả NULL                              */
    EH_HGN_MutantNode *overflow = eh_hgn_mutant_spawn(&ctx, 0, 0, 0.0f);
    ASSERT(overflow == NULL, "T7: overflow returns NULL");

    /* ---- T8: step_end deactivates all mutants ---- */
    fprintf(stderr, "\n-- T8: step_end --\n");
    eh_hgn_collapse_step_end(&ctx);
    ASSERT_EQ(ctx.mutant_active, 0u, "T8: mutant_active==0 after step_end");
    for (uint32_t i = 0; i < EH_HGN_MAX_MUTANTS; i++)
        ASSERT(ctx.mutant_pool[i].active == false,
               "T8: all mutants deactivated");

    /* ---- T9: ctx_reset giữ thresholds ---- */
    fprintf(stderr, "\n-- T9: ctx_reset --\n");
    ctx.collapse_thresh = 0.5f;   /* override */
    eh_hgn_collapse_ctx_reset(&ctx);
    ASSERT_FEQ(ctx.collapse_thresh, 0.5f, 1e-6f,
               "T9: reset preserves custom threshold");
    ASSERT_EQ(ctx.step_count, 0u, "T9: step_count reset to 0");
    ASSERT(ctx.h_prev_valid == false, "T9: h_prev_valid=false after reset");

    /* ---- T10: dump_stats no crash ---- */
    fprintf(stderr, "\n-- T10: dump_stats --\n");
    eh_hgn_collapse_gate(&ctx, h0);
    eh_hgn_collapse_gate(&ctx, h0);   /* trigger collapse           */
    eh_hgn_collapse_dump_stats(&ctx);
    ASSERT(1, "T10: dump_stats no crash");
    eh_hgn_collapse_dump_stats(NULL);
    ASSERT(1, "T10: dump_stats(NULL) no crash");

    /* ---- Summary ---- */
    fprintf(stderr, "\n=== %d/%d passed ===\n\n", g_pass, g_run);

    eh_arena_destroy(arena);
    return (g_pass == g_run) ? 0 : 1;
}