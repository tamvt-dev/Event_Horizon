#!/usr/bin/env python3
with open('training/trigram_v16_vocab.txt.pairs') as f:
    pairs = []
    for l in f:
        if l.startswith('#'): continue
        p = l.split()
        if len(p)==3: pairs.append((int(p[0]),int(p[1]),int(p[2])))

with open('training/trigram_v16_vocab.txt') as f:
    vocab = [l.strip() for l in f]

dag_size = 14430

for word in ['git','shakespeare','evolution','electricity','cleopatra','entropy']:
    wid = next((i for i,w in enumerate(vocab) if w==word), -1)
    is_id = next((i for i,w in enumerate(vocab) if w=='is'), -1)

    # Pairs starting WITH this word that are in DAG
    starts = [(pid,a,b) for pid,a,b in pairs if a==wid and pid<dag_size]
    print(f"{word}({wid}): pairs_starting IN DAG: {len(starts)}")
    for pid,a,b in starts[:5]:
        wb = vocab[b] if b<len(vocab) else str(b)
        print(f"  pid={pid}: ({word},{wb})")

    # (is, word) in DAG
    ig = [(pid,a,b) for pid,a,b in pairs if a==is_id and b==wid and pid<dag_size]
    print(f"  (is,{word}) in DAG: {ig}")
    print()
