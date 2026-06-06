/* ================================================================
 * eh_hgn_io.h — HGN I/O Layer: Binary Format Handling
 *
 * Purpose: Separate binary format (EHDAG) from inference logic.
 * Allows future format additions (JSON, protobuf, etc.) without
 * touching core inference code.
 *
 * Replaces: eh_hgn_dag_load() → eh_hgn_io_load()
 * ================================================================ */

#ifndef EH_HGN_IO_H
#define EH_HGN_IO_H

#include "eh_hgn_dag.h"
#include "../../include/core/eh_arena.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * EHDAG Binary Format Constants
 * ---------------------------------------------------------------- */

#define EH_HGN_IO_MAGIC      0x4548474E   /* 'EHGN' */
#define EH_HGN_IO_VERSION    2            /* Format version */

/* ----------------------------------------------------------------
 * File Format Header (32 bytes, matches EH_HGN_DagFileHeader)
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t magic;         /* Magic number: 0x48474E44 */
    uint32_t version;       /* Format version: 1 */
    uint32_t vocab_size;    /* Number of tokens */
    uint32_t total_edges;   /* Total edge count */
    uint32_t embed_dim;     /* Embedding dimension (must be 128) */
    uint32_t max_fanout;    /* Maximum edges per node */
    uint32_t reserved[2];   /* Padding to 32 bytes */
} __attribute__((packed)) EH_HGN_IO_Header;

_Static_assert(sizeof(EH_HGN_IO_Header) == 32, "Header must be 32 bytes");

/* ----------------------------------------------------------------
 * I/O Status Codes
 * ---------------------------------------------------------------- */
typedef enum {
    EH_HGN_IO_OK           = 0,   /* Success */
    EH_HGN_IO_ERR_FILE     = 1,   /* File I/O error */
    EH_HGN_IO_ERR_MAGIC    = 2,   /* Invalid magic number */
    EH_HGN_IO_ERR_VERSION  = 3,   /* Unsupported version */
    EH_HGN_IO_ERR_SIZE     = 4,   /* Size exceeds limits */
    EH_HGN_IO_ERR_ARENA    = 5,   /* Arena out of memory */
    EH_HGN_IO_ERR_CORRUPT  = 6,   /* Data corruption */
    EH_HGN_IO_ERR_ALIGN    = 7,   /* Alignment error */
} EH_HGN_IO_Status;

/* ----------------------------------------------------------------
 * API: Load EHDAG Binary
 * ---------------------------------------------------------------- */

/* Load EHDAG file into BaseDag structure.
 * 
 * Zero-copy loading: file data is read directly into arena.
 * All pointers in BaseDag reference arena memory.
 * 
 * Parameters:
 *   arena: Arena allocator (must be large enough for file)
 *   path:  File path to EHDAG binary
 *   dag:   Output BaseDag structure (populated on success)
 * 
 * Returns:
 *   EH_HGN_IO_OK on success
 *   Error code on failure
 * 
 * Validates:
 *   - Magic number (0x48474E44)
 *   - Version compatibility
 *   - Size limits (vocab_size, total_edges)
 *   - Alignment (32-byte for embeddings/weights)
 *   - CSR integrity
 */
EH_HGN_IO_Status eh_hgn_io_load(EH_Arena           *arena,
                                 const char         *path,
                                 EH_HGN_BaseDag     *dag);

/* ----------------------------------------------------------------
 * API: Save BaseDag to EHDAG Binary
 * ---------------------------------------------------------------- */

/* Save BaseDag structure to EHDAG file.
 * 
 * Writes complete binary format with proper alignment.
 * Can be loaded back with eh_hgn_io_load().
 * 
 * Parameters:
 *   dag:   BaseDag structure to save
 *   path:  Output file path
 * 
 * Returns:
 *   EH_HGN_IO_OK on success
 *   Error code on failure
 * 
 * Format:
 *   [Header: 32 bytes]
 *   [Node Embeddings: aligned 32]
 *   [Node Adjacency]
 *   [Edge Compact]
 *   [Padding to 32-byte]
 *   [Edge Weights: aligned 32]
 */
EH_HGN_IO_Status eh_hgn_io_save(const EH_HGN_BaseDag *dag,
                                 const char           *path);

/* ----------------------------------------------------------------
 * API: Validate EHDAG File
 * ---------------------------------------------------------------- */

/* Validate EHDAG file without loading into memory.
 * 
 * Checks:
 *   - Magic number
 *   - Version
 *   - File size matches header
 *   - Alignment requirements
 * 
 * Returns:
 *   EH_HGN_IO_OK if file is valid
 *   Error code if validation fails
 */
EH_HGN_IO_Status eh_hgn_io_validate(const char *path);

/* ----------------------------------------------------------------
 * API: File Info
 * ---------------------------------------------------------------- */

/* Read EHDAG header without loading full file.
 * 
 * Useful for checking vocab_size, total_edges before allocation.
 * 
 * Returns:
 *   EH_HGN_IO_OK on success, header populated
 *   Error code on failure
 */
EH_HGN_IO_Status eh_hgn_io_read_header(const char        *path,
                                        EH_HGN_IO_Header  *header);

/* ----------------------------------------------------------------
 * Utilities: Error Messages
 * ---------------------------------------------------------------- */

/* Get human-readable error message for status code. */
const char *eh_hgn_io_strerror(EH_HGN_IO_Status status);

#ifdef __cplusplus
}
#endif

#endif /* EH_HGN_IO_H */
