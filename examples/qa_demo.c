/* ================================================================
 * examples/qa_demo.c — Simple Q&A Demo with Vocabulary
 *
 * Demonstrates:
 *   1. Training from Q&A corpus with vocabulary tracking
 *   2. Saving vocabulary to file
 *   3. Loading vocabulary and model
 *   4. Converting text → tokens → inference → text
 *
 * Usage:
 *   ./qa_demo train <corpus.txt> <model.ehdag> <vocab.txt>
 *   ./qa_demo ask <model.ehdag> <vocab.txt> <question>
 *
 * Example:
 *   ./qa_demo train qa_corpus.txt qa_model.ehdag qa_vocab.txt
 *   ./qa_demo ask qa_model.ehdag qa_vocab.txt "what is"
 *
 * Compile:
 *   gcc -O3 -std=c99 -Wall -Wextra \
 *       -Iinclude \
 *       examples/qa_demo.c \
 *       src/hgn/*.c src/core/*.c \
 *       -o qa_demo -lm
 * ================================================================ */

#include "hgn/eh_hgn_builder.h"
#include "hgn/eh_hgn_engine.h"
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_VOCAB_SIZE  1000
#define MAX_TOKEN_LEN   32
#define MAX_LINE_LEN    256

/* ----------------------------------------------------------------
 * Vocabulary
 * ---------------------------------------------------------------- */
typedef struct {
    char tokens[MAX_VOCAB_SIZE][MAX_TOKEN_LEN];
    uint32_t size;
} Vocabulary;

static Vocabulary g_vocab = {0};

/* Find token, return -1 if not found */
static int vocab_find(const char *token)
{
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        if (strcmp(g_vocab.tokens[i], token) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Add token, return ID */
static int vocab_add(const char *token)
{
    if (g_vocab.size >= MAX_VOCAB_SIZE) return -1;
    
    strncpy(g_vocab.tokens[g_vocab.size], token, MAX_TOKEN_LEN - 1);
    g_vocab.tokens[g_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
    return g_vocab.size++;
}

/* Get or add */
static int vocab_get_or_add(const char *token)
{
    int id = vocab_find(token);
    return (id >= 0) ? id : vocab_add(token);
}

/* Save vocabulary to file */
static int vocab_save(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        fprintf(fp, "%s\n", g_vocab.tokens[i]);
    }
    
    fclose(fp);
    return 0;
}

/* Load vocabulary from file */
static int vocab_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    g_vocab.size = 0;
    char line[MAX_TOKEN_LEN];
    
    while (fgets(line, sizeof(line), fp) && g_vocab.size < MAX_VOCAB_SIZE) {
        /* Remove newline */
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            vocab_add(line);
        }
    }
    
    fclose(fp);
    return 0;
}

/* Tokenize text */
static uint32_t tokenize(const char *text, uint32_t *tokens, uint32_t max_tokens)
{
    char buffer[MAX_LINE_LEN];
    strncpy(buffer, text, MAX_LINE_LEN - 1);
    buffer[MAX_LINE_LEN - 1] = '\0';
    
    uint32_t count = 0;
    char *token = strtok(buffer, " \t\n");
    
    while (token && count < max_tokens) {
        /* Convert to lowercase */
        for (char *p = token; *p; p++) *p = tolower(*p);
        
        int id = vocab_get_or_add(token);
        if (id >= 0) {
            tokens[count++] = (uint32_t)id;
        }
        token = strtok(NULL, " \t\n");
    }
    
    return count;
}

/* ----------------------------------------------------------------
 * Training Mode
 * ---------------------------------------------------------------- */
