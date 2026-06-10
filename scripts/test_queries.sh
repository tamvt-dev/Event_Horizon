#!/bin/bash
cd /mnt/c/Users/Administrator/Desktop/my_project/eventhorizon

gcc -O3 -std=c99 -Wno-unused-parameter -Wno-sign-compare -march=native \
    -Iinclude -Iinclude/core -Iinclude/hgn \
    src/core/*.c src/hgn/*.c examples/eh_query.c \
    -o examples/eh_query -lm

printf 'what is git\nwho is shakespeare\nwhat is evolution\nwhat is the capital of italy\nwhat is electricity\nwhat is python\nwho is einstein\nwhat is the capital of germany\nwhat is a cpu\nwho is cleopatra\n' \
    | ./examples/eh_query \
        training/trigram_v16_model.ehdag \
        training/eh_g3_embeddings.bin \
        training/trigram_v16_vocab.txt \
        training/trigram_v16_vocab.txt.pairs 2>/dev/null
