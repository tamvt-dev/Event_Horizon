#!/usr/bin/env python3
"""Build v12 corpus: integrate all distilled Q&A files + existing anchors."""

import re
import random

def load_prose(path):
    """Load existing prose corpus (one sentence per line)."""
    lines = []
    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            l = line.rstrip('\r\n').strip()
            if l and not l.startswith('#'):
                lines.append(l)
    return lines

def parse_distilled(path, min_conf=5.0, max_answer_words=15):
    """
    Parse distilled Q&A files with format:
      # conf=X.X model=...
      question
      answer

    Returns list of (question, answer, confidence) tuples.
    Applies quality gate and truncation.
    """
    pairs = []
    skipped = 0

    with open(path, encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Split by confidence comment blocks
    blocks = re.split(r'# conf=', content)
    
    for block in blocks:
        if not block.strip():
            continue
        
        lines = block.strip().splitlines()
        if not lines:
            continue

        # Parse confidence from first line
        conf_line = lines[0].strip()
        conf_match = re.match(r'^(\d+\.?\d*)', conf_line)
        if not conf_match:
            continue
        conf = float(conf_match.group(1))

        if conf < min_conf:
            skipped += 1
            continue

        # Find question and answer (skip empty lines)
        text_lines = [l.strip() for l in lines[1:] if l.strip()]
        if len(text_lines) < 2:
            skipped += 1
            continue

        question = text_lines[0].lower()
        answer = text_lines[1].lower()

        # Quality checks
        if len(question) < 4 or len(answer) < 4:
            skipped += 1
            continue

        # Remove non-ASCII artifacts
        question = re.sub(r'[^\x20-\x7e]', ' ', question).strip()
        answer = re.sub(r'[^\x20-\x7e]', ' ', answer).strip()

        # Truncate answer if too long
        words = answer.split()
        if len(words) > max_answer_words:
            answer = ' '.join(words[:max_answer_words])

        # Skip if answer has no real content
        if len(answer.split()) < 2:
            skipped += 1
            continue

        pairs.append((question, answer, conf))

    return pairs, skipped


def distilled_to_prose(pairs, weight_by_confidence=True):
    """Convert distilled pairs to prose lines, optionally weighted by confidence."""
    out = []
    for q, a, conf in pairs:
        out.append(q)
        out.append(a)
    return out


# ── Sources ────────────────────────────────────────────────────
DISTILLED_FILES = [
    'training/openrouter.txt',
    'training/openrouter1.txt',
    'training/qa_distilled.txt',
    'training/qa_gemini.txt',
]

# Existing anchor files (front-load for vocab seeding)
ANCHOR_FILES = [
    'training/definition_anchors.txt',
    'training/divergence_anchors.txt',
    'training/divergence_v2.txt',
    'training/geography_anchors.txt',
]

# Existing MARCO corpus
MARCO = load_prose('training/merged_v4_prose.txt')

# ── Parse all distilled files ──────────────────────────────────
all_pairs = []
total_skipped = 0
for fpath in DISTILLED_FILES:
    try:
        pairs, skipped = parse_distilled(fpath, min_conf=5.0)
        print(f"  {fpath}: {len(pairs):,} pairs accepted, {skipped:,} skipped")
        all_pairs.extend(pairs)
        total_skipped += skipped
    except FileNotFoundError:
        print(f"  {fpath}: NOT FOUND, skipping")

print(f"\nTotal distilled: {len(all_pairs):,} pairs")
print(f"Total skipped:   {total_skipped:,} (below conf threshold or malformed)")

# ── Sort by confidence descending ─────────────────────────────
all_pairs.sort(key=lambda x: x[2], reverse=True)

# Deduplicate by question (keep highest-confidence)
seen_questions = set()
deduped = []
for q, a, conf in all_pairs:
    if q not in seen_questions:
        seen_questions.add(q)
        deduped.append((q, a, conf))

print(f"After dedup:     {len(deduped):,} unique Q&A pairs")

# ── Convert to prose lines ─────────────────────────────────────
distilled_prose = distilled_to_prose(deduped)

# ── Load anchor files (front-load for vocab seeding) ──────────
all_anchors = []
for fpath in ANCHOR_FILES:
    try:
        lines = load_prose(fpath)
        all_anchors.extend(lines)
    except FileNotFoundError:
        pass

print(f"Anchor lines:    {len(all_anchors):,} (vocab seeding)")

# ── Boost high-confidence distilled pairs ─────────────────────
# Pairs with conf >= 9.0 get 3x repetition (high-quality teacher signal)
# Pairs with conf 7.0-8.9 get 2x
# Pairs below 7.0 get 1x
high_conf = [(q,a,c) for q,a,c in deduped if c >= 9.0]
mid_conf  = [(q,a,c) for q,a,c in deduped if 7.0 <= c < 9.0]
low_conf  = [(q,a,c) for q,a,c in deduped if c < 7.0]

boosted = []
for q,a,_ in high_conf:
    boosted.extend([q,a,q,a,q,a])   # 3x
for q,a,_ in mid_conf:
    boosted.extend([q,a,q,a])        # 2x
for q,a,_ in low_conf:
    boosted.extend([q,a])            # 1x

print(f"Boosted lines:   {len(boosted):,} (conf-weighted)")
print(f"  high (>=9.0):  {len(high_conf):,} pairs × 3 = {len(high_conf)*6:,} lines")
print(f"  mid  (7-8.9):  {len(mid_conf):,} pairs × 2 = {len(mid_conf)*4:,} lines")
print(f"  low  (<7.0):   {len(low_conf):,} pairs × 1 = {len(low_conf)*2:,} lines")

# ── Interleave through MARCO corpus ───────────────────────────
random.seed(42)  # reproducible
random.shuffle(boosted)  # shuffle so patterns don't cluster

chunk_size = len(MARCO) // 10
chunks = [MARCO[i:i+chunk_size] for i in range(0, len(MARCO), chunk_size)]
anc_per = len(boosted) // len(chunks)

# Front: vocab seeding (anchors first)
out = list(all_anchors)

# Then inject distilled throughout MARCO
idx = 0
for chunk in chunks:
    out.extend(chunk)
    out.extend(boosted[idx:idx+anc_per])
    idx += anc_per
out.extend(boosted[idx:])  # remainder

print(f"\nFinal corpus:    {len(out):,} lines")
print(f"  Vocab seeding: {len(all_anchors):,} (anchors)")
print(f"  MARCO:         {len(MARCO):,}")
print(f"  Distilled:     {len(boosted):,} (conf-weighted)")

# Write LF-only
with open('training/merged_v12_prose.txt', 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

print(f"\nWritten: training/merged_v12_prose.txt")
print(f"Next: train v12 model")
