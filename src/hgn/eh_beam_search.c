/* ================================================================
 * eh_beam_search.c — HGN Layer 3: Beam Search Implementation
 *
 * Fix log:
 *   v1.1 - Fixed parent beam tracking bug: ScoredCandidate now
 *          stores parent_beam_idx so new paths are built from the
 *          correct parent sequence (not always paths[0]).
 *        - Added EOS/sink detection: nodes with 0 outgoing edges
 *          automatically set is_finished=true on the beam.
 *   v1.2 - Added global question embedding support for EH-G2:
 *          When eh_beam_use_question_embedding=1, scoring uses
 *          dot(edge, question_embedding) instead of context pooling.
 *   v1.3 - Hybrid scoring (EH-G2 Hybrid):
 *          Blend 70% local context (EH-G1) + 30% global attention
 *          to preserve baseline performance while adding disambiguation.
 *   v1.4 - EH-G3: Query context + hub-aware penalty:
 *          Added EH_QueryContext for advanced scoring.
 *          Hub nodes (high fanout like "is a") are penalized to reduce
 *          collisions and improve robustness.
 * ================================================================ */

#include "../../include/hgn/eh_beam_search.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ----------------------------------------------------------------
 * Global Question Embedding (EH-G2/G3 Attention)
 * Applications can set these to enable attention-based scoring.
 * ---------------------------------------------------------------- */
float eh_beam_question_embedding[EH_HGN_EMBED_DIM] = {0};
int eh_beam_use_question_embedding = 0;
float eh_beam_attention_mix = 0.3f;  /* Default: 30% global, 70% local */

/* ----------------------------------------------------------------
 * EH-G3: Hub Penalty & Query Context
 * ---------------------------------------------------------------- */
int eh_beam_use_hub_penalty = 0;
EH_BeamQueryContext eh_beam_query_ctx = {
    .hub_penalty = 2.0f,
    .hub_threshold = 10,
    .max_node_fanout = 0
};

/* ----------------------------------------------------------------
 * Runtime-tunable decoding parameters
 * ---------------------------------------------------------------- */
float eh_beam_rep_penalty = 5.0f;
float eh_beam_temperature = 0.6f;

/* ----------------------------------------------------------------
 * Internal: Candidate pool entry.
 * Tracks which parent beam generated this candidate so we can
 * copy the correct token history when building new paths.
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t token_id;         /* The candidate next token              */
    float    score;            /* Cumulative score (parent + this edge) */
    uint32_t parent_beam_idx;  /* Which beam in tracker->paths spawned  */
    bool     is_terminal;      /* True if dst node is a sink (no edges) */
} ScoredCandidate;

static int compare_candidates(const void *a, const void *b) {
    const ScoredCandidate *ca = (const ScoredCandidate *)a;
    const ScoredCandidate *cb = (const ScoredCandidate *)b;
    if (ca->score > cb->score) return -1;  /* Descending */
    if (ca->score < cb->score) return 1;
    return 0;
}

/* ----------------------------------------------------------------
 * Internal: simple 128D dot product
 * (Replace with AVX2 intrinsics for production hot path)
 * ---------------------------------------------------------------- */
