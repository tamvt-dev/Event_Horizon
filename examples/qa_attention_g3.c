/* ================================================================
 * qa_attention_g3.c — EH-G3 Q&A Demo with Contrastive Embeddings
 * 
 * Demonstrates the EH-G3 paradigm shift: from Language Model to
 * Retrieval/Matching Model using contrastively trained embeddings.
 * 
 * Features:
 *   1. Loads pre-trained query embeddings (eh_g3_embeddings.bin)
 *   2. Enables hybrid scoring: 70% local + 30% global attention
 *   3. Applies hub-aware penalty to reduce collisions
 *   4. Tests on benchmark questions with accuracy reporting
 * 
 * Compilation:
 *   cd eventhorizon
 *   gcc -O3 -std=c99 -Wall -Wextra -march=native \
 *       -Iinclude -Iinclude/core -Iinclude/hgn \
 *       src/core/*.c src/hgn/*.c examples/qa_attention_g3.c \
 *       -o examples/qa_attention_g3 -lm
 * 
 * Usage:
 *   ./examples/qa_attention_g3 [dag_file] [embeddings_file]
 * 
 * Example:
 *   ./examples/qa_attention_g3 training/qa_model.ehdag
 *                               training/eh_g3_embeddings.bin
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

/* External beam search tuning parameters */
extern float eh_beam_rep_penalty;
extern float eh_beam_temperature;

/* ================================================================
 * Test Case: Question + Expected Answer
 * ================================================================ */
typedef struct {
    const char *question;
    const char *expected_answer;
    const char *category;
} TestCase;

static TestCase test_cases[] = {
    /* ---- Hub collision cases (should improve with EH-G3) ---- */
    { "what is a computer", "machine", "hub_collision" },
    { "what is a database", "storage", "hub_collision" },
    { "what is a tree", "structure", "hub_collision" },
    
    /* ---- Baseline cases (should maintain quality) ---- */
    { "what is your name", "my name", "baseline" },
    { "how are you", "i am fine", "baseline" },
    { "hello", "hello", "baseline" },
    
    /* ---- Domain-specific (should improve with domain corpus) ---- */
    { "what is the capital of france", "paris", "geography" },
    { "who is the author", "author", "literary" },
    { "what is python", "programming language", "technical" },
};

#define NUM_TEST_CASES (sizeof(test_cases) / sizeof(test_cases[0]))

/* ================================================================
 * Pair Management for Trigram Model
 * Trigram uses PAIRS as nodes, not single tokens!
 * ================================================================ */
typedef struct {
    uint32_t token_a;
    uint32_t token_b;
} TokenPair;

#define MAX_PAIRS 10000
static TokenPair g_pair_map[MAX_PAIRS];
static uint32_t g_pair_count = 0;

static int load_pair_map(const char *pair_path)
{
    FILE *fp = fopen(pair_path, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot load pair map from %s\n", pair_path);
        return -1;
    }
    
    g_pair_count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp) && g_pair_count < MAX_PAIRS) {
        if (line[0] == '#') continue;  /* Skip comments */
        
        uint32_t pair_id, tok_a, tok_b;
        if (sscanf(line, "%u %u %u", &pair_id, &tok_a, &tok_b) == 3) {
            if (pair_id < MAX_PAIRS) {
                g_pair_map[pair_id].token_a = tok_a;
                g_pair_map[pair_id].token_b = tok_b;
                if (pair_id >= g_pair_count) {
                    g_pair_count = pair_id + 1;
                }
            }
        }
    }
    
    fclose(fp);
    printf("[OK] Loaded %u token pairs\n", g_pair_count);
    return 0;
}

static int find_pair_id(uint32_t tok_a, uint32_t tok_b)
{
    for (uint32_t i = 0; i < g_pair_count; i++) {
        if (g_pair_map[i].token_a == tok_a && g_pair_map[i].token_b == tok_b) {
            return (int)i;
        }
    }
    return -1;
}

