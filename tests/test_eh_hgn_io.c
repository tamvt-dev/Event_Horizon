/* ================================================================
 * tests/test_eh_hgn_io.c — Unit test I/O Layer: eh_hgn_io
 *
 * Tests:
 *   - Save/load round-trip
 *   - Validation functions
 *   - Header reading
 *   - Error handling (bad magic, version, sizes)
 *   - Alignment requirements
 *
 * Compile from EventHorizon/ root:
 *   gcc -O3 -std=c99 -mavx2 -Wall -Wextra \
 *       -Iinclude \
 *       tests/test_eh_hgn_io.c \
 *       src/hgn/eh_hgn_io.c src/hgn/eh_hgn_dag.c src/core/eh_arena.c \
 *       -o tests/test_eh_hgn_io -lm && tests/test_eh_hgn_io
 * ================================================================ */

#define _POSIX_C_SOURCE 200112L

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
static const char *FILE_VALID   = "/tmp/test_io_valid.bin";
static const char *FILE_SAVE    = "/tmp/test_io_save.bin";
static const char *FILE_BAD     = "/tmp/test_io_bad.bin";

/* ----------------------------------------------------------------
 * Helper: Create valid test file (vocab=16, edges=32, K=4)
 * ---------------------------------------------------------------- */
static int create_test_file(const char *path)
{
    const uint32_t V = 16, E = 32, K = 4;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* Header */
    EH_HGN_IO_Header hdr = {
        .magic       = EH_HGN_IO_MAGIC,
        .version     = EH_HGN_IO_VERSION,
        .vocab_size  = V,
        .total_edges = E,
        .embed_dim   = EH_HGN_EMBED_DIM,
        .max_fanout  = K,
        .reserved    = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* node_embed: vec[k] = i*1000 + k */
    EH_HGN_NodeEmbed ne;
    for (uint32_t i = 0; i < V; i++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ne.vec[k] = (float)(i * 1000 + k);
        fwrite(&ne, sizeof(ne), 1, fp);
    }

    /* node_adj: CSR, 2 edges per node */
    EH_HGN_NodeAdj adj;
    for (uint32_t i = 0; i < V; i++) {
        adj.edge_offset = i * 2;
        adj.edge_count  = 2;
        fwrite(&adj, sizeof(adj), 1, fp);
    }

    /* edge_compact: dst=(i+1)%V and (i+2)%V */
    EH_HGN_EdgeCompact ec;
    for (uint32_t i = 0; i < V; i++) {
        ec.dst        = (i + 1) % V;
        ec.prior      = 0.5f + (float)i;
        ec.weight_idx = i * 2;
        fwrite(&ec, sizeof(ec), 1, fp);

        ec.dst        = (i + 2) % V;
        ec.prior      = 0.7f + (float)i;
        ec.weight_idx = i * 2 + 1;
        fwrite(&ec, sizeof(ec), 1, fp);
    }

    /* Padding to 32-byte alignment */
    long pos = ftell(fp);
    long apos = (pos + 31L) & ~31L;
    if (apos != pos) {
        uint8_t pad[32] = {0};
        fwrite(pad, (size_t)(apos - pos), 1, fp);
    }

    /* weight_pool: vec[k] = e*2000 + k + 5000 */
    EH_HGN_EdgeWeight ew;
    for (uint32_t e = 0; e < E; e++) {
        for (uint32_t k = 0; k < EH_HGN_EMBED_DIM; k++)
            ew.vec[k] = (float)(e * 2000 + k + 5000);
        fwrite(&ew, sizeof(ew), 1, fp);
    }

    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Helper: Create file with bad magic
 * ---------------------------------------------------------------- */
static int create_bad_magic_file(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    EH_HGN_IO_Header hdr = {
        .magic = 0xDEADBEEF,  /* Wrong magic */
        .version = EH_HGN_IO_VERSION,
        .vocab_size = 16,
        .total_edges = 32,
        .embed_dim = EH_HGN_EMBED_DIM,
        .max_fanout = 4,
        .reserved = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Helper: Create file with bad version
 * ---------------------------------------------------------------- */
static int create_bad_version_file(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    EH_HGN_IO_Header hdr = {
        .magic = EH_HGN_IO_MAGIC,
        .version = 999,  /* Unsupported version */
        .vocab_size = 16,
        .total_edges = 32,
        .embed_dim = EH_HGN_EMBED_DIM,
        .max_fanout = 4,
        .reserved = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * Helper: Create file with size overflow
 * ---------------------------------------------------------------- */
static int create_bad_size_file(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    EH_HGN_IO_Header hdr = {
        .magic = EH_HGN_IO_MAGIC,
        .version = EH_HGN_IO_VERSION,
        .vocab_size = EH_HGN_VOCAB_SIZE + 1,  /* Exceeds limit */
        .total_edges = 32,
        .embed_dim = EH_HGN_EMBED_DIM,
        .max_fanout = 4,
        .reserved = {0, 0}
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fclose(fp);
    return 0;
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    fprintf(stderr, "\n=== EH_HGN_IO Tests ===\n\n");

    /* Setup */
    ASSERT(create_test_file(FILE_VALID) == 0, "T0: create valid test file");

    /* ---- T1: Load valid file ---- */
    fprintf(stderr, "\n-- T1: Load valid file --\n");
    EH_Arena *arena = eh_arena_create(8 * 1024 * 1024);  /* 8MB */
    ASSERT_NEQ((uintptr_t)arena, 0u, "T1: arena created");

    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    EH_HGN_IO_Status status = eh_hgn_io_load(arena, FILE_VALID, &dag);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T1: load returns OK");

    /* ---- T2: Verify loaded data ---- */
    fprintf(stderr, "\n-- T2: Verify loaded data --\n");
    ASSERT_EQ(dag.vocab_size, 16u, "T2: vocab_size");
    ASSERT_EQ(dag.total_edges, 32u, "T2: total_edges");
    ASSERT_EQ(dag.embed_dim, 128u, "T2: embed_dim");
    ASSERT_EQ(dag.max_fanout, 4u, "T2: max_fanout");
    ASSERT_NEQ((uintptr_t)dag.node_embed, 0u, "T2: node_embed allocated");
    ASSERT_NEQ((uintptr_t)dag.node_adj, 0u, "T2: node_adj allocated");
    ASSERT_NEQ((uintptr_t)dag.edge_compact, 0u, "T2: edge_compact allocated");
    ASSERT_NEQ((uintptr_t)dag.weight_pool, 0u, "T2: weight_pool allocated");

    /* ---- T3: Verify embeddings ---- */
    fprintf(stderr, "\n-- T3: Verify embeddings --\n");
    const float *nv0 = dag.node_embed[0].vec;
    ASSERT_FEQ(nv0[0], 0.0f, "T3: node[0][0] == 0");
    ASSERT_FEQ(nv0[127], 127.0f, "T3: node[0][127] == 127");

    const float *nv5 = dag.node_embed[5].vec;
    ASSERT_FEQ(nv5[0], 5000.0f, "T3: node[5][0] == 5000");
    ASSERT_FEQ(nv5[127], 5127.0f, "T3: node[5][127] == 5127");

    /* ---- T4: Verify CSR structure ---- */
    fprintf(stderr, "\n-- T4: Verify CSR structure --\n");
    ASSERT_EQ(dag.node_adj[0].edge_offset, 0u, "T4: node[0] offset");
    ASSERT_EQ(dag.node_adj[0].edge_count, 2u, "T4: node[0] count");
    ASSERT_EQ(dag.edge_compact[0].dst, 1u, "T4: edge[0].dst");
    ASSERT_EQ(dag.edge_compact[1].dst, 2u, "T4: edge[1].dst");
    ASSERT_FEQ(dag.edge_compact[0].prior, 0.5f, "T4: edge[0].prior");

    /* ---- T5: Verify weights ---- */
    fprintf(stderr, "\n-- T5: Verify weights --\n");
    const float *w0 = dag.weight_pool[0].vec;
    ASSERT_FEQ(w0[0], 5000.0f, "T5: weight[0][0]");
    ASSERT_FEQ(w0[127], 5127.0f, "T5: weight[0][127]");

    const float *w10 = dag.weight_pool[10].vec;
    ASSERT_FEQ(w10[0], 25000.0f, "T5: weight[10][0] == 20000+5000");
    ASSERT_FEQ(w10[127], 25127.0f, "T5: weight[10][127]");

    /* ---- T6: Save to new file ---- */
    fprintf(stderr, "\n-- T6: Save to new file --\n");
    status = eh_hgn_io_save(&dag, FILE_SAVE);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T6: save returns OK");

    /* ---- T7: Load saved file (round-trip) ---- */
    fprintf(stderr, "\n-- T7: Round-trip test --\n");
    EH_Arena *arena2 = eh_arena_create(8 * 1024 * 1024);  /* New arena for round-trip */
    EH_HGN_BaseDag dag2;
    memset(&dag2, 0, sizeof(dag2));
    status = eh_hgn_io_load(arena2, FILE_SAVE, &dag2);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T7: load saved file OK");

    ASSERT_EQ(dag2.vocab_size, dag.vocab_size, "T7: vocab_size match");
    ASSERT_EQ(dag2.total_edges, dag.total_edges, "T7: total_edges match");
    ASSERT_EQ(dag2.embed_dim, dag.embed_dim, "T7: embed_dim match");
    ASSERT_EQ(dag2.max_fanout, dag.max_fanout, "T7: max_fanout match");

    /* ---- T8: Verify round-trip data integrity ---- */
    fprintf(stderr, "\n-- T8: Round-trip data integrity --\n");
    const float *nv2_5 = dag2.node_embed[5].vec;
    ASSERT_FEQ(nv2_5[0], 5000.0f, "T8: node[5][0] preserved");
    ASSERT_FEQ(nv2_5[127], 5127.0f, "T8: node[5][127] preserved");

    ASSERT_EQ(dag2.edge_compact[0].dst, 1u, "T8: edge[0].dst preserved");
    ASSERT_FEQ(dag2.edge_compact[0].prior, 0.5f, "T8: edge[0].prior preserved");

    const float *w2_10 = dag2.weight_pool[10].vec;
    ASSERT_FEQ(w2_10[0], 25000.0f, "T8: weight[10][0] preserved");

    /* ---- T9: Validate function ---- */
    fprintf(stderr, "\n-- T9: Validate function --\n");
    status = eh_hgn_io_validate(FILE_VALID);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T9: validate valid file");

    status = eh_hgn_io_validate(FILE_SAVE);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T9: validate saved file");

    status = eh_hgn_io_validate("/nonexistent/file.bin");
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T9: validate nonexistent file");

    /* ---- T10: Read header ---- */
    fprintf(stderr, "\n-- T10: Read header --\n");
    EH_HGN_IO_Header hdr;
    status = eh_hgn_io_read_header(FILE_VALID, &hdr);
    ASSERT_EQ(status, EH_HGN_IO_OK, "T10: read_header OK");
    ASSERT_EQ(hdr.magic, EH_HGN_IO_MAGIC, "T10: magic correct");
    ASSERT_EQ(hdr.version, EH_HGN_IO_VERSION, "T10: version correct");
    ASSERT_EQ(hdr.vocab_size, 16u, "T10: vocab_size correct");
    ASSERT_EQ(hdr.total_edges, 32u, "T10: total_edges correct");

    /* ---- T11: Error - bad magic ---- */
    fprintf(stderr, "\n-- T11: Error handling - bad magic --\n");
    ASSERT(create_bad_magic_file(FILE_BAD) == 0, "T11: create bad magic file");
    EH_Arena *arena_bad = eh_arena_create(4 * 1024 * 1024);
    EH_HGN_BaseDag dag_bad;
    status = eh_hgn_io_load(arena_bad, FILE_BAD, &dag_bad);
    ASSERT_EQ(status, EH_HGN_IO_ERR_MAGIC, "T11: load bad magic");

    status = eh_hgn_io_validate(FILE_BAD);
    ASSERT_EQ(status, EH_HGN_IO_ERR_MAGIC, "T11: validate bad magic");

    /* ---- T12: Error - bad version ---- */
    fprintf(stderr, "\n-- T12: Error handling - bad version --\n");
    ASSERT(create_bad_version_file(FILE_BAD) == 0, "T12: create bad version file");
    eh_arena_reset(arena_bad);
    status = eh_hgn_io_load(arena_bad, FILE_BAD, &dag_bad);
    ASSERT_EQ(status, EH_HGN_IO_ERR_VERSION, "T12: load bad version");

    status = eh_hgn_io_validate(FILE_BAD);
    ASSERT_EQ(status, EH_HGN_IO_ERR_VERSION, "T12: validate bad version");

    /* ---- T13: Error - size overflow ---- */
    fprintf(stderr, "\n-- T13: Error handling - size overflow --\n");
    ASSERT(create_bad_size_file(FILE_BAD) == 0, "T13: create bad size file");
    eh_arena_reset(arena_bad);
    status = eh_hgn_io_load(arena_bad, FILE_BAD, &dag_bad);
    ASSERT_EQ(status, EH_HGN_IO_ERR_SIZE, "T13: load size overflow");

    status = eh_hgn_io_validate(FILE_BAD);
    ASSERT_EQ(status, EH_HGN_IO_ERR_SIZE, "T13: validate size overflow");

    eh_arena_destroy(arena_bad);

    /* ---- T14: Error - NULL parameters ---- */
    fprintf(stderr, "\n-- T14: Error handling - NULL params --\n");
    status = eh_hgn_io_load(NULL, FILE_VALID, &dag);
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T14: load NULL arena");

    status = eh_hgn_io_load(arena, NULL, &dag);
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T14: load NULL path");

    status = eh_hgn_io_load(arena, FILE_VALID, NULL);
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T14: load NULL dag");

    status = eh_hgn_io_save(NULL, FILE_SAVE);
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T14: save NULL dag");

    status = eh_hgn_io_save(&dag, NULL);
    ASSERT_EQ(status, EH_HGN_IO_ERR_FILE, "T14: save NULL path");

    /* ---- T15: Error strings ---- */
    fprintf(stderr, "\n-- T15: Error strings --\n");
    const char *msg;
    msg = eh_hgn_io_strerror(EH_HGN_IO_OK);
    ASSERT_NEQ((uintptr_t)msg, 0u, "T15: strerror OK");
    ASSERT_NEQ((uintptr_t)strstr(msg, "Success"), 0u, "T15: OK message");

    msg = eh_hgn_io_strerror(EH_HGN_IO_ERR_MAGIC);
    ASSERT_NEQ((uintptr_t)strstr(msg, "magic"), 0u, "T15: magic message");

    msg = eh_hgn_io_strerror(EH_HGN_IO_ERR_VERSION);
    ASSERT_NEQ((uintptr_t)strstr(msg, "version"), 0u, "T15: version message");

    msg = eh_hgn_io_strerror(EH_HGN_IO_ERR_CORRUPT);
    ASSERT_NEQ((uintptr_t)strstr(msg, "corrupt"), 0u, "T15: corrupt message");

    /* ---- T16: Full graph integrity after round-trip ---- */
    fprintf(stderr, "\n-- T16: Full graph integrity --\n");
    uint32_t edge_count2 = 0;
    for (uint32_t i = 0; i < dag2.vocab_size; i++) {
        edge_count2 += dag2.node_adj[i].edge_count;
    }
    ASSERT_EQ(edge_count2, dag2.total_edges, "T16: CSR edges == total_edges");
    ASSERT_EQ(edge_count2, 32u, "T16: edge count after round-trip");
    
    /* Verify CSR integrity: all edges within bounds */
    int csr_valid = 1;
    for (uint32_t i = 0; i < dag2.vocab_size; i++) {
        uint32_t off = dag2.node_adj[i].edge_offset;
        uint32_t cnt = dag2.node_adj[i].edge_count;
        if (off + cnt > dag2.total_edges) {
            csr_valid = 0;
            break;
        }
    }
    ASSERT(csr_valid, "T16: CSR bounds valid");

    /* Cleanup */
    eh_arena_destroy(arena);
    eh_arena_destroy(arena2);

    /* Summary */
    fprintf(stderr, "\n=== %d/%d passed ===\n\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
