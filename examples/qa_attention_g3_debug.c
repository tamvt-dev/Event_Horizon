/* ================================================================
 * qa_attention_g3_debug.c — EH-G3 Debug Version with Embedding Analysis
 * 
 * Purpose: Investigate why all scores are 0.00 and embeddings aren't affecting ranking
 * 
 * Debug Output:
 *   1. Question embedding values [0..7]
 *   2. Candidate embedding values [0..7]
 *   3. Raw cosine similarity scores
 *   4. Beam scoring breakdown
 *   5. Hub penalty effect
 * ================================================================ */

#define _GNU_SOURCE
#include "../include/hgn/eh_hgn_dag.h"
#include "../include/hgn/eh_beam_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
 * Simple Tokenizer
 * ================================================================ */
static void simple_tokenize(const char *text, uint32_t *token_ids, uint32_t *count)
{
    *count = 0;
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return;
    strcpy(copy, text);
    
    char *word = strtok(copy, " \t\n");
    while (word && *count < 100) {
        uint32_t hash = 0;
        for (size_t i = 0; word[i]; i++) {
            hash += (uint32_t)word[i];
        }
        token_ids[*count] = hash % 2000;
        (*count)++;
        word = strtok(NULL, " \t\n");
    }
    free(copy);
}

/* ================================================================
 * Cosine Similarity with Debug Output
 * ================================================================ */
static float compute_cosine_debug(const float *vec1, const float *vec2, int dim, 
                                   const char *label)
{
    float dot_product = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;
    
    for (int i = 0; i < dim; i++) {
        dot_product += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }
    
    norm1 = sqrtf(norm1);
    norm2 = sqrtf(norm2);
    
    if (norm1 == 0 || norm2 == 0) return 0.0f;
    
    float cosine = dot_product / (norm1 * norm2);
    
    printf("    [%s]\n", label);
    printf("      dot_product: %.6f\n", dot_product);
    printf("      norm1:       %.6f\n", norm1);
    printf("      norm2:       %.6f\n", norm2);
    printf("      cosine:      %.6f\n", cosine);
    
    return cosine;
}

/* ================================================================
 * Debug: Print first N values of embedding
 * ================================================================ */
static void print_embedding_sample(const float *embedding, int dim, int n, 
                                   const char *label)
{
    printf("    %s: [", label);
    for (int i = 0; i < n && i < dim; i++) {
        printf("%.4f", embedding[i]);
        if (i < n - 1) printf(", ");
    }
    printf(" ...]\n");
}

/* ================================================================
 * Main Debug Analysis
 * ================================================================ */
