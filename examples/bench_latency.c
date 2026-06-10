/* ================================================================
 * bench_latency.c — Per-query latency benchmark for EH-G3
 *
 * Measures: model load time, inference latency (cold + warm),
 *           memory footprint, and throughput (queries/sec).
 *
 * Usage:
 *   ./bench_latency <model.ehdag> <vocab.txt> <pairs.txt> [iterations]
 *
 * Compile:
 *   gcc -O3 -std=c99 -march=native -Iinclude -Iinclude/core -Iinclude/hgn \
 *       src/core/*.c src/hgn/*.c examples/bench_latency.c \
 *       -o examples/bench_latency -lm
 * ================================================================ */

#define _GNU_SOURCE
#include "../include/hgn/eh_hgn_dag.h"
#include "../include/hgn/eh_beam_search.h"
#include "../include/core/eh_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* ── Timing ────────────────────────────────────────────────── */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* ── Vocab (minimal, same as qa_attention_g3) ──────────────── */
#define MAX_VOCAB  8000
#define MAX_TOKEN  64
#define MAX_PAIRS  20000

typedef struct { char w[MAX_TOKEN]; } Word;
static Word   g_vocab[MAX_VOCAB];
static uint32_t g_vocab_size = 0;

typedef struct { uint32_t a, b; } Pair;
static Pair     g_pairs[MAX_PAIRS];
static uint32_t g_pair_count = 0;

static void load_vocab(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "Cannot open vocab: %s\n", path); return; }
    char line[MAX_TOKEN];
    while (fgets(line, sizeof(line), fp) && g_vocab_size < MAX_VOCAB) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            strncpy(g_vocab[g_vocab_size++].w, line, MAX_TOKEN - 1);
        }
    }
    fclose(fp);
}

static void load_pairs(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "Cannot open pairs: %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof(line), fp) && g_pair_count < MAX_PAIRS) {
        if (line[0] == '#') continue;
        uint32_t id, a, b;
        if (sscanf(line, "%u %u %u", &id, &a, &b) == 3 && id < MAX_PAIRS) {
            g_pairs[id].a = a;
            g_pairs[id].b = b;
            if (id >= g_pair_count) g_pair_count = id + 1;
        }
    }
    fclose(fp);
}

static int find_vocab(const char *w)
{
    for (uint32_t i = 0; i < g_vocab_size; i++)
        if (strcmp(g_vocab[i].w, w) == 0) return (int)i;
    return -1;
}

static int find_pair(uint32_t a, uint32_t b)
{
    for (uint32_t i = 0; i < g_pair_count; i++)
        if (g_pairs[i].a == a && g_pairs[i].b == b) return (int)i;
    return -1;
}

/* ── Single inference ──────────────────────────────────────── */
static double run_query(const EH_HGN_BaseDag *dag, const char *question,
                        uint32_t max_steps)
{
    double t0 = now_ms();

    /* Tokenize */
    char buf[256];
    strncpy(buf, question, sizeof(buf) - 1);
    for (char *p = buf; *p; p++) *p = tolower(*p);

    uint32_t ids[64];
    uint32_t n = 0;
    char *tok = strtok(buf, " \t");
    while (tok && n < 64) {
        int id = find_vocab(tok);
        ids[n++] = (id >= 0) ? (uint32_t)id : (uint32_t)(id % dag->vocab_size);
        tok = strtok(NULL, " \t");
    }

    /* Find start pair */
    uint32_t start = 0;
    int found = 0;
    for (int i = (int)n - 2; i >= 0 && !found; i--) {
        int pid = find_pair(ids[i], ids[i+1]);
        if (pid >= 0 && eh_hgn_dag_fanout(dag, (uint32_t)pid) > 0) {
            start = (uint32_t)pid;
            found = 1;
        }
    }
    if (!found) {
        /* fallback: highest fanout */
        uint32_t best = 0, bfan = 0;
        for (uint32_t i = 0; i < dag->vocab_size; i++) {
            uint32_t f = eh_hgn_dag_fanout(dag, i);
            if (f > bfan) { bfan = f; best = i; }
        }
        start = best;
    }

    /* Beam search */
    EH_HGN_BeamPath paths[8];
    EH_HGN_BeamTracker tracker;
    eh_hgn_beam_init(&tracker, dag, &start, 1, paths, 8);

    uint32_t active = 1, step = 0;
    while (active > 0 && step < max_steps) {
        active = eh_hgn_beam_step(&tracker, dag);
        step++;
    }

    return now_ms() - t0;
}