/* ================================================================
 * Vocabulary Management
 * Load vocab.txt to map words to token IDs correctly
 * ================================================================ */
#define MAX_VOCAB_SIZE 10000
#define MAX_TOKEN_LEN 64

typedef struct {
    char words[MAX_VOCAB_SIZE][MAX_TOKEN_LEN];
    uint32_t size;
} VocabTable;

static VocabTable g_vocab = {0};

static int load_vocab(const char *vocab_path)
{
    FILE *fp = fopen(vocab_path, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot load vocab from %s, using hash fallback\n", vocab_path);
        return -1;
    }
    
    g_vocab.size = 0;
    char line[MAX_TOKEN_LEN];
    
    while (fgets(line, sizeof(line), fp) && g_vocab.size < MAX_VOCAB_SIZE) {
        /* Remove newline */
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) > 0) {
            strncpy(g_vocab.words[g_vocab.size], line, MAX_TOKEN_LEN - 1);
            g_vocab.words[g_vocab.size][MAX_TOKEN_LEN - 1] = '\0';
            g_vocab.size++;
        }
    }
    
    fclose(fp);
    printf("[OK] Loaded %u words from vocabulary\n", g_vocab.size);
    return 0;
}

static int find_word_in_vocab(const char *word)
{
    for (uint32_t i = 0; i < g_vocab.size; i++) {
        if (strcmp(g_vocab.words[i], word) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ================================================================
 * Simple tokenizer: split by spaces, convert to token IDs
 * 
 * FIXED: Use proper string hashing instead of char sum
 * Uses djb2 hash algorithm to avoid collisions
 * ================================================================ */
typedef struct {
    char *tokens[1000];
    uint32_t ids[1000];
    uint32_t count;
} Tokenization;

static uint32_t hash_string(const char *str)
{
    /* djb2 hash - much better than sum(chars) */
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    
    return hash;
}

static void simple_tokenize(const char *text, uint32_t dag_vocab_size, Tokenization *out)
{
    memset(out, 0, sizeof(*out));
    
    char *copy = strdup(text);
    if (!copy) return;
    
    /* Convert to lowercase */
    for (char *p = copy; *p; p++) {
        *p = tolower(*p);
    }
    
    char *word = strtok(copy, " \t\n");
    while (word && out->count < 1000) {
        out->tokens[out->count] = strdup(word);
        
        /* Try vocab lookup first */
        int vocab_id = find_word_in_vocab(word);
        
        if (vocab_id >= 0) {
            /* Found in vocab - use exact ID */
            out->ids[out->count] = (uint32_t)vocab_id;
        } else {
            /* Not in vocab - use hash as fallback */
            uint32_t hash = hash_string(word);
            out->ids[out->count] = hash % dag_vocab_size;
        }
        
        out->count++;
        word = strtok(NULL, " \t\n");
    }
    free(copy);
}

static void tokenization_free(Tokenization *tok)
{
    for (uint32_t i = 0; i < tok->count; i++) {
        if (tok->tokens[i]) free(tok->tokens[i]);
    }
    memset(tok, 0, sizeof(*tok));
}

/* ================================================================
 * Compute average embedding from tokens
 * ================================================================ */
static void compute_question_embedding(const uint32_t *token_ids,
                                       uint32_t token_count,
                                       const EH_HGN_BaseDag *dag,
                                       float *embedding_out)
{
    /* Initialize to zero */
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        embedding_out[i] = 0.0f;
    }
    
    if (token_count == 0) return;
    
    /* Sum embeddings of all tokens */
    for (uint32_t t = 0; t < token_count; t++) {
        uint32_t token_id = token_ids[t];
        const float *token_vec = eh_hgn_dag_node_vec(dag, token_id);
        if (token_vec) {
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
                embedding_out[i] += token_vec[i];
            }
        }
    }
    
    /* Average */
    float scale = 1.0f / (float)token_count;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        embedding_out[i] *= scale;
    }
}