static int train_mode(const char *corpus_path, const char *model_path, 
                     const char *vocab_path)
{
    printf("\n=== Q&A Training Mode ===\n\n");
    printf("Corpus: %s\n", corpus_path);
    printf("Model:  %s\n", model_path);
    printf("Vocab:  %s\n\n", vocab_path);
    
    /* Read corpus and build vocabulary */
    FILE *fp = fopen(corpus_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open corpus\n");
        return 1;
    }
    
    /* Count tokens and build edges */
    uint32_t tokens[1024];
    uint32_t total_tokens = 0;
    uint32_t total_lines = 0;
    
    /* First pass: build vocabulary */
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize(line, tokens, 1024);
        total_tokens += n;
        total_lines++;
    }
    rewind(fp);
    
    printf("Pass 1: Vocabulary\n");
    printf("  Lines:  %u\n", total_lines);
    printf("  Tokens: %u\n", total_tokens);
    printf("  Vocab:  %u\n\n", g_vocab.size);
    
    /* Build graph */
    printf("Pass 2: Building graph...\n");
    
    EH_HGN_Builder *builder = eh_hgn_builder_create(g_vocab.size, 16);
    if (!builder) {
        fprintf(stderr, "Error: Failed to create builder\n");
        fclose(fp);
        return 1;
    }
    
    /* Set embeddings */
    float emb[EH_HGN_EMBED_DIM];
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        for (int j = 0; j < EH_HGN_EMBED_DIM; j++) {
            emb[j] = sinf((float)i * 0.1f + (float)j * 0.01f);
        }
        eh_hgn_builder_set_embedding(builder, i, emb);
    }
    
    /* Add edges from corpus */
    uint32_t edge_count = 0;
    float weight[EH_HGN_EMBED_DIM];
    
    while (fgets(line, sizeof(line), fp)) {
        uint32_t n = tokenize(line, tokens, 1024);
        
        for (uint32_t i = 0; i < n - 1; i++) {
            uint32_t src = tokens[i];
            uint32_t dst = tokens[i + 1];
            
            /* Generate weight */
            for (int j = 0; j < EH_HGN_EMBED_DIM; j++) {
                weight[j] = cosf((float)src * 0.2f + (float)dst * 0.3f + (float)j * 0.05f);
            }
            
            EH_HGN_BuilderStatus rc = eh_hgn_builder_add_edge(builder, src, dst, 
                                                               0.5f, weight);
            if (rc == EH_HGN_BUILDER_OK) edge_count++;
        }
    }
    fclose(fp);
    
    printf("  Edges added: %u\n\n", edge_count);
    
    /* Finalize and save */
    printf("Pass 3: Finalizing...\n");
    
    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);
    EH_HGN_BaseDag dag;
    
    EH_HGN_BuilderStatus rc = eh_hgn_builder_finalize(builder, arena, &dag);
    if (rc != EH_HGN_BUILDER_OK) {
        fprintf(stderr, "Error: Finalize failed\n");
        return 1;
    }
    
    EH_HGN_IO_Status io_rc = eh_hgn_io_save(&dag, model_path);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Save failed\n");
        return 1;
    }
    
    printf("  ✓ Model saved: %s\n", model_path);
    
    /* Save vocabulary */
    if (vocab_save(vocab_path) != 0) {
        fprintf(stderr, "Error: Vocab save failed\n");
        return 1;
    }
    
    printf("  ✓ Vocab saved: %s\n\n", vocab_path);
    
    printf("=== Training Complete ===\n\n");
    
    eh_hgn_builder_destroy(builder);
    eh_arena_destroy(arena);
    return 0;
}

/* ----------------------------------------------------------------
 * Ask Mode
 * ---------------------------------------------------------------- */
static int ask_mode(const char *model_path, const char *vocab_path, 
                   const char *question)
{
    printf("\n=== Q&A Ask Mode ===\n\n");
    
    /* Load vocabulary */
    if (vocab_load(vocab_path) != 0) {
        fprintf(stderr, "Error: Cannot load vocabulary\n");
        return 1;
    }
    printf("Vocabulary: %u tokens\n", g_vocab.size);
    
    /* Load model */
    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);
    EH_HGN_BaseDag dag;
    
    EH_HGN_IO_Status io_rc = eh_hgn_io_load(arena, model_path, &dag);
    if (io_rc != EH_HGN_IO_OK) {
        fprintf(stderr, "Error: Cannot load model\n");
        eh_arena_destroy(arena);
        return 1;
    }
    printf("Model:      %u tokens, %u edges\n\n", dag.vocab_size, dag.total_edges);
    
    /* Tokenize question */
    uint32_t tokens[64];
    uint32_t n = tokenize(question, tokens, 64);
    
    if (n == 0) {
        fprintf(stderr, "Error: No tokens in question\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Question: \"%s\"\n", question);
    printf("Tokens:   ");
    for (uint32_t i = 0; i < n; i++) {
        printf("%s ", g_vocab.tokens[tokens[i]]);
    }
    printf("\n\n");
    
    /* Run inference */
    EH_HGN_InferenceSession session;
    EH_HGN_EngineConfig config = eh_hgn_default_config();
    config.max_steps = 10;
    
    int rc = eh_hgn_session_init(&session, &dag, arena, tokens, n, &config);
    if (rc != 0) {
        fprintf(stderr, "Error: Session init failed\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("Answer:   ");
    
    /* Track last known length */
    uint32_t last_len = n;
    
    /* Generate continuation */
    while (!eh_hgn_session_is_done(&session)) {
        uint32_t active = eh_hgn_session_step(&session);
        if (active == 0) break;
        
        const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
        if (best && best->seq_len > last_len) {
            /* Print only new tokens */
            for (uint32_t i = last_len; i < best->seq_len; i++) {
                uint32_t token_id = best->tokens[i];
                if (token_id < g_vocab.size) {
                    printf("%s ", g_vocab.tokens[token_id]);
                    fflush(stdout);
                }
            }
            last_len = best->seq_len;
        }
    }
    printf("\n\n");
    
    eh_arena_destroy(arena);
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s train <corpus.txt> <model.ehdag> <vocab.txt>\n", argv[0]);
        fprintf(stderr, "  %s ask <model.ehdag> <vocab.txt> <question>\n", argv[0]);
        return 1;
    }
    
    const char *mode = argv[1];
    
    if (strcmp(mode, "train") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s train <corpus.txt> <model.ehdag> <vocab.txt>\n", 
                    argv[0]);
            return 1;
        }
        return train_mode(argv[2], argv[3], argv[4]);
    }
    else if (strcmp(mode, "ask") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s ask <model.ehdag> <vocab.txt> <question>\n", 
                    argv[0]);
            return 1;
        }
        return ask_mode(argv[2], argv[3], argv[4]);
    }
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }
}
