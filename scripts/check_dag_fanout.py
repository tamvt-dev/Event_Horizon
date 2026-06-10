#!/usr/bin/env python3
"""Check DAG fanout for specific tokens by reading the .ehdag binary directly."""
import struct, sys

dag_path  = "training/trigram_v16_model.ehdag"
vocab_path = "training/trigram_v16_vocab.txt"

with open(vocab_path, encoding='utf-8', errors='ignore') as f:
    vocab = [l.strip() for l in f if l.strip()]

CHECK = ['git','shakespeare','evolution','electricity','cleopatra','entropy','is','what','python']

# The .ehdag format: read the binary to find node adjacency
# We'll use the fact that the pairs file encodes the DAG structure
pairs_path = "training/trigram_v16_vocab.txt.pairs"
with open(pairs_path) as f:
    pairs = []
    for line in f:
        if line.startswith('#'): continue
        p = line.split()
        if len(p)==3: pairs.append((int(p[0]),int(p[1]),int(p[2])))

# Build edge list: token A -> [token B ...] (from corpus bigrams in pairs)
from collections import defaultdict
edges_from = defaultdict(set)
for pid,a,b in pairs:
    edges_from[a].add(b)

print("Token fanout (from pairs file = expected DAG edges):")
for word in CHECK:
    wid = next((i for i,w in enumerate(vocab) if w==word), None)
    if wid is None:
        print(f"  {word}: NOT IN VOCAB")
        continue
    fanout = len(edges_from[wid])
    continuations = list(edges_from[wid])[:5]
    cont_words = [vocab[t] if t<len(vocab) else f'?{t}' for t in continuations]
    print(f"  {word:15} id={wid:4} fanout={fanout:3}  sample_next={cont_words}")

print(f"\nTotal pairs: {len(pairs)}")
print(f"Vocab size: {len(vocab)}")

# Check if beam sees these: in trigram model, the DAG node = token_id
# The beam's last_token after starting at pair(a,b) is b
# It calls eh_hgn_dag_fanout(dag, b) to check if there are outgoing edges
# This is equivalent to: how many times does token b appear as 'a' in some bigram
print("\nKey insight: beam last_token = pair.token_b")
print("Beam checks fanout(token_b) = edges starting FROM token_b")
for word in ['git','is','evolution']:
    wid = next((i for i,w in enumerate(vocab) if w==word), None)
    if wid is None: continue
    fanout = len(edges_from[wid])
    print(f"  {word} as token_b: fanout={fanout} (beam can expand if > 0)")
