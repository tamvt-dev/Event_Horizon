/* ================================================================
 * eh_adaptive.c — HGN Adaptive Mechanisms Implementation
 * ================================================================ */

#include "../../../include/hgn/legacy/eh_adaptive.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * Cosine Similarity: cos(a, b) = dot(a,b) / (||a|| × ||b||)
 * ---------------------------------------------------------------- */
float eh_adaptive_cosine_similarity(const float *a, const float *b, uint32_t dim)
{
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);
    
    if (norm_a < 1e-8f || norm_b < 1e-8f) return 0.0f;
    
    return dot / (norm_a * norm_b);
}

/* ----------------------------------------------------------------
 * Beam Entropy: H = -Σ p_i log(p_i)
 * Normalize scores to probabilities first via softmax
 * ---------------------------------------------------------------- */
float eh_adaptive_beam_entropy(const EH_HGN_BeamTracker *tracker)
{
    if (tracker->active_paths == 0) return 0.0f;
    
    /* Find max score for numerical stability */
    float max_score = tracker->paths[0].score;
    for (uint32_t i = 1; i < tracker->active_paths; i++) {
        if (tracker->paths[i].score > max_score) {
            max_score = tracker->paths[i].score;
        }
    }
    
    /* Compute softmax probabilities */
    float probs[EH_BEAM_WIDTH];
    float sum = 0.0f;
    
    for (uint32_t i = 0; i < tracker->active_paths; i++) {
        probs[i] = expf(tracker->paths[i].score - max_score);
        sum += probs[i];
    }
    
    if (sum < 1e-8f) return 0.0f;
    
    /* Normalize and compute entropy */
    float entropy = 0.0f;
    for (uint32_t i = 0; i < tracker->active_paths; i++) {
        probs[i] /= sum;
        if (probs[i] > 1e-8f) {
            entropy -= probs[i] * logf(probs[i]);
        }
    }
    
    return entropy;
}

/* ----------------------------------------------------------------
 * Linear Congruential Generator (LCG)
 * Constants from Numerical Recipes
 * ---------------------------------------------------------------- */
uint32_t eh_adaptive_lcg_next(uint32_t seed)
{
    return (1664525u * seed + 1013904223u);
}

/* ----------------------------------------------------------------
 * Generate Perturbation Vector
 * Uses LCG for deterministic randomness in range [-scale, +scale]
 * ---------------------------------------------------------------- */
void eh_adaptive_generate_perturbation(uint32_t seed,
                                        float scale,
                                        float *out_vec,
                                        uint32_t dim)
{
    uint32_t rng_state = seed;
    
    for (uint32_t i = 0; i < dim; i++) {
        rng_state = eh_adaptive_lcg_next(rng_state);
        
        /* Map uint32 [0, 2^32-1] → float [-1, +1] */
        float normalized = (float)rng_state / (float)UINT32_MAX;
        normalized = normalized * 2.0f - 1.0f;  /* [-1, +1] */
        
        out_vec[i] = normalized * scale;
    }
}

/* ----------------------------------------------------------------
 * Initialize Adaptive Context
 * ---------------------------------------------------------------- */
void eh_adaptive_init(EH_AdaptiveContext *ctx,
                      float collapse_threshold,
                      float entropy_threshold,
                      float mutant_scale)
{
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->state = EH_STATE_EXPAND;
    ctx->collapse_threshold = collapse_threshold;
    ctx->entropy_threshold = entropy_threshold;
    ctx->mutant_scale = mutant_scale;
}

/* ----------------------------------------------------------------
 * Collapse Gating Decision
 * ---------------------------------------------------------------- */
EH_CollapseState eh_adaptive_gate(EH_AdaptiveContext *ctx,
                                   const float        *h_current)
{
    /* First step: always EXPAND, save h_current as h_prev */
    if (ctx->step_counter == 0) {
        memcpy(ctx->h_prev, h_current, sizeof(ctx->h_prev));
        ctx->state = EH_STATE_EXPAND;
        ctx->expand_count++;
        ctx->step_counter++;
        return EH_STATE_EXPAND;
    }
    
    /* Compute cosine similarity */
    float cos_sim = eh_adaptive_cosine_similarity(
        h_current, ctx->h_prev, EH_HGN_EMBED_DIM);
    
    /* Gating decision */
    if (cos_sim > ctx->collapse_threshold) {
        ctx->state = EH_STATE_COLLAPSE;
        ctx->collapse_count++;
    } else {
        ctx->state = EH_STATE_EXPAND;
        ctx->expand_count++;
    }
    
    /* Update h_prev */
    memcpy(ctx->h_prev, h_current, sizeof(ctx->h_prev));
    ctx->step_counter++;
    
    return ctx->state;
}

