/*
 * EH-Engine: EventHorizon Heuristic Decoding Engine
 * File: include/eh_tokenizer.h
 * Description: LCRS Trie Tokenizer — encodes text into a token ID stream
 *              using the Greedy Longest-Match algorithm (analogous to BPE decode).
 *
 * LCRS (Left-Child Right-Sibling) structure:
 *   Instead of a fixed child array (high RAM cost), each node has only 2 pointers:
 *   - first_child:  the first child node (deeper in the string)
 *   - next_sibling: the next sibling at the same depth (alternative bytes)
 *   RAM/node = 2 pointers + 1 byte + 1 int ≈ 18 bytes vs. 256 pointers.
 *
 * EH_Arena integration:
 *   All TrieNodes are allocated from EH-Engine's EH_Arena —
 *   no discrete malloc() calls. Deallocation = Arena reset.
 *
 * Design limits:
 *   - Maximum vocabulary: EH_TOK_MAX_VOCAB tokens
 *   - Maximum token string: EH_TOK_MAX_TOKEN_LEN bytes
 *   - LCRS traversal: O(k) where k = number of siblings at each node
 *     (acceptable for vocab < 100k; hash nodes are preferred above that)
 */

#ifndef EH_TOKENIZER_H
#define EH_TOKENIZER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "eh_arena.h"

/* =================================================================
 * CONSTANTS
 * ================================================================= */
#define EH_TOK_MAX_VOCAB       65536  /* Maximum number of tokens in vocabulary  */
#define EH_TOK_MAX_TOKEN_LEN   256    /* Maximum token string length (bytes)     */
#define EH_TOK_BYTE_OFFSET     256    /* Byte-fallback: ID = byte_value + offset */
#define EH_TOK_UNKNOWN_ID      0      /* ID for unknown tokens                   */

/* =================================================================
 * LCRS TRIE NODE
 * Minimal structure — only for use with EH_Arena (no individual free)
 * ================================================================= */
typedef struct EH_TrieNode {
    uint8_t              byte_val;     /* Byte value at this position in the string */
    int32_t              token_id;     /* -1 if this is an intermediate node        */
    struct EH_TrieNode  *first_child;  /* First child (goes deeper)                 */
    struct EH_TrieNode  *next_sibling; /* Next sibling (same depth, next byte)      */
} EH_TrieNode;

/* =================================================================
 * DECODE TABLE: token_id → original string
 * Required so the engine can recover the meaning of tokens post-inference.
 * Stored in a separate Arena (decode_arena) to allow independent resets.
 * ================================================================= */
typedef struct {
    const uint8_t *str;  /* Pointer to the original string (in decode_arena) */
    uint16_t       len;  /* String length (bytes)                             */
} EH_TokenEntry;

/* =================================================================
 * TOKENIZER CONTEXT
 * ================================================================= */
typedef struct {
    EH_TrieNode  *root;          /* Root of the LCRS Trie                            */
    EH_Arena     *trie_arena;    /* Arena holding all TrieNodes                      */
    EH_Arena     *decode_arena;  /* Arena holding reverse strings (decode table)     */
    EH_TokenEntry decode_table[EH_TOK_MAX_VOCAB]; /* ID → string lookup table       */
    int32_t       vocab_size;    /* Number of tokens currently loaded                */
    int32_t       next_id;       /* Auto-incrementing ID for new tokens              */
} EH_Tokenizer;

/* =================================================================
 * PUBLIC API
 * ================================================================= */

/*
 * Create a Tokenizer with two separate Arenas.
 * trie_arena   : holds all TrieNodes (allocated per token insert)
 * decode_arena : holds original string copies (for the decode table)
 * Returns: new tokenizer, or NULL on failure.
 */
EH_Tokenizer *eh_tokenizer_create(EH_Arena *trie_arena,
                                   EH_Arena *decode_arena);

/*
 * Insert a token into the vocabulary.
 * token_str: original UTF-8/byte string of the token
 * token_id : desired ID (-1 = auto-assign an incrementing ID)
 * Returns: the actual assigned ID, or -1 on failure.
 */
int32_t eh_tokenizer_insert(EH_Tokenizer  *tok,
                             const uint8_t *token_str,
                             int32_t        token_id);

/*
 * ENCODE: Convert raw text → token ID array.
 * Algorithm: Greedy Longest-Match over the LCRS Trie.
 *   - At each position i, find the longest matching token.
 *   - If no match: fall back to byte-level ID = byte + EH_TOK_BYTE_OFFSET.
 *
 * text         : input string (UTF-8 or raw bytes)
 * text_len     : text length (-1 to auto-detect via strlen)
 * out_ids      : output buffer allocated by the caller
 * max_tokens   : maximum number of output tokens
 * Returns: actual number of tokens produced.
 */
int eh_tokenizer_encode(const EH_Tokenizer *tok,
                         const uint8_t      *text,
                         int                 text_len,
                         int32_t            *out_ids,
                         int                 max_tokens);

/*
 * DECODE: Convert a token ID array → text string.
 * Writes to out_buf, guarantees null-termination.
 * Returns: number of bytes written (excluding the null terminator).
 */
int eh_tokenizer_decode(const EH_Tokenizer *tok,
                         const int32_t      *ids,
                         int                 id_count,
                         char               *out_buf,
                         int                 out_buf_size);

/*
 * Print vocabulary and Arena usage statistics.
 */
void eh_tokenizer_stats(const EH_Tokenizer *tok);

/*
 * No individual free needed — deallocate by resetting or destroying the Arenas.
 * This function frees only the EH_Tokenizer wrapper struct (not the Arenas).
 */
void eh_tokenizer_free(EH_Tokenizer *tok);

#endif /* EH_TOKENIZER_H */