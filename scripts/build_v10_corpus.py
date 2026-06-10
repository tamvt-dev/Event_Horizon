#!/usr/bin/env python3
"""Build v10 corpus: per-concept weight control + CRLF-safe."""

def load(path):
    lines = []
    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            l = line.rstrip('\r\n').strip()
            if l and not l.startswith('#'):
                lines.append(l)
    return lines


def load_pairs(path):
    """Load as (question, answer) pairs, returns list of (q, a) tuples."""
    lines = load(path)
    pairs = []
    for i in range(0, len(lines) - 1, 2):
        pairs.append((lines[i], lines[i+1]))
    return pairs


def boost_selective(pairs, default_weight, overrides):
    """
    Repeat pairs with per-concept weight control.
    overrides: dict mapping substring -> weight
    e.g. {'hash': 2, 'tree': 20, 'computer': 20}
    """
    out = []
    for q, a in pairs:
        w = default_weight
        for keyword, weight in overrides.items():
            if keyword in q or keyword in a:
                w = weight
                break
        for _ in range(w):
            out.append(q)
            out.append(a)
    return out


# ── Load sources ───────────────────────────────────────────────
marco    = load('training/merged_v4_prose.txt')
general  = load_pairs('training/definition_anchors.txt')
div1     = load_pairs('training/divergence_anchors.txt')
div2     = load_pairs('training/divergence_v2.txt')

# ── Per-concept weight overrides ──────────────────────────────
# Bleeding concepts get LOWER weight
# Victim concepts get HIGHER weight
GENERAL_OVERRIDES = {
    'hash':           2,   # was 5 — bleeding source, reduce
    'blockchain':     2,   # uses "hash" in context
    'computer':      10,   # victim — boost
    'database':      10,   # victim — boost
    'tree':          15,   # victim (gravity bleeding) — boost most
    'python':        12,   # victim — boost
    'france':        10,   # victim — boost
    'paris':         10,
}

DIV1_OVERRIDES = {
    'hash':           2,   # reduce
    'computer':      12,
    'database':      12,
    'tree':          18,
    'python':        15,
    'france':        12,
    'paris':         12,
}

DIV2_OVERRIDES = {
    'hash':           3,
    'tree':          20,   # max boost — worst victim
    'database':      15,
    'python':        18,
    'france':        15,
    'paris':         15,
    'gravity':        3,   # gravity is bleeding source for tree
}

# ── Build boosted anchor blocks ───────────────────────────────
boosted_general = boost_selective(general, default_weight=5,  overrides=GENERAL_OVERRIDES)
boosted_div1    = boost_selective(div1,    default_weight=10, overrides=DIV1_OVERRIDES)
boosted_div2    = boost_selective(div2,    default_weight=15, overrides=DIV2_OVERRIDES)

# Front-load 1x of everything (vocab seeding — LF only)
front = [l for p in general for l in p] + \
        [l for p in div1    for l in p] + \
        [l for p in div2    for l in p]

# All boosted anchors combined
all_anchors = boosted_general + boosted_div1 + boosted_div2

# ── Interleave through MARCO ──────────────────────────────────
chunk_count = 10
chunk_size = len(marco) // chunk_count
chunks = [marco[i:i+chunk_size] for i in range(0, len(marco), chunk_size)]
anc_per = len(all_anchors) // len(chunks)

out = list(front)
idx = 0
for chunk in chunks:
    out.extend(chunk)
    out.extend(all_anchors[idx:idx + anc_per])
    idx += anc_per
out.extend(all_anchors[idx:])

# ── Write LF-only ─────────────────────────────────────────────
with open('training/merged_v10_prose.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

# ── Stats ─────────────────────────────────────────────────────
hash_count  = sum(1 for l in all_anchors if 'hash' in l)
tree_count  = sum(1 for l in all_anchors if 'tree' in l and 'street' not in l)
comp_count  = sum(1 for l in all_anchors if 'computer' in l)
db_count    = sum(1 for l in all_anchors if 'database' in l)
py_count    = sum(1 for l in all_anchors if 'python' in l)

print(f"Front anchors  : {len(front)} lines (vocab seeding)")
print(f"Boosted anchors: {len(all_anchors)} lines")
print(f"Total corpus   : {len(out):,} lines")
print()
print("Per-concept anchor lines (boosted):")
print(f"  hash     : {hash_count:4d}  (reduced — was bleeding source)")
print(f"  tree     : {tree_count:4d}  (max boost — worst victim)")
print(f"  computer : {comp_count:4d}")
print(f"  database : {db_count:4d}")
print(f"  python   : {py_count:4d}")
print()
print("Written: training/merged_v10_prose.txt")