/* ── Main ──────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <model.ehdag> <vocab.txt> <pairs.txt> [iterations]\n"
            "\nExample:\n"
            "  ./bench_latency training/trigram_v10_model.ehdag \\\n"
            "    training/trigram_v10_vocab.txt \\\n"
            "    training/trigram_v10_vocab.txt.pairs\n", argv[0]);
        return 1;
    }

    const char *model_path = argv[1];
    const char *vocab_path = argv[2];
    const char *pairs_path = argv[3];
    int         iterations = (argc > 4) ? atoi(argv[4]) : 100;
    int         max_steps  = 6;

    printf("\n=== EH-G3 Latency Benchmark ===\n\n");
    printf("Model  : %s\n", model_path);
    printf("Iters  : %d per query\n", iterations);
    printf("Steps  : %d (max_steps)\n\n", max_steps);

    /* ── Load model ─────────────────────────────────────────── */
    double t_load_start = now_ms();

    EH_Arena *arena = eh_arena_create(256 * 1024 * 1024);
    load_vocab(vocab_path);
    load_pairs(pairs_path);

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    EH_HGN_Status rc = eh_hgn_dag_load(arena, model_path, &dag);
    if (rc != EH_HGN_OK) {
        fprintf(stderr, "ERROR: Failed to load model (status=%d)\n", rc);
        return 1;
    }

    double t_load = now_ms() - t_load_start;
    printf("Load time  : %.2f ms\n", t_load);
    printf("Vocab size : %u tokens\n", g_vocab_size);
    printf("Pairs      : %u pairs\n", g_pair_count);
    printf("Graph      : %u nodes, %u edges\n\n",
           dag.vocab_size, dag.total_edges);

    /* ── Test queries ───────────────────────────────────────── */
    const char *queries[] = {
        "what is a computer",
        "what is a database",
        "what is a tree",
        "what is your name",
        "how are you",
        "what is the capital of france",
        "what is python",
        "what is gravity",
        "what is photosynthesis",
        "hello",
    };
    int nq = sizeof(queries) / sizeof(queries[0]);

    printf("%-45s %8s %8s %8s\n", "Query", "Cold(ms)", "Warm(ms)", "Avg(ms)");
    printf("%-45s %8s %8s %8s\n",
           "---------------------------------------------",
           "--------", "--------", "--------");

    double total_cold = 0, total_warm_sum = 0;
    int    total_warm_count = 0;

    for (int q = 0; q < nq; q++) {
        /* Cold run */
        double cold = run_query(&dag, queries[q], (uint32_t)max_steps);
        total_cold += cold;

        /* Warm runs */
        double warm_sum = 0;
        for (int i = 0; i < iterations; i++) {
            warm_sum += run_query(&dag, queries[q], (uint32_t)max_steps);
        }
        double warm_avg = warm_sum / iterations;
        total_warm_sum += warm_sum;
        total_warm_count += iterations;

        printf("%-45s %8.3f %8.3f %8.3f\n",
               queries[q], cold, warm_avg,
               (cold + warm_sum) / (1 + iterations));
    }

    double overall_avg = total_warm_sum / total_warm_count;
    double cold_avg    = total_cold / nq;

    printf("\n");
    printf("=== Summary ===\n");
    printf("  Cold avg   : %.3f ms/query\n", cold_avg);
    printf("  Warm avg   : %.3f ms/query\n", overall_avg);
    printf("  Throughput : %.0f queries/sec\n", 1000.0 / overall_avg);

    /* ── Target check ───────────────────────────────────────── */
    printf("\n=== Target Check (25ms SLA) ===\n");
    printf("  Cold: %s (%.3f ms)\n",
           cold_avg <= 25.0 ? "✓ PASS" : "✗ FAIL", cold_avg);
    printf("  Warm: %s (%.3f ms)\n",
           overall_avg <= 25.0 ? "✓ PASS" : "✗ FAIL", overall_avg);

    /* ── Memory estimate ────────────────────────────────────── */
    size_t model_bytes = (size_t)dag.vocab_size * EH_HGN_EMBED_DIM * sizeof(float)
                       + (size_t)dag.total_edges * 140;
    printf("\n=== Memory ===\n");
    printf("  Arena capacity : %.1f MB\n", 256.0);
    printf("  Model estimate : %.2f MB\n", (double)model_bytes / (1024.0 * 1024.0));

    eh_arena_destroy(arena);
    printf("\nDone.\n\n");
    return 0;
}
