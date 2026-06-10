#!/usr/bin/env python3
"""
Parse qa_output.txt and convert to divergence_v6.txt prose pairs.

Handles two formats:
  Layer 1: concept: X \n q:... \n a: ...
  Layer 2: q:... (no concept label) \n a: ...
"""
import re
from pathlib import Path

src = Path("training/qa_output.txt")
out = Path("training/divergence_v6.txt")

content = src.read_text(encoding="utf-8", errors="ignore")
blocks = [b.strip() for b in content.split("--------------------") if b.strip()]

concepts = []
for block in blocks:
    lines = [l.strip() for l in block.splitlines() if l.strip()]
    if not lines:
        continue

    concept = None
    questions = []
    answer = None

    for line in lines:
        if line.startswith("concept:"):
            concept = line[8:].strip()
        elif line.startswith("q:"):
            questions.append(line[2:].strip())
        elif line.startswith("a:"):
            answer = line[2:].strip()

    # Layer 2: no concept label — derive from first question or leave generic
    if not concept and questions:
        concept = "misc"

    if questions and answer:
        concepts.append((concept or "misc", questions, answer))

print(f"Parsed {len(concepts)} concepts, {sum(len(q) for _,q,_ in concepts)} questions total")

# Build prose pairs
# For each concept: each question + answer = one prose pair
# Truncate answer to ≤12 words for training
out_lines = []
out_lines.append(f"# Divergence v6 — qa_output.txt converted")
out_lines.append(f"# {len(concepts)} concepts, {sum(len(q) for _,q,_ in concepts)} Q/A pairs")
out_lines.append(f"# Weight: x30 in corpus (targeted fix for 4 super-paths)")
out_lines.append("")

for concept, questions, answer in concepts:
    out_lines.append(f"# ── {concept} ──")

    # Normalize answer: lowercase, strip punctuation, max 12 words
    ans_clean = answer.lower().strip()
    ans_clean = re.sub(r'[^\x20-\x7e]', ' ', ans_clean)
    ans_clean = re.sub(r'\s+', ' ', ans_clean).strip()
    ans_words = ans_clean.split()
    if len(ans_words) > 12:
        ans_clean = ' '.join(ans_words[:12])

    # Normalize each question
    for q in questions:
        q_clean = q.lower().strip()
        q_clean = re.sub(r'[^\x20-\x7e]', ' ', q_clean)
        q_clean = re.sub(r'[?]', '', q_clean).strip()
        q_clean = re.sub(r'\s+', ' ', q_clean).strip()

        if len(q_clean) < 5 or len(ans_clean) < 3:
            continue

        out_lines.append(q_clean)
        out_lines.append(ans_clean)

    out_lines.append("")

# Write LF-only
out.write_text('\n'.join(out_lines) + '\n', encoding='utf-8', newline='\n' if hasattr(out, 'write_text') else None)

# Manual LF write
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out_lines) + '\n')

print(f"Written: {out}")
print(f"Lines: {len(out_lines)}")

# Print sample
print("\nSample (first concept):")
for line in out_lines[4:20]:
    print(f"  {line}")
