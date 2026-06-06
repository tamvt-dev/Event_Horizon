#!/usr/bin/env python3
"""
scripts/rebuild_vocab.py — Rebuild vocab từ corpus, merge với vocab cũ.

Usage:
    python scripts/rebuild_vocab.py \
        --corpus training/p0_corpus_expansion.txt \
        --old-vocab training/trigram_v2_vocab.txt \
        --out training/trigram_v2_vocab.txt \
        [--min-freq 1]

Strategy:
    1. Extract tất cả words từ corpus mới
    2. Merge với vocab cũ (giữ nguyên token IDs cũ để không break existing model)
    3. Append các words mới vào cuối
    4. Verify coverage >= 90%
"""

import re
import sys
import argparse
from collections import Counter
from pathlib import Path


def extract_words(text: str) -> list[str]:
    text = text.lower()
    text = re.sub(r"[^a-z0-9]+", " ", text)
    return [w for w in text.split() if w]


def load_vocab(path: str) -> list[str]:
    """Load vocab, trả về list giữ nguyên thứ tự (index = token ID)."""
    words = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            tok = line.strip()
            if tok:
                words.append(tok)
    return words


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus",    required=True, help="Corpus text file")
    parser.add_argument("--old-vocab", required=True, help="Existing vocab file")
    parser.add_argument("--out",       required=True, help="Output vocab file")
    parser.add_argument("--min-freq",  type=int, default=1,
                        help="Min frequency để thêm word mới (default: 1)")
    parser.add_argument("--dry-run",   action="store_true",
                        help="Chỉ report, không ghi file")
    args = parser.parse_args()

    # -- Load corpus --
    corpus_path = Path(args.corpus)
    if not corpus_path.exists():
        print(f"[ERROR] Corpus not found: {args.corpus}")
        sys.exit(1)

    with open(corpus_path, "r", encoding="utf-8") as f:
        corpus_text = f.read()

    corpus_words = extract_words(corpus_text)
    corpus_freq  = Counter(corpus_words)
    corpus_unique = set(corpus_words)

    print(f"[INFO] Corpus: {len(corpus_words)} tokens, {len(corpus_unique)} unique")

    # -- Load old vocab --
    old_vocab_path = Path(args.old_vocab)
    if old_vocab_path.exists():
        old_vocab = load_vocab(args.old_vocab)
        old_vocab_set = set(old_vocab)
        print(f"[INFO] Old vocab: {len(old_vocab)} tokens")
    else:
        old_vocab     = []
        old_vocab_set = set()
        print(f"[WARN] Old vocab not found — building from scratch")

    # -- Tìm words mới cần thêm --
    new_words = []
    for word, freq in sorted(corpus_freq.items(), key=lambda x: -x[1]):
        if word not in old_vocab_set and freq >= args.min_freq:
            new_words.append(word)

    print(f"[INFO] New words to add: {len(new_words)}")
    if new_words:
        print(f"[INFO] Sample new words: {new_words[:20]}")

    # -- Merged vocab --
    merged_vocab = old_vocab + new_words

    # -- Coverage check --
    merged_set = set(merged_vocab)
    hits   = sum(1 for w in corpus_unique if w in merged_set)
    coverage = hits / max(1, len(corpus_unique))

    print(f"\n=== Coverage Report ===")
    print(f"  Old vocab size    : {len(old_vocab)}")
    print(f"  New words added   : {len(new_words)}")
    print(f"  Merged vocab size : {len(merged_vocab)}")
    print(f"  Corpus unique     : {len(corpus_unique)}")
    print(f"  Coverage (unique) : {coverage*100:.2f}%")

    # Còn missing không?
    still_missing = [w for w in corpus_unique if w not in merged_set]
    if still_missing:
        print(f"  Still missing     : {still_missing}")
    else:
        print(f"  Still missing     : 0 ✓")

    if coverage >= 0.90:
        print(f"\nRESULT: PASS (>= 90%)")
    else:
        print(f"\nRESULT: FAIL (< 90%) — tăng --min-freq xuống 1")

    # -- Ghi file --
    if args.dry_run:
        print(f"\n[DRY RUN] Would write {len(merged_vocab)} tokens to {args.out}")
        return

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Backup vocab cũ
    if out_path.exists() and out_path == old_vocab_path:
        backup = out_path.with_suffix(".txt.bak")
        import shutil
        shutil.copy2(out_path, backup)
        print(f"[INFO] Backed up old vocab → {backup}")

    with open(out_path, "w", encoding="utf-8") as f:
        for word in merged_vocab:
            f.write(word + "\n")

    print(f"[OK] Written {len(merged_vocab)} tokens → {args.out}")


if __name__ == "__main__":
    main()