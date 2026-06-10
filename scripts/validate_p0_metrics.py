#!/usr/bin/env python3
"""
Validate P0 metrics on the 20-question benchmark.
Runs qa_attention_g3 (Phase 2 / G3 config) and checks keyword matches.

Usage:
    python scripts/validate_p0_metrics.py \
        training/trigram_v14_model.ehdag \
        training/trigram_v14_vocab.txt \
        training/trigram_v14_vocab.txt.pairs \
        training/eh_g3_embeddings.bin
"""

import subprocess
import sys
import re
from dataclasses import dataclass, field
from typing import List

# ── Test suite (20 questions) ─────────────────────────────────
@dataclass
class TestCase:
    question: str
    keywords: List[str]   # any match = correct
    domain: str
    split: str = "train"

TEST_CASES = [
    # Hub collision cases
    TestCase("what is a computer",            ["computer","electronic","machine","processes","device","processor"],   "technical"),
    TestCase("what is a database",            ["database","data","organized","collection","stores","stored","query"], "technical"),
    TestCase("what is a tree",                ["tree","plant","trunk","branches","leaves","grows","woody","oxygen","shade","wood"], "nature"),
    # Baseline
    TestCase("what is your name",             ["name","my","assistant"],                                   "baseline"),
    TestCase("how are you",                   ["well","fine","good","doing","am","help"],                  "baseline"),
    TestCase("hello",                         ["hello","hi","help","greet"],                               "baseline"),
    # Geography
    TestCase("what is the capital of france", ["paris","france"],                                          "geography"),
    TestCase("what is the capital of japan",  ["tokyo","japan"],                                           "geography"),
    TestCase("what is the capital of germany",["berlin","germany"],                                        "geography"),
    TestCase("where is paris",                ["paris","france","city"],                                   "geography"),
    # Technical
    TestCase("what is python",                ["python","programming","language","script"],                "technical"),
    TestCase("what is an algorithm",          ["algorithm","step","procedure","problem","solves","sequence"], "technical"),
    TestCase("what is a network",             ["network","computers","connect","communication","devices","share"], "technical"),
    TestCase("what is machine learning",      ["learning","machine","data","ai","system","neural","algorithms"], "technical"),
    # Science
    TestCase("what is gravity",               ["gravity","force","attracts","mass"],                       "science"),
    TestCase("what is photosynthesis",        ["photosynthesis","plants","sunlight","food","convert"],     "science"),
    TestCase("what is dna",                   ["dna","genetic","molecule","information"],                  "science"),
    # People / literary
    TestCase("who is the author",             ["author","writer","wrote","books","writes"],                "literary",  "holdout"),
    TestCase("who is einstein",               ["einstein","physicist","relativity","theory","physics"],    "people",    "holdout"),
    TestCase("who is alan turing",            ["turing","computer","mathematician","science","foundation"], "people",    "holdout"),
]

PROJECT = "/mnt/c/Users/Administrator/Desktop/my_project/eventhorizon"


def run_all_inference(model, vocab, pairs, embed):
    """Run the binary once, capture all [N] Q:...|A: lines from Phase 2."""
    import shutil
    wsl_exe = shutil.which("wsl") or r"C:\Windows\System32\wsl.exe"
    cmd = [
        wsl_exe, "bash", "-c",
        f"cd {PROJECT} && "
        f"./examples/qa_attention_g3 {model} {embed} {vocab} {pairs} 2>/dev/null | "
        f"grep -E '^\\[[ ]*[0-9]+\\] Q:.*\\| A:'"
    ]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return r.stdout
    except Exception as e:
        print(f"[ERROR running binary: {e}]")
        return ""


def parse_answers(raw_output):
    """
    Parse output lines like:
      [ 1] Q: what is a computer        | A: computer is an electronic machine
    Returns dict: question_text -> answer_text (last occurrence = Phase 2)
    """
    answers = {}
    pattern = re.compile(r'^\[[ ]*(\d+)\] Q:\s*(.+?)\s*\| A:\s*(.+)$')
    for line in raw_output.splitlines():
        m = pattern.match(line.strip())
        if m:
            idx = int(m.group(1))
            q   = m.group(2).strip().lower()
            a   = m.group(3).strip().lower()
            answers[idx] = (q, a)  # overwrite → last = Phase 2
    return answers


def check_answer(answer, keywords):
    al = answer.lower()
    return any(kw.lower() in al for kw in keywords)


def main():
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <model> <vocab> <pairs> <embed>")
        sys.exit(1)

    model, vocab, pairs, embed = sys.argv[1:5]

    print(f"\n{'='*60}")
    print(f"P0 Validation — EH-G3 Q&A Benchmark (20 questions)")
    print(f"{'='*60}")
    print(f"Model : {model}")
    print(f"Vocab : {vocab}")
    print(f"Pairs : {pairs}")
    print(f"{'='*60}\n")

    raw = run_all_inference(model, vocab, pairs, embed)
    if not raw:
        print("[ERROR] No output from inference binary.")
        sys.exit(1)

    by_idx = parse_answers(raw)

    results = []
    domain_counts  = {}
    domain_correct = {}

    for i, tc in enumerate(TEST_CASES):
        idx = i + 1
        if idx in by_idx:
            _, answer = by_idx[idx]
        else:
            answer = ""

        correct = check_answer(answer, tc.keywords)
        mark    = "✅" if correct else "❌"
        print(f"{mark} Q: {tc.question:<45} A: {answer[:60]}")

        results.append((tc, answer, correct))
        domain_counts[tc.domain]  = domain_counts.get(tc.domain, 0) + 1
        if correct:
            domain_correct[tc.domain] = domain_correct.get(tc.domain, 0) + 1

    # ── Summary ───────────────────────────────────────────────
    print(f"\n{'='*60}")
    print("Domain Breakdown:")
    for d in sorted(domain_counts):
        total   = domain_counts[d]
        correct = domain_correct.get(d, 0)
        bar     = "█" * correct + "░" * (total - correct)
        print(f"  {d:<12}: {correct}/{total} ({100*correct//total}%) [{bar}]")

    total_correct = sum(1 for _, _, ok in results if ok)
    total_q       = len(TEST_CASES)
    accuracy      = 100.0 * total_correct / total_q

    print(f"\n{'='*60}")
    print("Overall Results:")
    print(f"  Total questions : {total_q}")
    print(f"  Correct         : {total_correct}")
    print(f"  Accuracy        : {accuracy:.1f}%")

    print(f"\nP0 Targets:")
    status = "✅" if accuracy >= 40 else "❌"
    print(f"  Accuracy >= 40% : {status} ({accuracy:.1f}%)")

    print(f"\nVersion Progression:")
    print(f"  v11: 80% (16/20)")
    print(f"  v12: 75% (regression)")
    print(f"  v13: 80% (filtered)")
    print(f"  v14: {accuracy:.1f}% ({total_correct}/{total_q}) ← current")

    return 0 if accuracy >= 40 else 1


if __name__ == "__main__":
    sys.exit(main())