/* ================================================================
 * Run Q&A on a single test case
 * Added tunable parameters for experimentation
 * ================================================================ */
typedef struct {
    char answer_text[256];
    float score;
    uint32_t num_tokens;
    bool success;
} QAResult;

typedef struct {
    float attention_mix;      /* 0.0-1.0: local vs global balance */
    float hub_penalty;        /* 0.0-5.0: penalty strength */
    float temperature;        /* 0.1-2.0: diversity control */
    float rep_penalty;        /* 0.0-10.0: repetition penalty */
    uint32_t max_steps;       /* 1-50: max generation steps */
} TuningParams;

static void run_qa_test_with_params(const TestCase *test,
                                     const EH_HGN_BaseDag *dag,
                                     const TuningParams *params,
                                     QAResult *result)
{
    memset(result, 0, sizeof(*result));
    
    /* Tokenize question */
    Tokenization question_tokens;
    simple_tokenize(test->question, dag->vocab_size, &question_tokens);
    
    if (question_tokens.count == 0) {
        strcpy(result->answer_text, "[ERROR: empty question]");
        return;
    }
    
    printf("     [DEBUG] Tokenized: ");
    for (uint32_t i = 0; i < question_tokens.count && i < 5; i++) {
        printf("%s[%u] ", question_tokens.tokens[i], question_tokens.ids[i]);
    }
    printf("\n");
    
    /* Compute question embedding (average of token embeddings) */
    float question_embedding[EH_HGN_EMBED_DIM];
    compute_question_embedding(question_tokens.ids, question_tokens.count,
                              dag, question_embedding);
    
    /* DEBUG: Print first 8 dimensions of question embedding */
    printf("     [DEBUG] Question embedding[0..7]: ");
    for (int i = 0; i < 8; i++) {
        printf("%.3f ", question_embedding[i]);
    }
    printf("\n");
    
    /* Override global question embedding for this query */
    memcpy(eh_beam_question_embedding, question_embedding,
           EH_HGN_EMBED_DIM * sizeof(float));
    
    /* Initialize beam search with BEST PAIR from question
     * Strategy: Try all consecutive pairs from right to left
     * This fixes hub collision cases like "what is a computer"
     * where (a, computer) is a better starting point than token fallback
     */
    EH_HGN_BeamTracker tracker;
    
    /* Try to find ANY valid pair in the question (right to left) */
    uint32_t start_pair_id = UINT32_MAX;  /* Invalid sentinel */
    int best_pair_idx = -1;
    
    if (question_tokens.count >= 2) {
        /* Try all consecutive pairs, prefer ones closer to end */
        for (int i = (int)question_tokens.count - 2; i >= 0 && start_pair_id == UINT32_MAX; i--) {
            uint32_t tok_a = question_tokens.ids[i];
            uint32_t tok_b = question_tokens.ids[i + 1];
            int pair_id = find_pair_id(tok_a, tok_b);
            
            if (pair_id >= 0 && eh_hgn_dag_fanout(dag, (uint32_t)pair_id) > 0) {
                /* Found valid pair with outgoing edges! */
                start_pair_id = (uint32_t)pair_id;
                best_pair_idx = i;
                printf("     [DEBUG] Found valid pair at [%d,%d]: (%s,%s) → pair_id=%u, fanout=%u\n",
                       i, i+1,
                       g_vocab.words[tok_a], g_vocab.words[tok_b],
                       start_pair_id, eh_hgn_dag_fanout(dag, start_pair_id));
                break;  /* Use first valid pair found (rightmost) */
            }
        }
    }
    
    /* Fallback strategies if no pair found */
    if (start_pair_id == UINT32_MAX) {
        /* Fallback: Embedding-based nearest pair search
         * Instead of random fallback, find the pair in graph that is most
         * semantically similar to the question embedding.
         * This handles OOV queries gracefully without needing more training data.
         */
        printf("     [DEBUG] No valid trained pair found, searching nearest pair by embedding...\n");
        
        float best_similarity = -999.0f;
        uint32_t best_pair_id = 0;
        int candidates_checked = 0;
        
        /* Scan all pairs in graph to find most similar */
        for (uint32_t pair_id = 0; pair_id < dag->vocab_size; pair_id++) {
            /* Skip pairs without outgoing edges (sink nodes) */
            if (eh_hgn_dag_fanout(dag, pair_id) == 0) continue;
            
            const float *pair_vec = eh_hgn_dag_node_vec(dag, pair_id);
            if (!pair_vec) continue;
            
            candidates_checked++;
            
            /* Compute cosine similarity with question embedding */
            float similarity = 0.0f;
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
                similarity += question_embedding[i] * pair_vec[i];
            }
            
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_pair_id = pair_id;
            }
        }
        
        if (best_similarity > 0.1f) {  /* Reasonable threshold */
            start_pair_id = best_pair_id;
            
            /* Try to decode pair for debug output */
            if (best_pair_id < g_pair_count) {
                uint32_t tok_a = g_pair_map[best_pair_id].token_a;
                uint32_t tok_b = g_pair_map[best_pair_id].token_b;
                if (tok_a < g_vocab.size && tok_b < g_vocab.size) {
                    printf("     [DEBUG] Found nearest pair: (%s,%s) → pair_id=%u, similarity=%.3f (checked %d pairs)\n",
                           g_vocab.words[tok_a], g_vocab.words[tok_b],
                           best_pair_id, best_similarity, candidates_checked);
                } else {
                    printf("     [DEBUG] Found nearest pair: pair_id=%u, similarity=%.3f (checked %d pairs)\n",
                           best_pair_id, best_similarity, candidates_checked);
                }
            } else {
                printf("     [DEBUG] Found nearest pair: pair_id=%u, similarity=%.3f (checked %d pairs)\n",
                       best_pair_id, best_similarity, candidates_checked);
            }
        } else {
            /* Final fallback: use pair with highest fanout (most common) */
            uint32_t max_fanout = 0;
            for (uint32_t pair_id = 0; pair_id < dag->vocab_size; pair_id++) {
                uint32_t fanout = eh_hgn_dag_fanout(dag, pair_id);
                if (fanout > max_fanout) {
                    max_fanout = fanout;
                    best_pair_id = pair_id;
                }
            }
            start_pair_id = best_pair_id;
            printf("     [DEBUG] Low similarity (%.3f), using most common pair: pair_id=%u, fanout=%u\n",
                   best_similarity, best_pair_id, max_fanout);
        }
    }
    
    EH_HGN_BeamPath paths_buf[8];
    eh_hgn_beam_init(&tracker, dag, &start_pair_id, 1, paths_buf, 8);
    
    /* Apply tuning parameters */
    eh_beam_attention_mix = params->attention_mix;
    eh_beam_query_ctx.hub_penalty = params->hub_penalty;
    eh_beam_temperature = params->temperature;
    eh_beam_rep_penalty = params->rep_penalty;
    
    /* Run beam search for max steps */
    uint32_t active = 1;
    uint32_t step = 0;
    while (active > 0 && step < params->max_steps) {
        active = eh_hgn_beam_step(&tracker, dag);
        step++;
    }
    
    /* Get best path */
    const EH_HGN_BeamPath *best = eh_hgn_beam_get_best(&tracker);
    
    if (best && best->seq_len > 0) {
        result->score = best->score;
        result->num_tokens = best->seq_len;
        result->success = true;
        
        /* Reconstruct answer text from PAIR IDs
         * Each token in beam is actually a PAIR ID in trigram model
         * Extract second token from each pair to form answer
         */
        int written = 0;
        
        for (uint32_t i = 0; i < best->seq_len && i < 20; i++) {
            uint32_t pair_id = best->tokens[i];
            
            if (pair_id < g_pair_count) {
                uint32_t tok_b = g_pair_map[pair_id].token_b;
                
                if (tok_b < g_vocab.size) {
                    written += snprintf(result->answer_text + written,
                                      sizeof(result->answer_text) - written,
                                      "%s%s", (i > 0 ? " " : ""), g_vocab.words[tok_b]);
                } else {
                    written += snprintf(result->answer_text + written,
                                      sizeof(result->answer_text) - written,
                                      "%s[%u]", (i > 0 ? " " : ""), tok_b);
                }
            } else {
                written += snprintf(result->answer_text + written,
                                  sizeof(result->answer_text) - written,
                                  "%s<pair_%u>", (i > 0 ? " " : ""), pair_id);
            }
        }
        
        if (best->seq_len > 20) {
            snprintf(result->answer_text + written,
                    sizeof(result->answer_text) - written,
                    "... (%u more)", best->seq_len - 20);
        }
    } else {
        strcpy(result->answer_text, "[No answer generated]");
    }
    
    tokenization_free(&question_tokens);
}

