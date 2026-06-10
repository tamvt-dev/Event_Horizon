#!/usr/bin/env python3
"""
Extract MS MARCO session TSV files into prose corpus for train_trigram.c

MS MARCO Session format (tab-separated):
    session_id  query1  query2  query3  ...

Each session = multiple related queries from one user.
We extract individual queries as sentences AND consecutive
query pairs as Q→Q context (natural language transitions).

Strategy:
  1. Extract all unique queries as individual sentences
  2. Write consecutive pairs within session as Q/A-like context
  3. Filter: keep only short, clean queries (≤10 words, no special chars)
  4. Deduplicate across sessions

Usage:
    python scripts/extract_marco_tsv.py [--max-pairs N] [--filter-words N] \
        training/marco_ann_session.dev.all.tsv \
        training/marco_prose_corpus.txt

    # Use smaller file first for testing:
    python scripts/extract_marco_tsv.py \
        training/marco_ann_session.dev.half_specify.tsv \
        training/marco_prose_small.txt
"""

import sys
import re
import argparse
from pathlib import Path


def clean_query(q: str) -> str:
    """Lowercase, strip, remove non-ASCII and special chars."""
    q = q.strip().lower()
    # Remove non-printable and non-ASCII
    q = re.sub(r'[^\x20-\x7e]', ' ', q)
    # Collapse whitespace
    q = re.sub(r'\s+', ' ', q).strip()
    return q


def is_valid(q: str, max_words: int) -> bool:
    """Accept only clean, short, useful queries."""
    if not q or len(q) < 4:
        return False
    words = q.split()
    if len(words) > max_words:
        return False
    # Skip queries that are just numbers or codes
    if re.match(r'^[\d\s\+\-\*\/\.\,\%]+$', q):
        return False
    # Skip medical codes, ICD codes, etc.
    if re.match(r'^icd\s*\d', q) or re.match(r'^icd\s*1[0-9]', q):
        return False
    # Must contain at least one real word (≥3 alpha chars)
    if not re.search(r'[a-z]{3,}', q):
        return False
    return True


def extract_corpus(tsv_path: str, output_path: str,
                   max_pairs: int = 5000,
                   max_words: int = 10,
                   mode: str = "both") -> dict:
    """
    Extract prose corpus from MARCO TSV.

    mode:
      "queries"  — one query per line (for vocabulary building)
      "sessions" — consecutive pairs as context lines
      "both"     — both approaches merged
    """
    seen_queries = set()
    session_pairs = []   # (query_a, query_b) consecutive pairs
    all_queries = []     # unique individual queries

    total_sessions = 0
    total_queries = 0
    skipped = 0

    with open(tsv_path, 'rb') as f:
        for raw_line in f:
            line = raw_line.decode('utf-8', errors='ignore').rstrip('\r\n')
            parts = line.split('\t')

            if len(parts) < 2:
                continue

            # First column is session_id, rest are queries
            session_queries = []
            for part in parts[1:]:
                q = clean_query(part)
                if is_valid(q, max_words):
                    session_queries.append(q)
                    total_queries += 1
                else:
                    skipped += 1

            if not session_queries:
                continue

            total_sessions += 1

            # Collect unique queries
            for q in session_queries:
                if q not in seen_queries:
                    seen_queries.add(q)
                    all_queries.append(q)

            # Collect consecutive pairs from session (natural context)
            for i in range(len(session_queries) - 1):
                q_a = session_queries[i]
                q_b = session_queries[i + 1]
                # Only pair queries that share at least one content word
                words_a = set(q_a.split()) - {'what', 'is', 'a', 'an', 'the', 'of', 'in', 'for', 'how', 'why', 'when', 'where', 'who'}
                words_b = set(q_b.split()) - {'what', 'is', 'a', 'an', 'the', 'of', 'in', 'for', 'how', 'why', 'when', 'where', 'who'}
                if words_a & words_b:  # share a content word → likely same topic
                    session_pairs.append((q_a, q_b))

            if len(session_pairs) >= max_pairs * 2:
                break  # enough data

    # Write output
    Path(output_path).parent.mkdir(parents=True, exist_ok=True)
    lines_written = 0

    with open(output_path, 'w', encoding='utf-8') as out:
        out.write(f"# MS MARCO Session corpus — extracted for EH-G3 training\n")
        out.write(f"# Source: {tsv_path}\n")
        out.write(f"# Sessions: {total_sessions}, Unique queries: {len(all_queries)}\n\n")

        if mode in ("queries", "both"):
            out.write("# --- Individual queries (vocabulary building) ---\n")
            for q in all_queries[:max_pairs]:
                out.write(q + "\n")
                lines_written += 1

        if mode in ("sessions", "both"):
            out.write("\n# --- Consecutive session pairs (context transitions) ---\n")
            for q_a, q_b in session_pairs[:max_pairs]:
                out.write(q_a + "\n")
                out.write(q_b + "\n")
                lines_written += 2

    stats = {
        "sessions": total_sessions,
        "unique_queries": len(all_queries),
        "session_pairs": len(session_pairs),
        "lines_written": lines_written,
        "skipped": skipped,
    }
    return stats


def main():
    parser = argparse.ArgumentParser(description="Extract MS MARCO TSV to prose corpus")
    parser.add_argument("input", help="Input TSV file")
    parser.add_argument("output", help="Output prose text file")
    parser.add_argument("--max-pairs", type=int, default=5000,
                        help="Max query pairs to extract (default: 5000)")
    parser.add_argument("--max-words", type=int, default=10,
                        help="Max words per query (default: 10)")
    parser.add_argument("--mode", choices=["queries", "sessions", "both"],
                        default="both", help="Extraction mode (default: both)")
    args = parser.parse_args()

    print(f"\nExtracting MS MARCO TSV corpus...")
    print(f"  Input:     {args.input}")
    print(f"  Output:    {args.output}")
    print(f"  Mode:      {args.mode}")
    print(f"  Max pairs: {args.max_pairs}")
    print(f"  Max words: {args.max_words}")

    stats = extract_corpus(
        args.input, args.output,
        max_pairs=args.max_pairs,
        max_words=args.max_words,
        mode=args.mode
    )

    print(f"\nDone!")
    print(f"  Sessions processed : {stats['sessions']:,}")
    print(f"  Unique queries     : {stats['unique_queries']:,}")
    print(f"  Session pairs      : {stats['session_pairs']:,}")
    print(f"  Lines written      : {stats['lines_written']:,}")
    print(f"  Queries skipped    : {stats['skipped']:,}")
    print(f"\nOutput: {args.output}")
    print(f"\nNext step — merge with existing corpus and retrain:")
    print(f"  python scripts/merge_corpus_v3.py")
    print(f"  wsl bash -c './examples/train_trigram training/merged_v4_prose.txt training/trigram_v4_model.ehdag training/trigram_v4_vocab.txt'")


if __name__ == "__main__":
    main()
