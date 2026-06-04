/* Simple model verification */
#include "hgn/eh_hgn_io.h"
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.ehdag>\n", argv[0]);
        return 1;
    }

    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);
    EH_HGN_BaseDag dag;
    
    EH_HGN_IO_Status rc = eh_hgn_io_load(arena, argv[1], &dag);
    if (rc != EH_HGN_IO_OK) {
        printf("Load failed: %s\n", eh_hgn_io_strerror(rc));
        return 1;
    }
    
    printf("\n=== Trained Model Verification ===\n\n");
    eh_hgn_dag_dump_info(&dag);
    
    eh_arena_destroy(arena);
    return 0;
}
