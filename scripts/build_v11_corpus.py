#!/usr/bin/env python3
"""Build v11 corpus: add geography anchors x20 to fix capitals."""

def load_pairs(path):
    pairs = []
    with open(path, encoding='utf-8', errors='ignore') as f:
        lines = [l.rstrip('\r\n').strip() for l in f
                 if l.strip() and not l.strip().startswith('#')]
    for i in range(0, len(lines) - 1, 2):
        pairs.append((lines[i], lines[i+1]))
    return pairs

def load_lines(path):
    with open(path, encoding='utf-8', errors='ignore') as f:
        return [l.rstrip('\r\n').strip() for l in f
                if l.strip() and not l.strip().startswith('#')]

def boost(pairs, default_w, overrides=None):
    out = []
    overrides = overrides or {}
    for q, a in pairs:
        w = default_w
        for kw, wt in overrides.items():
            if kw in q or kw in a:
                w = wt
                break
        out += [q, a] * w
    return out

# ── Sources ────────────────────────────────────────────────────
marco    = load_lines('training/merged_v4_prose.txt')
general  = load_pairs('training/definition_anchors.txt')
div1     = load_pairs('training/divergence_anchors.txt')
div2     = load_pairs('training/divergence_v2.txt')
geo      = load_pairs('training/geography_anchors.txt')

# ── Weights (same as v10 + geo x20) ──────────────────────────
GEN_OVR = {'hash':2, 'computer':10, 'database':10, 'tree':15, 'python':12,
           'france':10, 'paris':10}
D1_OVR  = {'hash':2, 'computer':12, 'database':12, 'tree':18, 'python':15,
           'france':12, 'paris':12}
D2_OVR  = {'hash':3, 'tree':20, 'database':15, 'python':18,
           'france':15, 'paris':15, 'gravity':3}
GEO_OVR = {'tokyo':25, 'berlin':25, 'japan':25, 'germany':25,
           'france':20, 'paris':20, 'italy':20, 'rome':20,
           'spain':20, 'madrid':20, 'china':20, 'beijing':20,
           'canada':20, 'ottawa':20, 'where':20}

bg  = boost(general, 5,  GEN_OVR)
bd1 = boost(div1,    10, D1_OVR)
bd2 = boost(div2,    15, D2_OVR)
bgeo= boost(geo,     20, GEO_OVR)

# Front-load all anchors (vocab seeding)
front = ([l for p in general for l in p] + [l for p in div1 for l in p] +
         [l for p in div2 for l in p]    + [l for p in geo  for l in p])

all_anchors = bg + bd1 + bd2 + bgeo

# Interleave through MARCO
chunk_size = len(marco) // 10
chunks = [marco[i:i+chunk_size] for i in range(0, len(marco), chunk_size)]
anc_per = len(all_anchors) // len(chunks)

out = list(front)
idx = 0
for chunk in chunks:
    out.extend(chunk)
    out.extend(all_anchors[idx:idx+anc_per])
    idx += anc_per
out.extend(all_anchors[idx:])

with open('training/merged_v11_prose.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

geo_count = sum(1 for l in all_anchors
               if any(w in l for w in ['tokyo','berlin','paris','france']))
print(f"Front anchors  : {len(front):,} lines (vocab seeding)")
print(f"Boosted anchors: {len(all_anchors):,} lines")
print(f"  Geography    : {len(bgeo):,} lines (x20)")
print(f"  Key geo lines: {geo_count:,} (tokyo/berlin/paris/france)")
print(f"Total corpus   : {len(out):,} lines")
print(f"Written: training/merged_v11_prose.txt")