/* ----------------------------------------------------------------
 * Try Spawn Mutant
 * ---------------------------------------------------------------- */
uint32_t eh_adaptive_try_spawn_mutant(EH_AdaptiveContext     *ctx,
                                      const EH_HGN_BeamTracker *tracker,
                                      const float              *h_current)
{
    /* Check if mutant pool is full */
    if (ctx->mutant_count >= EH_MAX_MUTANTS) {
        return UINT32_MAX;
    }
    
    /* Compute beam entropy */
    float entropy = eh_adaptive_beam_entropy(tracker);
    
    /* Check threshold */
    if (entropy <= ctx->entropy_threshold) {
        return UINT32_MAX;
    }
    
    /* Find free slot */
    uint32_t slot = 0;
    for (slot = 0; slot < EH_MAX_MUTANTS; slot++) {
        if (!ctx->mutants[slot].active) break;
    }
    
    if (slot >= EH_MAX_MUTANTS) return UINT32_MAX;
    
    /* Generate deterministic seed */
    uint32_t top_score_quantized = (uint32_t)(tracker->paths[0].score * 1000.0f);
    uint32_t seed = top_score_quantized ^ ctx->step_counter;
    
    /* Generate perturbation */
    float perturbation[EH_HGN_EMBED_DIM];
    eh_adaptive_generate_perturbation(seed, ctx->mutant_scale, 
                                       perturbation, EH_HGN_EMBED_DIM);
    
    /* Create mutant: h_{t-1} + perturbation */
    EH_MutantNode *mutant = &ctx->mutants[slot];
    for (uint32_t i = 0; i < EH_HGN_EMBED_DIM; i++) {
        mutant->vec[i] = h_current[i] + perturbation[i];
    }
    
    /* Assign virtual token_id >= vocab_size */
    mutant->token_id = 1000000u + ctx->total_mutants_spawned;
    mutant->step_counter = ctx->step_counter;
    mutant->active = true;
    
    ctx->mutant_count++;
    ctx->total_mutants_spawned++;
    
    return mutant->token_id;
}

/* ----------------------------------------------------------------
 * Get Mutant Vector
 * ---------------------------------------------------------------- */
const float *eh_adaptive_get_mutant_vec(const EH_AdaptiveContext *ctx,
                                         uint32_t token_id)
{
    for (uint32_t i = 0; i < EH_MAX_MUTANTS; i++) {
        if (ctx->mutants[i].active && ctx->mutants[i].token_id == token_id) {
            return ctx->mutants[i].vec;
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------
 * Reset Mutants (arena context reset)
 * ---------------------------------------------------------------- */
void eh_adaptive_reset_mutants(EH_AdaptiveContext *ctx)
{
    for (uint32_t i = 0; i < EH_MAX_MUTANTS; i++) {
        ctx->mutants[i].active = false;
    }
    ctx->mutant_count = 0;
}

/* ----------------------------------------------------------------
 * Dump Statistics
 * ---------------------------------------------------------------- */
void eh_adaptive_dump_stats(const EH_AdaptiveContext *ctx)
{
    fprintf(stderr, "\n=== EH_Adaptive Statistics ===\n");
    fprintf(stderr, "  Steps total      : %u\n", ctx->step_counter);
    fprintf(stderr, "  Collapse count   : %u (%.1f%%)\n", 
            ctx->collapse_count,
            ctx->step_counter ? 100.0f * ctx->collapse_count / ctx->step_counter : 0.0f);
    fprintf(stderr, "  Expand count     : %u (%.1f%%)\n",
            ctx->expand_count,
            ctx->step_counter ? 100.0f * ctx->expand_count / ctx->step_counter : 0.0f);
    fprintf(stderr, "  Mutants spawned  : %u\n", ctx->total_mutants_spawned);
    fprintf(stderr, "  Mutants active   : %u / %u\n", 
            ctx->mutant_count, EH_MAX_MUTANTS);
    fprintf(stderr, "==============================\n\n");
}
