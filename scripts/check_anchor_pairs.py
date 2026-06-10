#!/usr/bin/env python3
"""Check if key anchor pairs from v5 are in DAG (pid < dag_size)."""
with open('training/trigram_v16_vocab.txt.pairs') as f:
    pairs = []
    for l in f:
        if l.startswith('#'): continue
        p = l.split()
        if len(p)==3: pairs.append((int(p[0]),int(p[1]),int(p[2])))

with open('training/trigram_v16_vocab.txt') as f:
    vocab = [l.strip() for l in f]

dag_size = 14430

# Check key concept pairs
concepts = [
    ('evolution','is'), ('electricity','is'), ('entropy','is'),
    ('cleopatra','was'), ('shakespeare','wrote'), ('git','is'),
    ('evolution','works'), ('electricity','from'),
    ('gravity','is'), ('dna','is'), ('virus','is'),
    ('napoleon','was'), ('darwin','was'), ('curie','was'),
    ('italy','is'), ('spain','is'), ('china','is'),
    ('javascript','is'), ('html','is'), ('css','is'),
]

print(f"DAG size: {dag_size}")
print(f"Total pairs in file: {len(pairs)}")
print(f"\nConcept pair status:")
for a_word, b_word in concepts:
    a_id = next((i for i,w in enumerate(vocab) if w==a_word), -1)
    b_id = next((i for i,w in enumerate(vocab) if w==b_word), -1)
    if a_id < 0: print(f"  ({a_word},{b_word}): '{a_word}' not in vocab"); continue
    if b_id < 0: print(f"  ({a_word},{b_word}): '{b_word}' not in vocab"); continue
    found = [(pid,a,b) for pid,a,b in pairs if a==a_id and b==b_id]
    if found:
        pid = found[0][0]
        status = "IN DAG ✓" if pid < dag_size else f"OUT OF DAG ✗ (pid={pid})"
        print(f"  ({a_word},{b_word}): pid={pid} — {status}")
    else:
        print(f"  ({a_word},{b_word}): NOT IN PAIRS FILE")

# Show distribution: how many pairs are in DAG vs out
in_dag = sum(1 for pid,a,b in pairs if pid < dag_size)
out_dag = sum(1 for pid,a,b in pairs if pid >= dag_size)
print(f"\nIn DAG: {in_dag}, Out of DAG: {out_dag}")
print(f"Wasted pairs (not accessible): {out_dag}")
