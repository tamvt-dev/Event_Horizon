#!/usr/bin/env python3
with open('training/trigram_v16_vocab.txt.pairs') as f:
    pairs = []
    for l in f:
        if l.startswith('#'): continue
        p = l.split()
        if len(p)==3: pairs.append((int(p[0]),int(p[1]),int(p[2])))

with open('training/trigram_v16_vocab.txt') as f:
    vocab = [l.strip() for l in f]

git_id = next(i for i,w in enumerate(vocab) if w=='git')
is_id  = next(i for i,w in enumerate(vocab) if w=='is')

print(f'git={git_id}, is={is_id}')
print(f'Total pairs: {len(pairs)}, max pid: {max(p[0] for p in pairs)}')

ig = [(pid,a,b) for pid,a,b in pairs if a==is_id and b==git_id]
gi = [(pid,a,b) for pid,a,b in pairs if a==git_id and b==is_id]
print(f'(is,git): {ig}')
print(f'(git,is): {gi}')

# Check DAG node count from file
import struct
with open('training/trigram_v16_model.ehdag', 'rb') as f:
    magic = f.read(4)
    version = struct.unpack('<I', f.read(4))[0]
    vocab_sz = struct.unpack('<I', f.read(4))[0]
    total_edges = struct.unpack('<I', f.read(4))[0]
    print(f'DAG: vocab_size={vocab_sz}, total_edges={total_edges}')
    print(f'(is,git) pid={ig[0][0] if ig else "N/A"} vs DAG vocab_size={vocab_sz}')
    if ig:
        pid = ig[0][0]
        print(f'(is,git) in DAG: {pid < vocab_sz}')
