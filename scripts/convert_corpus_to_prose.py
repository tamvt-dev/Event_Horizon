#!/usr/bin/env python3
"""
Convert tab-separated Q&A corpus to prose format for train_trigram.c

Input format (tab-separated):
    what is a computer\ta computer is an electronic device that processes data

Output format (prose, one sentence per line):
    what is a computer
    a computer is an electronic device that processes data
"""

import sys
import os

def convert(input_path: str, output_path: str):
    pairs_written = 0
    skipped = 0

    with open(input_path, "r", encoding="utf-8", errors="ignore") as fin, \
         open(output_path, "w", encoding="utf-8") as fout:

        for raw_line in fin:
            line = raw_line.strip()

            # Skip comments and empty lines
            if not line or line.startswith("#"):
                continue

            # Tab-separated: question TAB answer
            if "\t" in line:
                parts = line.split("\t", 1)
                if len(parts) == 2:
                    q, a = parts[0].strip(), parts[1].strip()
                    if q and a:
                        fout.write(q + "\n")
                        fout.write(a + "\n")
                        pairs_written += 1
                    else:
                        skipped += 1
            else:
                # Already prose — write as-is (questions like "Q: ..." style)
                if line.startswith("Q: "):
                    fout.write(line[3:] + "\n")
                elif line.startswith("A: "):
                    fout.write(line[3:] + "\n")
                else:
                    # Plain prose line, write directly
                    fout.write(line + "\n")
                    pairs_written += 1  # count as one unit

    print(f"Converted: {pairs_written} Q&A pairs → {output_path}")
    print(f"Skipped:   {skipped} malformed lines")
    print(f"Output has ~{pairs_written * 2} lines (each pair = 2 lines)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_tab.txt> <output_prose.txt>")
        sys.exit(1)

    convert(sys.argv[1], sys.argv[2])