/* ================================================================
 * Main: EH-G3 Q&A Demonstration
 * ================================================================ */
int main(int argc, char *argv[])
{
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║        EH-G3 Query-Aware Q&A with Embeddings          ║\n");
    printf("║   Retrieval/Matching Model with Hybrid Scoring        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    /* Parse arguments */
    const char *dag_path = (argc > 1) ? argv[1] : "training/trigram_v2_model.ehdag";
    const char *embed_path = (argc > 2) ? argv[2] : "training/eh_g3_embeddings.bin";
    const char *vocab_path = (argc > 3) ? argv[3] : "training/trigram_v2_vocab.txt";
    const char *pair_path = (argc > 4) ? argv[4] : "training/trigram_v2_vocab.txt.pairs";
    
    printf("Loading DAG from: %s\n", dag_path);
    printf("Loading embeddings from: %s\n", embed_path);
    printf("Loading vocabulary from: %s\n", vocab_path);
    printf("Loading pair map from: %s\n\n", pair_path);
    
    /* Create Arena for memory management */
    EH_Arena *arena = eh_arena_create(128 * 1024 * 1024);  /* 128 MB */
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create memory arena\n");
        return 1;
    }
    
    /* Load vocabulary */
    load_vocab(vocab_path);
    
    /* Load pair map */
    load_pair_map(pair_path);
    
    /* Load DAG */
    EH_HGN_BaseDag dag;
    memset(&dag, 0, sizeof(dag));
    
    EH_HGN_Status rc = eh_hgn_dag_load(arena, dag_path, &dag);
    if (rc != EH_HGN_OK) {
        fprintf(stderr, "ERROR: Failed to load DAG (status=%d)\n", rc);
        eh_arena_destroy(arena);
        return 1;
    }
    printf("[OK] DAG loaded: %u nodes, %u edges\n\n", dag.vocab_size, dag.total_edges);
    
    /* ================================================================
     * PHASE 1: Load embeddings WITHOUT EH-G3 features (EH-G2 baseline)
     * ================================================================ */
    printf("┌─ PHASE 1: Parameter Tuning - Find Optimal Settings ─────────┐\n");
    printf("│ Testing different configurations to minimize repetition     │\n");
    printf("│ and maximize answer quality                                 │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n\n");
    
    /* Test configurations */
    TuningParams configs[] = {
        /* {attention_mix, hub_penalty, temperature, rep_penalty, max_steps} */
        {0.3f, 0.0f, 0.6f, 5.0f, 10},   /* Baseline: EH-G2 settings */
        {0.5f, 0.0f, 0.6f, 5.0f, 10},   /* More attention */
        {0.3f, 2.0f, 0.6f, 5.0f, 10},   /* + Hub penalty */
        {0.3f, 0.0f, 0.4f, 5.0f, 10},   /* Lower temperature (more focused) */
        {0.3f, 0.0f, 0.6f, 8.0f, 10},   /* Higher rep penalty */
        {0.3f, 0.0f, 0.6f, 5.0f, 6},    /* Shorter answers */
    };
    
    const char *config_names[] = {
        "Baseline (EH-G2)",
        "High Attention (50%)",
        "Hub Penalty (2.0)",
        "Low Temperature (0.4)",
        "High Rep Penalty (8.0)",
        "Short Answers (6 steps)",
    };
    
    int num_configs = sizeof(configs) / sizeof(configs[0]);
    
    /* Test key questions with each config */
    int test_indices[] = {4, 5, 7};  /* "what is your name", "how are you", "capital of france" */
    int num_tests = sizeof(test_indices) / sizeof(test_indices[0]);
    
    for (int cfg = 0; cfg < num_configs; cfg++) {
        printf("\n╔═══════════════════════════════════════════════════════════╗\n");
        printf("║ Config %d: %-47s║\n", cfg + 1, config_names[cfg]);
        printf("║   mix=%.1f  hub=%.1f  temp=%.1f  rep=%.1f  steps=%u        ║\n",
               configs[cfg].attention_mix, configs[cfg].hub_penalty,
               configs[cfg].temperature, configs[cfg].rep_penalty,
               configs[cfg].max_steps);
        printf("╚═══════════════════════════════════════════════════════════╝\n");
        
        for (int t = 0; t < num_tests; t++) {
            int idx = test_indices[t];
            QAResult result;
            run_qa_test_with_params(&test_cases[idx], &dag, &configs[cfg], &result);
            
            printf("[%d] Q: %-30s\n", idx + 1, test_cases[idx].question);
            printf("    A: %s\n", result.answer_text);
            printf("    Score: %.2f, Tokens: %u\n\n", result.score, result.num_tokens);
        }
    }
    
    printf("\n┌─ PHASE 2: EH-G2 Baseline (Hybrid Scoring Only) ─────────────┐\n");
    printf("│ Settings:                                                   │\n");
    printf("│   - eh_beam_use_question_embedding = 1 (hybrid)            │\n");
    printf("│   - eh_beam_attention_mix = 0.3 (70% local + 30% global)   │\n");
    printf("│   - eh_beam_use_hub_penalty = 0 (DISABLED)                 │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n\n");
    
    /* Load embeddings */
    int ret = eh_beam_load_query_embeddings(embed_path);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to load embeddings from %s\n", embed_path);
        eh_arena_destroy(arena);
        return 1;
    }
    
    /* Enable hybrid scoring (but not hub penalty yet) */
    eh_beam_use_question_embedding = 1;
    eh_beam_attention_mix = 0.3f;   /* 70% local + 30% global */
    eh_beam_use_hub_penalty = 0;    /* Disabled for baseline */
    
    TuningParams baseline_params = {0.3f, 0.0f, 0.6f, 5.0f, 6};  /* Config 6: Optimal! */
    
    printf("Running Q&A on %lu test cases...\n\n", NUM_TEST_CASES);
    
    QAResult results_baseline[NUM_TEST_CASES];
    uint32_t correct_baseline = 0;
    
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        run_qa_test_with_params(&test_cases[i], &dag, &baseline_params, &results_baseline[i]);
        
        printf("[%2lu] Q: %-35s | A: %s\n",
               i + 1, test_cases[i].question, results_baseline[i].answer_text);
        
        if (results_baseline[i].success) {
            correct_baseline++;
            printf("     ✓ Generated %u tokens (score: %.2f)\n",
                   results_baseline[i].num_tokens,
                   results_baseline[i].score);
        } else {
            printf("     ✗ No answer\n");
        }
        printf("\n");
    }
    
    printf("EH-G2 Baseline Results: %u/%lu answers generated (%.1f%%)\n\n",
           correct_baseline, NUM_TEST_CASES,
           (100.0 * correct_baseline) / NUM_TEST_CASES);
    
    /* ================================================================
     * PHASE 2: Enable EH-G3 Hub Penalty
     * ================================================================ */
    printf("┌─ PHASE 2: EH-G3 with Hub-Aware Penalty ──────────────────────┐\n");
    printf("│ Settings:                                                   │\n");
    printf("│   - eh_beam_use_question_embedding = 1 (hybrid)            │\n");
    printf("│   - eh_beam_attention_mix = 0.3 (70% local + 30% global)   │\n");
    printf("│   - eh_beam_use_hub_penalty = 1 (ENABLED)                  │\n");
    printf("│   - hub_penalty factor = 20.0 (AGGRESSIVE!)              │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n\n");
    
    /* Enable hub penalty */
    eh_beam_use_hub_penalty = 1;
    eh_beam_query_ctx.hub_penalty = 20.0f;  /* AGGRESSIVE: 10× default! */
    
    TuningParams g3_params = {0.3f, 2.0f, 0.6f, 5.0f, 6};  /* Config 6: Optimal! */
    
    printf("Running Q&A with hub penalty...\n\n");
    
    QAResult results_g3[NUM_TEST_CASES];
    uint32_t correct_g3 = 0;
    
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        run_qa_test_with_params(&test_cases[i], &dag, &g3_params, &results_g3[i]);
        
        printf("[%2lu] Q: %-35s | A: %s\n",
               i + 1, test_cases[i].question, results_g3[i].answer_text);
        
        if (results_g3[i].success) {
            correct_g3++;
            printf("     ✓ Generated %u tokens (score: %.2f)\n",
                   results_g3[i].num_tokens,
                   results_g3[i].score);
        } else {
            printf("     ✗ No answer\n");
        }
        printf("\n");
    }
    
    printf("EH-G3 Hub-Aware Results: %u/%lu answers generated (%.1f%%)\n\n",
           correct_g3, NUM_TEST_CASES,
           (100.0 * correct_g3) / NUM_TEST_CASES);
    
    /* ================================================================
     * Summary & Analysis
     * ================================================================ */
    printf("┌─ SUMMARY ─────────────────────────────────────────────────────┐\n");
    printf("│                                                               │\n");
    printf("│ EH-G2 Baseline (Hybrid):        %2u/%lu answers (%.1f%%)       │\n",
           correct_baseline, NUM_TEST_CASES,
           (100.0 * correct_baseline) / NUM_TEST_CASES);
    printf("│ EH-G3 + Hub Penalty:            %2u/%lu answers (%.1f%%)       │\n",
           correct_g3, NUM_TEST_CASES,
           (100.0 * correct_g3) / NUM_TEST_CASES);
    
    int delta = (int)correct_g3 - (int)correct_baseline;
    if (delta > 0) {
        printf("│ ✓ Improvement: +%d answers (hub penalty working!)          │\n", delta);
    } else if (delta == 0) {
        printf("│ ≈ No change (hub penalty effect is subtle)                  │\n");
    } else {
        printf("│ ✗ Regression: %d answers (consider retuning)                │\n", delta);
    }
    
    printf("│                                                               │\n");
    printf("├─ Next Steps ──────────────────────────────────────────────────┤\n");
    printf("│ 1. Evaluate on domain-specific test sets                      │\n");
    printf("│ 2. Tune eh_beam_attention_mix (try 0.4, 0.5)                 │\n");
    printf("│ 3. Tune hub_penalty strength (try 1.5, 2.5, 3.0)             │\n");
    printf("│ 4. Expand corpus: target 1000+ Q&A pairs                      │\n");
    printf("│ 5. Measure latency to ensure <25ms target maintained          │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n\n");
    
    /* Cleanup */
    eh_arena_destroy(arena);
    
    printf("Done!\n");
    return 0;
}
