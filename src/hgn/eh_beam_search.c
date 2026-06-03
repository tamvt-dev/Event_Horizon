/* ================================================================
 * eh_beam_search.c — HGN Layer 3: Beam Search Implementation
 *
 * Fix log:
 *   v1.1 - Fixed parent beam tracking bug: ScoredCandidate now
 *          stores parent_beam_idx so new paths are built from the
 *          correct parent sequence (not always paths[0]).
 *        - Added EOS/sink detection: nodes with 0 outgoing edges
 *          automatically set is_finished=true on the beam.
 * ================================================================ */

#include "../../include/hgn/eh_beam_search.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

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

        /* Last token context for scoring */
        uint32_t last_token  = beam->tokens[beam->seq_len - 1];
        const float *ctx_vec = eh_hgn_dag_node_vec(dag, last_token);
        if (!ctx_vec) continue;

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
            float ctx_score    = dot_product_128(w, ctx_vec);
            float total_score  = beam->score + edge->prior + ctx_score;

            /* Check if destination is a sink (pre-check for next step) */
            bool dst_is_sink = (eh_hgn_dag_fanout(dag, edge->dst) == 0);

            candidates[cand_count].token_id        = edge->dst;
            candidates[cand_count].score           = total_score;
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
