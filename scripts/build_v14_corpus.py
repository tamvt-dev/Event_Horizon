#!/usr/bin/env python3
"""
Build v14 corpus: v11 base + filtered distilled (v13 filter) + divergence_v3 fixes.

Changes from v13:
  - Added divergence_v3.txt: hello anchor, gravity/dna/author/einstein/turing fixes
  - divergence_v3 weight: x20 (critical fixes need strong signal)
  - Everything else same as v13 (v11 base + same distilled filter)
"""

import re
import random

# ── Toxic patterns (same as v13) ──────────────────────────────
TOXIC_ANSWER = [
    r'\bin europe\b', r'\bin asia\b', r'\bin north america\b', r'\bin south america\b',
    r'\bin africa\b', r'\bin australia\b', r'\bin the world\b', r'\bin many countries\b',
    r'\bin various\b', r'\bin all\b', r'\bworldwide\b', r'\beverywhere\b',
    r'\bin the universe\b', r'\baround the world\b', r'\bglobally\b',
    r'\bin many places\b', r'\bin many parts\b', r'\bin most\b',
    r'\bin different\b', r'\bin numerous\b',
]

TOXIC_QUESTION = [r'^where is\b', r'^where are\b']

SAFE_PREFIXES = [
    'what is ', 'what are ', 'what does ', 'define ', 'who is ',
    'what is a ', 'what is an ', 'how does ',
]

def is_toxic_answer(a):
    return any(re.search(p, a) for p in TOXIC_ANSWER)

def is_toxic_question(q):
    return any(re.search(p, q) for p in TOXIC_QUESTION)

def is_safe(q):
    return any(q.startswith(p) for p in SAFE_PREFIXES)


# ── Parse all distilled files (same as v13) ───────────────────
DISTILLED = [
    'training/openrouter.txt',
    'training/openrouter1.txt',
    'training/qa_distilled.txt',
    'training/qa_gemini.txt',
]

accepted = []
seen_q = set()
stats = {'low_conf': 0, 'toxic_q': 0, 'toxic_a': 0, 'unsafe': 0, 'dup': 0}

for path in DISTILLED:
    try:
        with open(path, encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except FileNotFoundError:
        continue

    for block in re.split(r'# conf=', content):
        if not block.strip(): continue
        lines = block.strip().splitlines()
        if not lines: continue
        m = re.match(r'^(\d+\.?\d*)', lines[0])
        if not m: continue
        conf = float(m.group(1))

        if conf < 8.0:
            stats['low_conf'] += 1
            continue

        text = [l.strip() for l in lines[1:] if l.strip()]
        if len(text) < 2: continue

        q = re.sub(r'[^\x20-\x7e]', ' ', text[0]).strip().lower()
        a = re.sub(r'[^\x20-\x7e]', ' ', text[1]).strip().lower()

        if len(q) < 4 or len(a.split()) < 2: continue

        if q in seen_q:
            stats['dup'] += 1
            continue
        if is_toxic_question(q):
            stats['toxic_q'] += 1
            continue
        if is_toxic_answer(a):
            stats['toxic_a'] += 1
            continue
        if not is_safe(q):
            stats['unsafe'] += 1
            continue

        words = a.split()
        if len(words) > 12:
            a = ' '.join(words[:12])

        seen_q.add(q)
        accepted.append((q, a, conf))

print(f"Accepted distilled: {len(accepted):,} clean pairs")
for k, v in stats.items():
    print(f"  rejected/{k}: {v:,}")


# ── Weight distilled by confidence (same as v13) ─────────────
high = [(q,a) for q,a,c in accepted if c >= 9.0]
mid  = [(q,a) for q,a,c in accepted if 8.0 <= c < 9.0]

distilled_lines = []
for q,a in high:
    distilled_lines.extend([q,a] * 15)
for q,a in mid:
    distilled_lines.extend([q,a] * 8)

print(f"\nDistilled boost:")
print(f"  high (>=9.0): {len(high):,} × 15 = {len(high)*30:,} lines")
print(f"  mid  (8-8.9): {len(mid):,} × 8  = {len(mid)*16:,} lines")
print(f"  total boost:  {len(distilled_lines):,} lines")


# ── Load v11 base ─────────────────────────────────────────────
with open('training/merged_v11_prose.txt', encoding='utf-8', errors='ignore') as f:
    v11 = [l.rstrip('\r\n').strip() for l in f if l.strip() and not l.startswith('#')]

print(f"\nv11 base:     {len(v11):,} lines")


# ── Load anchor files ─────────────────────────────────────────
# ANCHORS_X20: high-priority divergence fixes (placed front AND repeated)
ANCHORS_NORMAL = [
    'training/definition_anchors.txt',
    'training/divergence_anchors.txt',
    'training/divergence_v2.txt',
    'training/geography_anchors.txt',
]

ANCHORS_HIGH = [
    'training/divergence_v3.txt',  # critical fixes — x20
    'training/divergence_v4.txt',  # hello + einstein — x30
]

def load_anchor(path):
    lines = []
    try:
        with open(path, encoding='utf-8', errors='ignore') as f:
            for line in f:
                l = line.rstrip('\r\n').strip()
                if l and not l.startswith('#'):
                    lines.append(l)
    except FileNotFoundError:
        print(f"  [WARN] not found: {path}")
    return lines

front = []
for path in ANCHORS_NORMAL:
    data = load_anchor(path)
    front.extend(data)

v3_data = []
for path in ANCHORS_HIGH:
    data = load_anchor(path)
    v3_data.extend(data)
    weight = 30 if 'v4' in path else 20
    front.extend(data * weight)

print(f"Anchor normal seed:  {len(front) - len(v3_data)*20:,} lines")
print(f"Anchor v3 (x20):     {len(v3_data):,} raw = {len(v3_data)*20:,} repeated lines")
print(f"Total front:         {len(front):,} lines")


# ── Interleave distilled throughout v11 ───────────────────────
random.seed(42)
random.shuffle(distilled_lines)

chunk_size = max(1, len(v11) // 10)
chunks = [v11[i:i+chunk_size] for i in range(0, len(v11), chunk_size)]
boost_per = len(distilled_lines) // max(1, len(chunks))

out = list(front)  # anchors first (vocab seeding)

idx = 0
for chunk in chunks:
    out.extend(chunk)
    out.extend(distilled_lines[idx:idx + boost_per])
    idx += boost_per
out.extend(distilled_lines[idx:])  # remainder

# Also sprinkle v3 anchors again throughout for extra reinforcement
random.seed(123)
v3_shuffled = list(v3_data) * 5  # 5 more passes of v3
random.shuffle(v3_shuffled)
# Insert v3 every ~5000 lines
for i in range(0, len(out), 5000):
    v3_chunk = v3_shuffled[:100]
    v3_shuffled = v3_shuffled[100:]
    out[i:i] = v3_chunk

print(f"Final corpus: {len(out):,} lines")

# ── Write LF-only ─────────────────────────────────────────────
with open('training/merged_v15_prose.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

print(f"\nWritten: training/merged_v15_prose.txt")
