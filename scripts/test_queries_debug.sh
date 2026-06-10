#!/bin/bash
cd /mnt/c/Users/Administrator/Desktop/my_project/eventhorizon

# Add debug print to eh_query to see start_pair
gcc -O0 -g -DDEBUG_START_PAIR -std=c99 -Wno-unused-parameter -Wno-sign-compare \
    -Iinclude -Iinclude/core -Iinclude/hgn \
    src/core/*.c src/hgn/*.c examples/eh_query.c \
    -o examples/eh_query_debug -lm

printf 'what is git\nwhat is evolution\nwho is shakespeare\n' \
    | ./examples/eh_query_debug \
        training/trigram_v16_model.ehdag \
        training/eh_g3_embeddings.bin \
        training/trigram_v16_vocab.txt \
        training/trigram_v16_vocab.txt.pairs 2>&1 | grep -E "Q:|start_pair|domain|DEBUG"