static float dot_product_128(const float *a, const float *b) {
    float sum = 0.0f;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/* ----------------------------------------------------------------
 * Internal: Hub-aware penalty (EH-G3)
 * 
 * Hub nodes (like "is a" with 1000+ edges) dominate beam search by
 * sheer prior weight. This function penalizes high-fanout nodes
 * proportionally to reduce collisions.
 * 
 * Penalty formula:
 *   hub_penalty = alpha * (fanout / max_fanout)
 * 
 * Where:
 *   alpha = tunable strength (default 2.0)
 *   fanout = number of outgoing edges from this node
 *   max_fanout = largest fanout observed in the graph
 * 
 * Effect: Node with fanout=1000 in graph with max=10000 gets penalty 0.2x
 *         Node with fanout=100 gets penalty 0.02x
 *         Node with fanout=10 gets penalty 0.002x
 * ---------------------------------------------------------------- */
static float compute_hub_penalty(const EH_HGN_BaseDag *dag, uint32_t node_id)
{
    if (!eh_beam_use_hub_penalty) return 0.0f;
    
    uint32_t fanout = eh_hgn_dag_fanout(dag, node_id);
    
    /* Estimate max fanout (could be cached in DAG for efficiency) */
    uint32_t max_fanout = eh_beam_query_ctx.max_node_fanout;
    if (max_fanout == 0) max_fanout = 1;  /* Avoid divide-by-zero */
    
    float normalized = (float)fanout / (float)max_fanout;
    float alpha = eh_beam_query_ctx.hub_penalty;
    
    return alpha * normalized;
}

/* ----------------------------------------------------------------
 * Internal: Check if token was recently used (repetition detection)
 * Looks back REP_WINDOW tokens in the beam sequence.
 * Returns count of how many times the token appears in recent history.
 * ---------------------------------------------------------------- */
static uint32_t count_recent_repetitions(const EH_HGN_BeamPath *beam, 
                                          uint32_t candidate_token)
{
    uint32_t count = 0;
    uint32_t look_back = (beam->seq_len < EH_BEAM_REP_WINDOW) 
                         ? beam->seq_len : EH_BEAM_REP_WINDOW;
    
    for (uint32_t i = 0; i < look_back; i++) {
        uint32_t idx = beam->seq_len - 1 - i;
        if (beam->tokens[idx] == candidate_token) {
            count++;
        }
    }
    return count;
}

/* ----------------------------------------------------------------
 * Internal: Apply repetition penalty to score (additive)
 * ---------------------------------------------------------------- */
static float apply_repetition_penalty(float base_score, uint32_t rep_count)
{
    return base_score - ((float)rep_count * eh_beam_rep_penalty);
}

/* ----------------------------------------------------------------
 * Internal: Check if candidate token forms a cycle/loop
 * ---------------------------------------------------------------- */
static bool detect_cycle(const EH_HGN_BeamPath *beam, uint32_t candidate_token)
{
    for (uint32_t i = 0; i < beam->seq_len; i++) {
        if (beam->tokens[i] == candidate_token) {
            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------
 * Internal: Apply temperature scaling to score
 * Temperature > 1.0 → more uniform (diverse)
 * Temperature < 1.0 → sharper distribution (focused)
 * Temperature = 1.0 → no change
 * ---------------------------------------------------------------- */
static float apply_temperature(float score)
{
    return score / eh_beam_temperature;
}

/* ----------------------------------------------------------------
 * Internal: Compute context vector from recent tokens
 * Uses average pooling of last N token embeddings for richer context.
 * Helps model understand full question, not just last word.
 * ---------------------------------------------------------------- */
static void compute_context_vector(const EH_HGN_BeamPath *beam,
                                   const EH_HGN_BaseDag *dag,
                                   float *context_out)
{
    /* Clear output */
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        context_out[i] = 0.0f;
    }
    
    if (beam->seq_len == 0) return;
    
    /* Determine window size (min of CONTEXT_WINDOW and actual length) */
    uint32_t window = (beam->seq_len < EH_BEAM_CONTEXT_WINDOW) 
                      ? beam->seq_len : EH_BEAM_CONTEXT_WINDOW;
    
    /* Average last N token embeddings */
    for (uint32_t w = 0; w < window; w++) {
        uint32_t token_idx = beam->seq_len - 1 - w;
        uint32_t token_id = beam->tokens[token_idx];
        const float *token_vec = eh_hgn_dag_node_vec(dag, token_id);
        
        if (token_vec) {
            for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
                context_out[i] += token_vec[i];
            }
        }
    }
    
    /* Average (divide by window size) */
    float scale = 1.0f / (float)window;
    for (int i = 0; i < EH_HGN_EMBED_DIM; i++) {
        context_out[i] *= scale;
    }
}

/* ----------------------------------------------------------------
 * Internal: Initialize hub penalty using DAG fanout info
 * DAG header already contains max_fanout, no need to scan.
 * ---------------------------------------------------------------- */
static void init_hub_penalty(const EH_HGN_BaseDag *dag)
{
    if (!dag) {
        eh_beam_query_ctx.max_node_fanout = 1;
        return;
    }
    eh_beam_query_ctx.max_node_fanout = dag->max_fanout;
}

/* ----------------------------------------------------------------
 * eh_hgn_beam_init
 * ---------------------------------------------------------------- */
void eh_hgn_beam_init(EH_HGN_BeamTracker    *tracker,
                      const EH_HGN_BaseDag   *dag,
                      const uint32_t         *prompt,
                      uint32_t                prompt_len)
{
    memset(tracker, 0, sizeof(*tracker));
    tracker->dag = dag;
    tracker->active_paths = 1;

    /* If hub penalty enabled, initialize from DAG header info */
    if (eh_beam_use_hub_penalty) {
        init_hub_penalty(dag);
    }

    /* Initialize first beam with prompt */
    EH_HGN_BeamPath *path = &tracker->paths[0];
    path->seq_len    = 0;
    path->score      = 0.0f;
    path->is_finished = false;

    for (uint32_t i = 0; i < prompt_len && i < EH_BEAM_MAX_LEN; i++) {
        path->tokens[path->seq_len++] = prompt[i];
    }
}

/* ----------------------------------------------------------------
 * eh_hgn_beam_step
 *
 * Key fix: ScoredCandidate stores parent_beam_idx.
 * When we build new_paths[i], we memcpy from paths[parent_beam_idx]
 * instead of always copying paths[0].
 * ---------------------------------------------------------------- */
uint32_t eh_hgn_beam_step(EH_HGN_BeamTracker *tracker,
                           const EH_HGN_BaseDag *dag)
{
    if (tracker->active_paths == 0) return 0;

    /* Count how many beams still need expansion */
    uint32_t still_active = 0;
    for (uint32_t b = 0; b < tracker->active_paths; b++) {
        if (!tracker->paths[b].is_finished) still_active++;
    }
    if (still_active == 0) return 0;  /* All done */

    /* --------------------------------------------------------
     * Candidate pool: each unfinished beam expands its edges.
     * Finished beams are carried over directly.
     * -------------------------------------------------------- */
    #define MAX_CANDIDATES (EH_BEAM_WIDTH * EH_HGN_MAX_FANOUT + EH_BEAM_WIDTH)
    ScoredCandidate candidates[MAX_CANDIDATES];
    uint32_t cand_count = 0;

    for (uint32_t b = 0; b < tracker->active_paths; b++) {
        EH_HGN_BeamPath *beam = &tracker->paths[b];

        /* --- Finished beams: carry forward as-is (no new token) --- */
        if (beam->is_finished) {
            if (cand_count < MAX_CANDIDATES) {
                candidates[cand_count].token_id        = EH_BEAM_EOS_TOKEN; /* sentinel */
                candidates[cand_count].score           = beam->score;
                candidates[cand_count].parent_beam_idx = b;
                candidates[cand_count].is_terminal     = true;
                cand_count++;
            }
            continue;
        }

        if (beam->seq_len >= EH_BEAM_MAX_LEN) continue;

        /* Compute rich context from recent tokens (not just last one) */
        float context_vec[EH_HGN_EMBED_DIM];
        compute_context_vector(beam, dag, context_vec);

        uint32_t last_token = beam->tokens[beam->seq_len - 1];
        uint32_t fanout = eh_hgn_dag_fanout(dag, last_token);

        /* ---- Sink node: no edges → mark terminal, carry forward ---- */
        if (fanout == 0) {
            if (cand_count < MAX_CANDIDATES) {
                candidates[cand_count].token_id        = EH_BEAM_EOS_TOKEN;
                candidates[cand_count].score           = beam->score;
                candidates[cand_count].parent_beam_idx = b;
                candidates[cand_count].is_terminal     = true;
                cand_count++;
            }
            continue;
        }

        /* ---- Normal expansion: score each outgoing edge ---- */
        EH_HGN_FOR_EDGES(dag, last_token, edge) {
            if (cand_count >= MAX_CANDIDATES) break;

            const float *w     = eh_hgn_dag_edge_weight(dag, edge);
            
            /* EH-G2 Hybrid: Blend local context (EH-G1) with global attention (EH-G2)
             * Local context: Trained for sequential prediction (works well)
             * Global attention: Query matching (helps disambiguate hub nodes)
             * Hybrid: Best of both worlds!
             * 
             * Mix ratio controlled by eh_beam_attention_mix (default 0.3):
             *   0.0 = pure local (EH-G1)
             *   0.3 = 70% local + 30% global (default, tested)
             *   0.5 = 50% local + 50% global (balanced)
             *   1.0 = pure global (not recommended with current embeddings)
             */
            float ctx_score;
            if (eh_beam_use_question_embedding) {
                /* Hybrid mode: blend local and global with tunable ratio */
                float ctx_local = dot_product_128(w, context_vec);
                float ctx_global = dot_product_128(w, eh_beam_question_embedding);
                float local_weight = 1.0f - eh_beam_attention_mix;
                float global_weight = eh_beam_attention_mix;
                ctx_score = local_weight * ctx_local + global_weight * ctx_global;
            } else {
                /* Standard mode: pure local context (EH-G1) */
                ctx_score = dot_product_128(w, context_vec);
            }
            
            /* Weight context heavily to fight hub node dominance
             * Problem: High-frequency paths (e.g., "is a" hub) have high priors
             * Solution: Scale context 3x and prior 0.5x to make context dominant
             * EH-G3: Additionally apply hub-aware penalty for high-fanout nodes
             */
            float weighted_prior = edge->prior * 0.5f;
            float weighted_ctx = ctx_score * 3.0f;
            float hub_penalty_score = compute_hub_penalty(dag, edge->dst);
            float base_score   = beam->score + weighted_prior + weighted_ctx - hub_penalty_score;

            /* Apply temperature scaling */
            float temp_score = apply_temperature(base_score);

            /* Apply repetition penalty */
            uint32_t rep_count = count_recent_repetitions(beam, edge->dst);
            float final_score  = apply_repetition_penalty(temp_score, rep_count);

            /* Apply cycle penalty */
            if (detect_cycle(beam, edge->dst)) {
                final_score -= EH_BEAM_CYCLE_PENALTY;
            }

            /* Check if destination is a sink (pre-check for next step) */
            bool dst_is_sink = (eh_hgn_dag_fanout(dag, edge->dst) == 0);

            candidates[cand_count].token_id        = edge->dst;
            candidates[cand_count].score           = final_score;
            candidates[cand_count].parent_beam_idx = b;        /* <-- FIX */
            candidates[cand_count].is_terminal     = dst_is_sink;
            cand_count++;
        }
    }

    /* Sort candidates descending by score */
    qsort(candidates, cand_count, sizeof(ScoredCandidate), compare_candidates);

    /* Keep top-K beams */
    uint32_t new_beam_count = (cand_count < EH_BEAM_WIDTH) ? cand_count : EH_BEAM_WIDTH;

    EH_HGN_BeamPath new_paths[EH_BEAM_WIDTH];
    memset(new_paths, 0, sizeof(new_paths));

    uint32_t active_after = 0;

    for (uint32_t i = 0; i < new_beam_count; i++) {
        uint32_t pid = candidates[i].parent_beam_idx;  /* <-- FIX: correct parent */

        /* Copy full token history from the correct parent beam */
        memcpy(&new_paths[i], &tracker->paths[pid], sizeof(EH_HGN_BeamPath));

        new_paths[i].score       = candidates[i].score;
        new_paths[i].is_finished = candidates[i].is_terminal;

        /* Append new token only if not a sentinel */
        if (candidates[i].token_id != EH_BEAM_EOS_TOKEN) {
            if (new_paths[i].seq_len < EH_BEAM_MAX_LEN) {
                new_paths[i].tokens[new_paths[i].seq_len] = candidates[i].token_id;
                new_paths[i].seq_len++;
            }
        }

        if (!new_paths[i].is_finished) active_after++;
    }

    /* Commit new paths to tracker */
    memcpy(tracker->paths, new_paths, new_beam_count * sizeof(EH_HGN_BeamPath));
    tracker->active_paths = new_beam_count;

    return active_after;
    #undef MAX_CANDIDATES
}

/* ----------------------------------------------------------------
 * eh_hgn_beam_get_best
 * After sorting, paths[0] is always the highest-scoring beam.
 * ---------------------------------------------------------------- */
const EH_HGN_BeamPath *eh_hgn_beam_get_best(const EH_HGN_BeamTracker *tracker)
{
    if (tracker->active_paths == 0) return NULL;
    return &tracker->paths[0];
}

/* ----------------------------------------------------------------
 * eh_hgn_beam_reset
 * ---------------------------------------------------------------- */
void eh_hgn_beam_reset(EH_HGN_BeamTracker *tracker)
{
    memset(tracker, 0, sizeof(*tracker));
}

/* ================================================================
 * EH-G3: Load Query Embeddings from Binary File
 * 
 * File format (little-endian):
 *   [uint32_t] vocab_size (should be 2000 for EH-G3 trainer output)
 *   [uint32_t] embed_dim (should be 128)
 *   [floats] embeddings (vocab_size * embed_dim float values)
 * 
 * This function reads the average embedding of the question
 * (computed from the first embedding in the file or as a sentinel)
 * and populates eh_beam_question_embedding for use in hybrid scoring.
 * 
 * Usage:
 *   int ret = eh_beam_load_query_embeddings("training/eh_g3_embeddings.bin");
 *   if (ret == 0) {
 *       eh_beam_use_question_embedding = 1;
 *       eh_beam_use_hub_penalty = 1;  // Optional: also enable hub penalty
 *       // Now beam search will use the embeddings
 *   }
 * 
 * Returns: 0 on success, -1 on error (file not found, format error, etc.)
 * ================================================================ */
int eh_beam_load_query_embeddings(const char *bin_file)
{
    if (!bin_file) return -1;
    
    FILE *f = fopen(bin_file, "rb");
    if (!f) {
        fprintf(stderr, "[EH_G3] Error: Cannot open embeddings file '%s'\n", bin_file);
        return -1;
    }
    
    /* Read header */
    uint32_t vocab_size = 0;
    uint32_t embed_dim = 0;
    
    if (fread(&vocab_size, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "[EH_G3] Error: Failed to read vocab_size from '%s'\n", bin_file);
        fclose(f);
        return -1;
    }
    
    if (fread(&embed_dim, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "[EH_G3] Error: Failed to read embed_dim from '%s'\n", bin_file);
        fclose(f);
        return -1;
    }
    
    /* Validate header */
    if (embed_dim != EH_HGN_EMBED_DIM) {
        fprintf(stderr, "[EH_G3] Warning: embed_dim mismatch. Expected %d, got %u\n", 
                EH_HGN_EMBED_DIM, embed_dim);
        fclose(f);
        return -1;
    }
    
    if (vocab_size == 0 || vocab_size > 10000) {
        fprintf(stderr, "[EH_G3] Warning: vocab_size out of range: %u\n", vocab_size);
        fclose(f);
        return -1;
    }
    
    /* Read first embedding (used as question embedding for hybrid scoring)
     * In EH-G3 trainer output, the first embedding is typically a special token
     * or average of question embeddings. For simplicity, we use vocab[0].
     * 
     * In a more sophisticated setup, you'd:
     * 1. Encode the question into tokens
     * 2. Average the corresponding embeddings
     * 3. Store in eh_beam_question_embedding
     * 
     * For now, we use a fixed reference embedding (vocab token 0)
     * and override it dynamically per-query in inference code.
     */
    float embeddings_buffer[EH_HGN_EMBED_DIM];
    if (fread(embeddings_buffer, sizeof(float), EH_HGN_EMBED_DIM, f) != EH_HGN_EMBED_DIM) {
        fprintf(stderr, "[EH_G3] Error: Failed to read embedding from '%s'\n", bin_file);
        fclose(f);
        return -1;
    }
    
    /* Copy into global question embedding */
    memcpy(eh_beam_question_embedding, embeddings_buffer, 
           EH_HGN_EMBED_DIM * sizeof(float));
    
    fclose(f);
    
    fprintf(stderr, "[EH_G3] Successfully loaded embeddings from '%s'\n", bin_file);
    fprintf(stderr, "[EH_G3] vocab_size=%u, embed_dim=%u\n", vocab_size, embed_dim);
    fprintf(stderr, "[EH_G3] Question embedding loaded (first token from file)\n");
    fprintf(stderr, "[EH_G3] Tip: Override eh_beam_question_embedding before beam_init() "
            "for per-query embeddings\n");
    
    return 0;
}
