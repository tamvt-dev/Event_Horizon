#!/usr/bin/env python3
"""Build v9 corpus: anchors front-loaded for vocab seeding, then interleaved."""

def load(path):
    lines = []
    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            l = line.rstrip('\r\n').strip()  # strip CRLF + whitespace
            if l and not l.startswith('#'):
                lines.append(l)
    return lines

marco   = load('training/merged_v4_prose.txt')
general = load('training/definition_anchors.txt')
div1    = load('training/divergence_anchors.txt')
div2    = load('training/divergence_v2.txt')

W_G, W_D1, W_D2 = 5, 10, 15

# Front-load 1x of all anchors so every anchor word enters vocab
# before MARCO fills the 4000-slot cap
front = general + div1 + div2

# Remaining repetitions interleaved throughout MARCO
boosted = general * (W_G - 1) + div1 * (W_D1 - 1) + div2 * (W_D2 - 1)

# Split MARCO into 10 chunks, inject anchor slices between them
chunk_size = len(marco) // 10
chunks = [marco[i:i+chunk_size] for i in range(0, len(marco), chunk_size)]
anc_per = len(boosted) // len(chunks)

out = list(front)   # anchors FIRST
idx = 0
for chunk in chunks:
    out.extend(chunk)
    out.extend(boosted[idx:idx + anc_per])
    idx += anc_per
out.extend(boosted[idx:])   # remainder

with open('training/merged_v9_prose.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

python_count = sum(1 for l in front if l == 'python is')
print(f'Front anchors  : {len(front)} lines (vocab seeding)')
print(f'Boosted repeats: {len(boosted)} lines')
print(f'Total corpus   : {len(out):,} lines')
print(f'"python is" in front: {python_count}')
print(f'Written: training/merged_v9_prose.txt')