int main(int argc, char *argv[])
{
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║        EH-G3 Debug: Embedding Impact Analysis        ║\n");
    printf("║     Why are all scores 0.00? Let's investigate!      ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    const char *dag_path = (argc > 1) ? argv[1] : "training/qa_model.ehdag";
    const char *embed_path = (argc > 2) ? argv[2] : "training/eh_g3_embeddings.bin";
    
    printf("Loading DAG from: %s\n", dag_path);
    printf("Loading embeddings from: %s\n\n", embed_path);
    
    /* Create Arena */
    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return 1;
    }
    
    /* Load DAG */
    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    
    EH_HGN_Status rc = eh_hgn_dag_load(arena, dag_path, &dag);
    if (rc != EH_HGN_OK) {
        fprintf(stderr, "ERROR: Failed to load DAG\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("[OK] DAG loaded: %u nodes, %u edges\n\n", dag.vocab_size, dag.total_edges);
    
    /* Load embeddings */
    int ret = eh_beam_load_query_embeddings(embed_path);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to load embeddings\n");
        eh_arena_destroy(arena);
        return 1;
    }
    
    printf("[OK] Embeddings loaded\n\n");
    
    /* ================================================================
     * DEBUG 1: Check global question embedding after loading
     * ================================================================ */
    printf("┌─ DEBUG 1: Global Question Embedding State ────────────┐\n");
    printf("│ After loading first embedding from file:              │\n");
    print_embedding_sample(eh_beam_question_embedding, EH_HGN_EMBED_DIM, 8,
                          "eh_beam_question_embedding");
    
    float loaded_embed_norm = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        loaded_embed_norm += eh_beam_question_embedding[i] * eh_beam_question_embedding[i];
    }
    loaded_embed_norm = sqrtf(loaded_embed_norm);
    printf("    Norm: %.6f\n", loaded_embed_norm);
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    /* ================================================================
     * DEBUG 2: Test question tokenization and compute embedding
     * ================================================================ */
    printf("┌─ DEBUG 2: Question Processing ────────────────────────┐\n");
    
    const char *test_question = "what is a computer";
    printf("│ Test question: '%s'\n", test_question);
    
    uint32_t q_tokens[100];
    uint32_t q_count = 0;
    simple_tokenize(test_question, q_tokens, &q_count);
    
    printf("│ Tokenized to %u tokens: [", q_count);
    for (uint32_t i = 0; i < q_count; i++) {
        printf("%u", q_tokens[i]);
        if (i < q_count - 1) printf(", ");
    }
    printf("]\n");
    
    /* Compute question embedding */
    float question_embedding[EH_HGN_EMBED_DIM];
    memset(question_embedding, 0, sizeof(question_embedding));
    
    for (uint32_t t = 0; t < q_count; t++) {
        uint32_t token_id = q_tokens[t];
        const float *token_vec = eh_hgn_dag_node_vec(&dag, token_id);
        if (token_vec) {
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
                question_embedding[i] += token_vec[i];
            }
            printf("│ Token %u (%u): found in DAG\n", t, token_id);
        } else {
            printf("│ Token %u (%u): NOT found in DAG!\n", t, token_id);
        }
    }
    
    /* Average */
    if (q_count > 0) {
        float scale = 1.0f / (float)q_count;
        for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
            question_embedding[i] *= scale;
        }
    }
    
    float q_embed_norm = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        q_embed_norm += question_embedding[i] * question_embedding[i];
    }
    q_embed_norm = sqrtf(q_embed_norm);
    
    printf("│ Computed question embedding (norm: %.6f):\n", q_embed_norm);
    print_embedding_sample(question_embedding, EH_HGN_EMBED_DIM, 8, "question_embedding");
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    /* ================================================================
     * DEBUG 3: Compare embeddings with DAG vectors
     * ================================================================ */
    printf("┌─ DEBUG 3: DAG Vector vs Question Embedding ───────────┐\n");
    
    uint32_t test_nodes[] = {1, 5, 10};
    for (int i = 0; i < 3; i++) {
        uint32_t node_id = test_nodes[i];
        const float *node_vec = eh_hgn_dag_node_vec(&dag, node_id);
        
        if (!node_vec) {
            printf("│ Node %u: NOT in DAG\n", node_id);
            continue;
        }
        
        printf("│ Node %u:\n", node_id);
        print_embedding_sample(node_vec, EH_HGN_EMBED_DIM, 8, "node_embedding");
        
        /* Compute similarity */
        float sim = compute_cosine_debug(question_embedding, (float*)node_vec, 
                                        EH_HGN_EMBED_DIM, "similarity");
        printf("\n");
    }
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    /* ================================================================
     * DEBUG 4: Check beam search scoring
     * ================================================================ */
    printf("┌─ DEBUG 4: Beam Search Initialization ─────────────────┐\n");
    
    /* Enable embedding use */
    eh_beam_use_question_embedding = 1;
    eh_beam_attention_mix = 0.3f;
    
    /* Copy computed embedding to global */
    memcpy(eh_beam_question_embedding, question_embedding, 
           EH_HGN_EMBED_DIM * sizeof(float));
    
    printf("│ Settings:\n");
    printf("│   eh_beam_use_question_embedding = %d\n", eh_beam_use_question_embedding);
    printf("│   eh_beam_attention_mix = %.2f\n", eh_beam_attention_mix);
    printf("│   eh_beam_use_hub_penalty = %d\n", eh_beam_use_hub_penalty);
    printf("│\n");
    printf("│ Global embedding after update (norm: %.6f):\n", q_embed_norm);
    print_embedding_sample(eh_beam_question_embedding, EH_HGN_EMBED_DIM, 8,
                          "global_embedding");
    
    /* Initialize beam search */
    EH_HGN_BeamTracker tracker;
    uint32_t start_token = q_tokens[0];
    eh_hgn_beam_init(&tracker, &dag, &start_token, 1);
    
    printf("│\n│ Beam initialized with token: %u\n", start_token);
    printf("│ Active paths: %u\n", tracker.active_paths);
    
    if (tracker.active_paths > 0) {
        EH_HGN_BeamPath *best = &tracker.paths[0];
        printf("│ First path: score=%.6f, len=%u\n", best->score, best->seq_len);
    }
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    /* ================================================================
     * DEBUG 5: Run one beam search step
     * ================================================================ */
    printf("┌─ DEBUG 5: First Beam Search Step ─────────────────────┐\n");
    
    uint32_t active = eh_hgn_beam_step(&tracker, &dag);
    
    printf("│ After step 1:\n");
    printf("│   Active beams: %u\n", active);
    printf("│   Total beams: %u\n", tracker.active_paths);
    
    for (uint32_t b = 0; b < tracker.active_paths && b < 3; b++) {
        EH_HGN_BeamPath *beam = &tracker.paths[b];
        printf("│\n│ Beam %u:\n", b);
        printf("│   Score: %.6f\n", beam->score);
        printf("│   Length: %u\n", beam->seq_len);
        printf("│   Finished: %s\n", beam->is_finished ? "YES" : "NO");
        
        if (beam->seq_len > 0) {
            printf("│   Last token: %u\n", beam->tokens[beam->seq_len - 1]);
        }
    }
    
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    /* ================================================================
     * Summary
     * ================================================================ */
    printf("┌─ Analysis Summary ────────────────────────────────────┐\n");
    printf("│                                                       │\n");
    
    if (q_embed_norm < 0.001f) {
        printf("│ ⚠️  Question embedding is near zero!               │\n");
        printf("│    Problem: Embedding not computed correctly       │\n");
    } else {
        printf("│ ✓ Question embedding has magnitude %.4f           │\n", q_embed_norm);
    }
    
    if (loaded_embed_norm < 0.001f) {
        printf("│ ⚠️  Loaded embedding is near zero!                 │\n");
        printf("│    Problem: File might be corrupted                │\n");
    } else {
        printf("│ ✓ Loaded embedding has magnitude %.4f             │\n", loaded_embed_norm);
    }
    
    printf("│                                                       │\n");
    printf("│ Next debugging steps:                                 │\n");
    printf("│ 1. Check if DAG node embeddings exist                │\n");
    printf("│ 2. Verify cosine similarities are non-zero           │\n");
    printf("│ 3. Check beam scoring computation                    │\n");
    printf("│ 4. Verify hub penalty isn't zeroing scores           │\n");
    printf("│                                                       │\n");
    printf("└───────────────────────────────────────────────────────┘\n\n");
    
    eh_arena_destroy(arena);
    return 0;
}
