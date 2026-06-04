/* ================================================================
 * eh_hgn_io.c — HGN I/O Layer implementation
 *
 * Refactored from eh_hgn_dag.c to separate binary format handling.
 * ================================================================ */

#define _POSIX_C_SOURCE 200112L

#include "../../include/hgn/eh_hgn_io.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ----------------------------------------------------------------
 * eh_hgn_io_load
 * ---------------------------------------------------------------- */
EH_HGN_IO_Status eh_hgn_io_load(EH_Arena       *arena,
                                 const char     *path,
                                 EH_HGN_BaseDag *dag)
{
    if (!arena || !path || !dag) return EH_HGN_IO_ERR_FILE;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[eh_hgn_io] Cannot open '%s': %s\n",
                path, strerror(errno));
        return EH_HGN_IO_ERR_FILE;
    }

    /* Read and validate header */
    EH_HGN_IO_Header hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] Header read failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    /* Validate magic */
    if (hdr.magic != EH_HGN_IO_MAGIC) {
        fprintf(stderr, "[eh_hgn_io] Bad magic: 0x%08X (expected 0x%08X)\n",
                hdr.magic, EH_HGN_IO_MAGIC);
        fclose(fp);
        return EH_HGN_IO_ERR_MAGIC;
    }

    /* Validate version */
    if (hdr.version != EH_HGN_IO_VERSION) {
        fprintf(stderr, "[eh_hgn_io] Unsupported version: %u\n", hdr.version);
        fclose(fp);
        return EH_HGN_IO_ERR_VERSION;
    }

    /* Validate sizes */
    if (hdr.vocab_size > EH_HGN_VOCAB_SIZE) {
        fprintf(stderr, "[eh_hgn_io] vocab_size %u exceeds limit %u\n",
                hdr.vocab_size, EH_HGN_VOCAB_SIZE);
        fclose(fp);
        return EH_HGN_IO_ERR_SIZE;
    }

    if (hdr.total_edges > EH_HGN_MAX_EDGES) {
        fprintf(stderr, "[eh_hgn_io] total_edges %u exceeds limit %u\n",
                hdr.total_edges, EH_HGN_MAX_EDGES);
        fclose(fp);
        return EH_HGN_IO_ERR_SIZE;
    }

    if (hdr.embed_dim != EH_HGN_EMBED_DIM) {
        fprintf(stderr, "[eh_hgn_io] embed_dim %u != %u\n",
                hdr.embed_dim, EH_HGN_EMBED_DIM);
        fclose(fp);
        return EH_HGN_IO_ERR_SIZE;
    }

    if (hdr.max_fanout > EH_HGN_MAX_FANOUT) {
        fprintf(stderr, "[eh_hgn_io] max_fanout %u exceeds limit %u\n",
                hdr.max_fanout, EH_HGN_MAX_FANOUT);
        fclose(fp);
        return EH_HGN_IO_ERR_SIZE;
    }

    /* Calculate section sizes */
    size_t sz_embed   = (size_t)hdr.vocab_size  * sizeof(EH_HGN_NodeEmbed);
    size_t sz_adj     = (size_t)hdr.vocab_size  * sizeof(EH_HGN_NodeAdj);
    size_t sz_compact = (size_t)hdr.total_edges * sizeof(EH_HGN_EdgeCompact);
    size_t sz_weight  = (size_t)hdr.total_edges * sizeof(EH_HGN_EdgeWeight);

    /* Allocate from arena (32-byte aligned for embeddings/weights) */
    EH_HGN_NodeEmbed   *node_embed   = eh_arena_alloc(arena, sz_embed);
    EH_HGN_NodeAdj     *node_adj     = eh_arena_alloc(arena, sz_adj);
    EH_HGN_EdgeCompact *edge_compact = eh_arena_alloc(arena, sz_compact);
    EH_HGN_EdgeWeight  *weight_pool  = eh_arena_alloc(arena, sz_weight);

    if (!node_embed || !node_adj || !edge_compact || !weight_pool) {
        size_t need = sz_embed + sz_adj + sz_compact + sz_weight;
        size_t avail = (arena->capacity > arena->used)
                     ? (arena->capacity - arena->used) : 0;
        fprintf(stderr, "[eh_hgn_io] Arena OOM: need %zu bytes, avail %zu\n",
                need, avail);
        fclose(fp);
        return EH_HGN_IO_ERR_ARENA;
    }

    /* Zero-copy read into arena */
    if (fread(node_embed, sz_embed, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] node_embed read failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    if (fread(node_adj, sz_adj, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] node_adj read failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    if (fread(edge_compact, sz_compact, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] edge_compact read failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    /* Skip padding to 32-byte alignment before weight_pool */
    long pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    long aligned_pos = (pos + 31L) & ~31L;
    if (aligned_pos != pos) {
        if (fseek(fp, aligned_pos, SEEK_SET) != 0) {
            fprintf(stderr, "[eh_hgn_io] fseek alignment failed\n");
            fclose(fp);
            return EH_HGN_IO_ERR_ALIGN;
        }
    }

    if (fread(weight_pool, sz_weight, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] weight_pool read failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    fclose(fp);

    /* CSR integrity check: O(V+E) */
    for (uint32_t i = 0; i < hdr.vocab_size; i++) {
        const EH_HGN_NodeAdj *adj = &node_adj[i];

        /* Check offset and count bounds */
        if (adj->edge_offset > hdr.total_edges ||
            adj->edge_count > EH_HGN_MAX_FANOUT ||
            (uint64_t)adj->edge_offset + adj->edge_count > hdr.total_edges)
        {
            fprintf(stderr, "[eh_hgn_io] CSR corrupt at node %u\n", i);
            return EH_HGN_IO_ERR_CORRUPT;
        }

        /* Validate each edge */
        const EH_HGN_EdgeCompact *e = edge_compact + adj->edge_offset;
        const EH_HGN_EdgeCompact *e_end = e + adj->edge_count;

        for (; e != e_end; e++) {
            if (e->dst >= hdr.vocab_size) {
                fprintf(stderr, "[eh_hgn_io] Edge dst %u OOB at node %u\n",
                        e->dst, i);
                return EH_HGN_IO_ERR_CORRUPT;
            }

            if (e->weight_idx >= hdr.total_edges) {
                fprintf(stderr, "[eh_hgn_io] weight_idx %u OOB at node %u\n",
                        e->weight_idx, i);
                return EH_HGN_IO_ERR_CORRUPT;
            }
        }
    }

    /* Populate BaseDag */
    dag->vocab_size   = hdr.vocab_size;
    dag->total_edges  = hdr.total_edges;
    dag->embed_dim    = hdr.embed_dim;
    dag->max_fanout   = hdr.max_fanout;
    dag->node_embed   = node_embed;
    dag->node_adj     = node_adj;
    dag->edge_compact = edge_compact;
    dag->weight_pool  = weight_pool;

    return EH_HGN_IO_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_io_save
 * ---------------------------------------------------------------- */
EH_HGN_IO_Status eh_hgn_io_save(const EH_HGN_BaseDag *dag,
                                 const char           *path)
{
    if (!dag || !path) return EH_HGN_IO_ERR_FILE;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "[eh_hgn_io] Cannot create '%s': %s\n",
                path, strerror(errno));
        return EH_HGN_IO_ERR_FILE;
    }

    /* Write header */
    EH_HGN_IO_Header hdr = {
        .magic = EH_HGN_IO_MAGIC,
        .version = EH_HGN_IO_VERSION,
        .vocab_size = dag->vocab_size,
        .total_edges = dag->total_edges,
        .embed_dim = dag->embed_dim,
        .max_fanout = dag->max_fanout,
        .reserved = {0, 0}
    };

    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] Header write failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    /* Write node embeddings */
    size_t sz_embed = (size_t)dag->vocab_size * sizeof(EH_HGN_NodeEmbed);
    if (fwrite(dag->node_embed, sz_embed, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] node_embed write failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    /* Write node adjacency */
    size_t sz_adj = (size_t)dag->vocab_size * sizeof(EH_HGN_NodeAdj);
    if (fwrite(dag->node_adj, sz_adj, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] node_adj write failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    /* Write edge compact */
    size_t sz_compact = (size_t)dag->total_edges * sizeof(EH_HGN_EdgeCompact);
    if (fwrite(dag->edge_compact, sz_compact, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] edge_compact write failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    /* Pad to 32-byte alignment */
    long pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    long aligned_pos = (pos + 31L) & ~31L;
    if (aligned_pos != pos) {
        uint8_t padding[32] = {0};
        size_t pad_size = (size_t)(aligned_pos - pos);
        if (fwrite(padding, pad_size, 1, fp) != 1) {
            fprintf(stderr, "[eh_hgn_io] Padding write failed\n");
            fclose(fp);
            return EH_HGN_IO_ERR_FILE;
        }
    }

    /* Write edge weights */
    size_t sz_weight = (size_t)dag->total_edges * sizeof(EH_HGN_EdgeWeight);
    if (fwrite(dag->weight_pool, sz_weight, 1, fp) != 1) {
        fprintf(stderr, "[eh_hgn_io] weight_pool write failed\n");
        fclose(fp);
        return EH_HGN_IO_ERR_FILE;
    }

    fclose(fp);
    return EH_HGN_IO_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_io_validate
 * ---------------------------------------------------------------- */
EH_HGN_IO_Status eh_hgn_io_validate(const char *path)
{
    if (!path) return EH_HGN_IO_ERR_FILE;

    FILE *fp = fopen(path, "rb");
    if (!fp) return EH_HGN_IO_ERR_FILE;

    /* Read header */
    EH_HGN_IO_Header hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    /* Validate */
    if (hdr.magic != EH_HGN_IO_MAGIC) {
        fclose(fp);
        return EH_HGN_IO_ERR_MAGIC;
    }

    if (hdr.version != EH_HGN_IO_VERSION) {
        fclose(fp);
        return EH_HGN_IO_ERR_VERSION;
    }

    if (hdr.vocab_size > EH_HGN_VOCAB_SIZE ||
        hdr.total_edges > EH_HGN_MAX_EDGES ||
        hdr.embed_dim != EH_HGN_EMBED_DIM ||
        hdr.max_fanout > EH_HGN_MAX_FANOUT)
    {
        fclose(fp);
        return EH_HGN_IO_ERR_SIZE;
    }

    fclose(fp);
    return EH_HGN_IO_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_io_read_header
 * ---------------------------------------------------------------- */
EH_HGN_IO_Status eh_hgn_io_read_header(const char        *path,
                                        EH_HGN_IO_Header  *header)
{
    if (!path || !header) return EH_HGN_IO_ERR_FILE;

    FILE *fp = fopen(path, "rb");
    if (!fp) return EH_HGN_IO_ERR_FILE;

    if (fread(header, sizeof(*header), 1, fp) != 1) {
        fclose(fp);
        return EH_HGN_IO_ERR_CORRUPT;
    }

    fclose(fp);

    /* Basic validation */
    if (header->magic != EH_HGN_IO_MAGIC)
        return EH_HGN_IO_ERR_MAGIC;

    if (header->version != EH_HGN_IO_VERSION)
        return EH_HGN_IO_ERR_VERSION;

    return EH_HGN_IO_OK;
}

/* ----------------------------------------------------------------
 * eh_hgn_io_strerror
 * ---------------------------------------------------------------- */
const char *eh_hgn_io_strerror(EH_HGN_IO_Status status)
{
    switch (status) {
        case EH_HGN_IO_OK:          return "Success";
        case EH_HGN_IO_ERR_FILE:    return "File I/O error";
        case EH_HGN_IO_ERR_MAGIC:   return "Invalid magic number";
        case EH_HGN_IO_ERR_VERSION: return "Unsupported version";
        case EH_HGN_IO_ERR_SIZE:    return "Size exceeds limits";
        case EH_HGN_IO_ERR_ARENA:   return "Arena out of memory";
        case EH_HGN_IO_ERR_CORRUPT: return "Data corruption";
        case EH_HGN_IO_ERR_ALIGN:   return "Alignment error";
        default:                    return "Unknown error";
    }
}
