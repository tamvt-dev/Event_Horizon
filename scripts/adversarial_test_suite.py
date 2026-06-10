#!/usr/bin/env python3
"""
Adversarial Test Suite — EH-G3 v15 evaluation
200+ questions across 7 categories.

Grading:
  pass    = any keyword matches (\b word boundary \b)
  partial = domain_hint keyword matches but no main keyword
  fail    = no match

Metrics reported:
  in_dist_accuracy     (categories: in_dist_tech, in_dist_geo, in_dist_sci, in_dist_people)
  ood_accuracy         (category: ood)
  adversarial_accuracy (categories: paraphrase, multi_token)
  overall              = (in_dist + ood + adversarial) / total  [excl. multi_hop, negation]
  partial_rate         = partial / total  [report-only, NOT in accuracy]
  multi_hop_rate       = informational ceiling
  negation_behavior    = failure mode documentation
"""

from dataclasses import dataclass, field
from typing import List, Optional
import re
import subprocess
import sys

PROJECT   = "/mnt/c/Users/Administrator/Desktop/my_project/eventhorizon"
QUERY_BIN = f"{PROJECT}/examples/eh_query"
RETRIEVE_BIN = f"{PROJECT}/examples/eh_retrieve"
POOL_PATH    = f"{PROJECT}/training/answer_pool.txt"

# ── Category constants ────────────────────────────────────────
C_TECH    = "in_dist_tech"
C_GEO     = "in_dist_geo"
C_SCI     = "in_dist_sci"
C_PEOPLE  = "in_dist_people"
C_OOD     = "ood"
C_PARA    = "paraphrase"
C_MULTI   = "multi_token"
C_HOP     = "multi_hop"     # expected=fail, excluded from accuracy
C_NEG     = "negation"      # expected=fail, excluded from accuracy

EXCLUDED = {C_HOP, C_NEG}

@dataclass
class TC:
    question:     str
    keywords:     List[str]           # \b match = pass
    category:     str
    domain_hints: List[str] = field(default_factory=list)  # partial credit
    expected:     str = "pass"        # pass / fail

# ─────────────────────────────────────────────────────────────
# TEST CASES
# Keywords: word-boundary matched, non-trivial, not substring
# of training anchor text verbatim.
# ─────────────────────────────────────────────────────────────
TEST_SUITE: List[TC] = [

    # ══════════════════════════════════════════════════════════
    # IN-DIST TECH  (40 questions)
    # ══════════════════════════════════════════════════════════

    # — definitions (varied phrasing) —
    TC("what is a computer",              ["processor","electronic","machine","processes"],    C_TECH, ["computer"]),
    TC("what is a database",              ["organized","collection","stored","query","retrieves"], C_TECH, ["database"]),
    TC("what is python",                  ["programming","language","script","software"],      C_TECH, ["python"]),
    TC("what is javascript",              ["web","scripting","language","browser"],            C_TECH, ["javascript"]),
    TC("what is html",                    ["markup","web","pages","language"],                 C_TECH, ["html"]),
    TC("what is an algorithm",            ["steps","procedure","problem","solves","sequence"], C_TECH, ["algorithm"]),
    TC("what is machine learning",        ["learns","data","patterns","neural","ai"],          C_TECH, ["learning"]),
    TC("what is artificial intelligence", ["simulates","thinking","computers","human"],        C_TECH, ["intelligence","ai"]),
    TC("what is a neural network",        ["model","brain","neurons","layers","inspired"],     C_TECH, ["neural"]),
    TC("what is deep learning",           ["multilayer","networks","patterns","neural"],       C_TECH, ["learning"]),
    TC("what is a compiler",              ["translates","source","machine","code","executable"], C_TECH, ["compiler"]),
    TC("what is an operating system",     ["manages","hardware","software","resources"],       C_TECH, ["system","os"]),
    TC("what is encryption",              ["secure","converts","unreadable","format"],         C_TECH, ["encryption"]),
    TC("what is a variable",              ["stores","value","named","location"],               C_TECH, ["variable"]),
    TC("what is a function",              ["reusable","block","code","performs","task"],       C_TECH, ["function"]),
    TC("what is an array",                ["ordered","collection","elements","sequence"],      C_TECH, ["array"]),
    TC("what is a loop",                  ["repeats","code","iteration","multiple"],           C_TECH, ["loop"]),
    TC("what is a class",                 ["blueprint","objects","programming","template"],    C_TECH, ["class"]),
    TC("what is recursion",               ["calls","itself","function","repeatedly"],          C_TECH, ["recursion"]),
    TC("what is debugging",               ["finding","fixing","errors","faults"],              C_TECH, ["debugging","debug"]),
    TC("what is version control",         ["tracks","changes","history","code"],               C_TECH, ["version","control"]),
    TC("what is git",                     ["distributed","version","control","tracking"],      C_TECH, ["git"]),
    TC("what is cloud computing",         ["services","internet","remote","online"],           C_TECH, ["cloud"]),
    TC("what is a cpu",                   ["processing","unit","central","processor"],         C_TECH, ["cpu","processor"]),
    TC("what is ram",                     ["memory","temporary","programs","running"],         C_TECH, ["ram","memory"]),
    TC("what is a cache",                 ["frequently","accessed","faster","retrieval"],      C_TECH, ["cache"]),
    TC("what is a firewall",              ["blocks","unauthorized","network","traffic"],       C_TECH, ["firewall"]),
    TC("what is a bug",                   ["error","flaw","software","unexpected"],            C_TECH, ["bug"]),
    TC("what is open source",             ["publicly","available","source","code"],            C_TECH, ["source"]),
    TC("what is a blockchain",            ["distributed","ledger","transactions","digital"],   C_TECH, ["blockchain"]),
    TC("what is a stack",                 ["last","first","data","structure","lifo"],          C_TECH, ["stack"]),
    TC("what is a queue",                 ["first","fifo","data","structure"],                 C_TECH, ["queue"]),
    TC("what is bandwidth",               ["transfer","rate","network","maximum"],             C_TECH, ["bandwidth"]),
    TC("what is latency",                 ["delay","transmission","network","time"],           C_TECH, ["latency"]),
    TC("what is an api",                  ["interface","programs","communicate","allows"],     C_TECH, ["api"]),
    TC("what is a server",                ["provides","services","clients","data"],            C_TECH, ["server"]),
    TC("what is a network",               ["connects","computers","share","devices"],          C_TECH, ["network"]),
    TC("what is the internet",            ["global","network","connecting","worldwide"],       C_TECH, ["internet"]),
    TC("what is a pixel",                 ["smallest","unit","digital","image"],               C_TECH, ["pixel"]),
    TC("what is binary",                  ["zeros","ones","number","system"],                  C_TECH, ["binary"]),

    # ══════════════════════════════════════════════════════════
    # IN-DIST GEO  (30 questions)
    # ══════════════════════════════════════════════════════════
    TC("what is the capital of france",   ["paris"],                                           C_GEO,  ["france"]),
    TC("what is the capital of germany",  ["berlin"],                                          C_GEO,  ["germany"]),
    TC("what is the capital of japan",    ["tokyo"],                                           C_GEO,  ["japan"]),
    TC("what is the capital of italy",    ["rome"],                                            C_GEO,  ["italy"]),
    TC("what is the capital of spain",    ["madrid"],                                          C_GEO,  ["spain"]),
    TC("what is the capital of china",    ["beijing"],                                         C_GEO,  ["china"]),
    TC("what is the capital of russia",   ["moscow"],                                          C_GEO,  ["russia"]),
    TC("what is the capital of brazil",   ["brasilia"],                                        C_GEO,  ["brazil"]),
    TC("what is the capital of canada",   ["ottawa"],                                          C_GEO,  ["canada"]),
    TC("what is the capital of australia",["canberra"],                                        C_GEO,  ["australia"]),
    TC("what is the capital of india",    ["delhi","new delhi"],                               C_GEO,  ["india"]),
    TC("what is the capital of mexico",   ["mexico city"],                                     C_GEO,  ["mexico"]),
    TC("what is the capital of egypt",    ["cairo"],                                           C_GEO,  ["egypt"]),
    TC("what is the capital of south korea", ["seoul"],                                        C_GEO,  ["korea"]),
    TC("what is the capital of turkey",   ["ankara"],                                          C_GEO,  ["turkey"]),
    TC("what is the capital of argentina",["buenos aires"],                                    C_GEO,  ["argentina"]),
    TC("what is the capital of poland",   ["warsaw"],                                          C_GEO,  ["poland"]),
    TC("what is the capital of sweden",   ["stockholm"],                                       C_GEO,  ["sweden"]),
    TC("what is the capital of greece",   ["athens"],                                          C_GEO,  ["greece"]),
    TC("what is the capital of portugal", ["lisbon"],                                          C_GEO,  ["portugal"]),
    TC("where is paris",                  ["france","french","seine"],                         C_GEO,  ["paris"]),
    TC("where is mount everest",          ["nepal","himalayas"],                               C_GEO,  ["everest","mountain"]),
    TC("what is france",                  ["country","europe","paris"],                        C_GEO,  ["france"]),
    TC("what is japan",                   ["country","asia","tokyo"],                          C_GEO,  ["japan"]),
    TC("what is germany",                 ["country","europe","berlin"],                       C_GEO,  ["germany"]),
    TC("what is the largest country",     ["russia"],                                          C_GEO,  ["largest","country"]),
    TC("what is the longest river",       ["nile"],                                            C_GEO,  ["river","longest"]),
    TC("what is the highest mountain",    ["everest"],                                         C_GEO,  ["mountain","highest"]),
    TC("what is the largest ocean",       ["pacific"],                                         C_GEO,  ["ocean"]),
    TC("what is the largest continent",   ["asia"],                                            C_GEO,  ["continent"]),

    # ══════════════════════════════════════════════════════════
    # IN-DIST SCIENCE  (20 questions)
    # ══════════════════════════════════════════════════════════
    TC("what is gravity",                 ["force","attracts","mass","pulls"],                 C_SCI,  ["gravity"]),
    TC("what is photosynthesis",          ["plants","sunlight","convert","food"],              C_SCI,  ["photosynthesis"]),
    TC("what is evolution",               ["species","change","generations","natural"],        C_SCI,  ["evolution"]),
    TC("what is dna",                     ["genetic","molecule","information","carries"],      C_SCI,  ["dna"]),
    TC("what is an atom",                 ["smallest","element","unit","chemical"],            C_SCI,  ["atom"]),
    TC("what is electricity",             ["flow","charge","conductor","electric"],            C_SCI,  ["electricity"]),
    TC("what is a cell",                  ["basic","unit","living","organisms"],               C_SCI,  ["cell"]),
    TC("what is a molecule",              ["atoms","bonded","group","chemical"],               C_SCI,  ["molecule"]),
    TC("what is energy",                  ["capacity","work","cause","change"],                C_SCI,  ["energy"]),
    TC("what is a black hole",            ["gravity","light","region","escaping"],             C_SCI,  ["black","hole"]),
    TC("what is a star",                  ["massive","gas","fusion","light"],                  C_SCI,  ["star"]),
    TC("what is radiation",               ["energy","waves","emitted","electromagnetic"],      C_SCI,  ["radiation"]),
    TC("what is a virus",                 ["microscopic","infects","cells","agent"],           C_SCI,  ["virus"]),
    TC("what is nuclear energy",          ["splitting","fusing","nuclei","atomic"],            C_SCI,  ["nuclear"]),
    TC("what is the solar system",        ["sun","orbits","planets"],                          C_SCI,  ["solar","system"]),
    TC("what is water",                   ["hydrogen","oxygen","compound","essential"],        C_SCI,  ["water"]),
    TC("what is oxygen",                  ["gas","breathing","combustion","essential"],        C_SCI,  ["oxygen"]),
    TC("what is a wave",                  ["transfers","energy","medium","vibration"],         C_SCI,  ["wave"]),
    TC("what is entropy",                 ["disorder","randomness","measure","system"],        C_SCI,  ["entropy"]),
    TC("what is pi",                      ["ratio","circumference","diameter","circle"],       C_SCI,  ["pi"]),

    # ══════════════════════════════════════════════════════════
    # IN-DIST PEOPLE  (20 questions)
    # ══════════════════════════════════════════════════════════
    TC("who is einstein",                 ["physicist","relativity","theory","physics"],       C_PEOPLE, ["einstein"]),
    TC("who is alan turing",              ["mathematician","computer","founded","science"],    C_PEOPLE, ["turing"]),
    TC("who is isaac newton",             ["laws","motion","gravity","discovered"],            C_PEOPLE, ["newton"]),
    TC("who is charles darwin",           ["evolution","natural","selection","species"],       C_PEOPLE, ["darwin"]),
    TC("who is marie curie",              ["radium","polonium","scientist","discovered"],      C_PEOPLE, ["curie","marie"]),
    TC("who is shakespeare",              ["playwright","hamlet","english","wrote"],           C_PEOPLE, ["shakespeare"]),
    TC("who is napoleon",                 ["french","military","emperor","leader"],            C_PEOPLE, ["napoleon"]),
    TC("who is mahatma gandhi",           ["nonviolent","independence","india","resistance"],  C_PEOPLE, ["gandhi"]),
    TC("who is nelson mandela",           ["president","south africa","first","black"],        C_PEOPLE, ["mandela"]),
    TC("who is thomas edison",            ["invented","lightbulb","devices","inventor"],       C_PEOPLE, ["edison"]),
    TC("who is abraham lincoln",          ["president","united states","sixteenth","civil"],   C_PEOPLE, ["lincoln"]),
    TC("who is julius caesar",            ["roman","general","statesman","empire"],            C_PEOPLE, ["caesar"]),
    TC("who is cleopatra",                ["egypt","ruler","ancient","last"],                  C_PEOPLE, ["cleopatra"]),
    TC("who is alexander the great",      ["conquered","ancient","world","military"],          C_PEOPLE, ["alexander"]),
    TC("what is democracy",               ["citizens","vote","government","elect"],            C_PEOPLE, ["democracy"]),
    TC("what is communism",               ["state","property","political","system"],           C_PEOPLE, ["communism"]),
    TC("what is capitalism",              ["private","ownership","markets","economic"],        C_PEOPLE, ["capitalism"]),
    TC("what is world war two",           ["conflict","nineteen","global","war"],              C_PEOPLE, ["war"]),
    TC("what is the renaissance",         ["cultural","revival","europe","fifteenth"],         C_PEOPLE, ["renaissance"]),
    TC("who is the author",               ["writer","books","writes","wrote"],                 C_PEOPLE, ["author"]),

    # ══════════════════════════════════════════════════════════
    # OUT-OF-DISTRIBUTION  (20 questions)
    # Concept words not in trigram_v2_vocab.txt
    # ══════════════════════════════════════════════════════════
    TC("what is jazz",                    ["music","improvisation","rhythm","african"],        C_OOD, ["jazz","musical"]),
    TC("what is sushi",                   ["japanese","rice","fish","food","raw"],             C_OOD, ["sushi","japanese"]),
    TC("what is origami",                 ["paper","folding","japanese","art"],                C_OOD, ["origami","folding","paper"]),
    TC("what is yoga",                    ["exercise","stretching","meditation","practice"],   C_OOD, ["yoga","exercise"]),
    TC("what is chess",                   ["board","game","strategy","pieces","moves"],        C_OOD, ["chess","game","board"]),
    TC("what is a podcast",               ["audio","show","episodes","listening","broadcast"], C_OOD, ["podcast","audio"]),
    TC("what is a vaccine",               ["immune","disease","protection","injection"],       C_OOD, ["vaccine","immune"]),
    TC("what is meditation",              ["mindfulness","focus","mental","calm","practice"],  C_OOD, ["meditation","mind"]),
    TC("what is a tsunami",               ["wave","ocean","earthquake","coastal","giant"],     C_OOD, ["tsunami","wave"]),
    TC("what is cryptocurrency",          ["digital","currency","blockchain","bitcoin"],       C_OOD, ["crypto","currency","digital"]),
    TC("what is a drone",                 ["unmanned","aircraft","flying","remote"],           C_OOD, ["drone","aircraft","flying"]),
    TC("what is caffeine",                ["stimulant","coffee","alertness","chemical"],       C_OOD, ["caffeine","stimulant"]),
    TC("what is a marathon",              ["running","race","kilometers","distance","sport"],  C_OOD, ["marathon","running","race"]),
    TC("what is inflation",               ["prices","rise","economy","purchasing","money"],    C_OOD, ["inflation","prices"]),
    TC("what is a telescope",             ["magnifies","stars","distant","optical","observing"],C_OOD, ["telescope","stars"]),
    TC("what is a symphony",              ["orchestra","musical","composition","movement"],    C_OOD, ["symphony","orchestra","music"]),
    TC("what is a neuron",                ["brain","cell","signal","nerve","transmits"],       C_OOD, ["neuron","nerve","brain"]),
    TC("what is photon",                  ["light","particle","electromagnetic","energy"],     C_OOD, ["photon","light","particle"]),
    TC("what is a hurricane",             ["storm","tropical","rotating","winds","weather"],   C_OOD, ["hurricane","storm","wind"]),
    TC("what is a genome",                ["dna","genes","organism","complete","genetic"],     C_OOD, ["genome","genetic","genes"]),

    # ══════════════════════════════════════════════════════════
    # PARAPHRASE  (15 questions)
    # Same concept, different phrasing — tests generalization
    # ══════════════════════════════════════════════════════════
    TC("define python",                   ["programming","language","software"],               C_PARA, ["python"]),
    TC("what does python mean",           ["programming","language","software"],               C_PARA, ["python"]),
    TC("tell me about python",            ["programming","language","software"],               C_PARA, ["python"]),
    TC("define algorithm",                ["steps","procedure","problem","solves"],            C_PARA, ["algorithm"]),
    TC("what does algorithm mean",        ["steps","procedure","problem","solves"],            C_PARA, ["algorithm"]),
    TC("define gravity",                  ["force","attracts","mass","pulls"],                 C_PARA, ["gravity"]),
    TC("what does gravity mean",          ["force","attracts","mass"],                         C_PARA, ["gravity"]),
    TC("define democracy",                ["citizens","vote","elect","government"],            C_PARA, ["democracy"]),
    TC("define photosynthesis",           ["plants","sunlight","convert","food"],              C_PARA, ["photosynthesis"]),
    TC("define machine learning",         ["learns","data","patterns","ai"],                   C_PARA, ["learning","machine"]),
    TC("what does dna stand for",         ["genetic","molecule","deoxyribonucleic","information"], C_PARA, ["dna"]),
    TC("define a computer",               ["electronic","machine","processor","processes"],    C_PARA, ["computer"]),
    TC("define encryption",               ["secure","converts","format","unreadable"],         C_PARA, ["encryption"]),
    TC("what does a server do",           ["provides","services","clients","data"],            C_PARA, ["server"]),
    TC("what is meant by algorithm",      ["steps","procedure","problem","solves"],            C_PARA, ["algorithm"]),

    # ══════════════════════════════════════════════════════════
    # MULTI-TOKEN CONCEPTS  (15 questions)
    # Concepts that are 2+ words — tests tokenization
    # ══════════════════════════════════════════════════════════
    TC("what is machine learning",        ["learns","data","patterns","neural","ai"],          C_MULTI, ["learning","machine"]),
    TC("what is deep learning",           ["multilayer","networks","patterns"],                C_MULTI, ["learning","deep"]),
    TC("what is version control",         ["tracks","changes","history","code"],               C_MULTI, ["version","control"]),
    TC("what is cloud computing",         ["services","internet","remote","online"],           C_MULTI, ["cloud"]),
    TC("what is open source",             ["publicly","available","source","code"],            C_MULTI, ["source","open"]),
    TC("what is a black hole",            ["gravity","light","region","escaping"],             C_MULTI, ["black","hole"]),
    TC("what is the solar system",        ["sun","orbits","planets"],                          C_MULTI, ["solar"]),
    TC("what is artificial intelligence", ["simulates","thinking","computers","human"],        C_MULTI, ["intelligence","artificial"]),
    TC("what is a neural network",        ["model","brain","neurons","layers"],                C_MULTI, ["neural","network"]),
    TC("what is world war two",           ["conflict","global","nineteen","war"],              C_MULTI, ["war"]),
    TC("what is nuclear energy",          ["splitting","fusing","nuclei","atomic"],            C_MULTI, ["nuclear","energy"]),
    TC("what is a tree data structure",   ["hierarchical","nodes","parent","child"],           C_MULTI, ["tree","structure"]),
    TC("what is binary search",           ["dividing","element","finds","space"],              C_MULTI, ["binary","search"]),
    TC("what is the internet",            ["global","network","connecting","worldwide"],       C_MULTI, ["internet"]),
    TC("what is the industrial revolution",["manufacturing","transformed","eighteenth","century"], C_MULTI, ["industrial","revolution"]),

    # ══════════════════════════════════════════════════════════
    # MULTI-HOP  (10 questions)
    # expected=fail — documents ceiling, excluded from accuracy
    # ══════════════════════════════════════════════════════════
    TC("what is the capital of the country where einstein was born",
       ["berlin","germany"],  C_HOP, ["einstein","germany"], expected="fail"),
    TC("what is the language used by the creator of linux",
       ["c","programming"],   C_HOP, ["linux","linus"], expected="fail"),
    TC("what is the birthplace of the inventor of the telephone",
       ["scotland","edinburgh"], C_HOP, ["bell","telephone"], expected="fail"),
    TC("what is the capital of the country that invented democracy",
       ["athens","greece"],   C_HOP, ["greece","democracy"], expected="fail"),
    TC("what is the language of the country with the eiffel tower",
       ["french","france"],   C_HOP, ["france","paris"], expected="fail"),
    TC("who invented the device used to look at stars",
       ["galileo","telescope","astronomer"], C_HOP, ["telescope","stars"], expected="fail"),
    TC("what is the element discovered by the scientist who discovered radium",
       ["polonium","curie"],  C_HOP, ["curie","radium"], expected="fail"),
    TC("what is the capital of the largest country in the world",
       ["moscow","russia"],   C_HOP, ["russia","moscow"], expected="fail"),
    TC("what is the theory developed by the physicist who won the 1921 nobel prize",
       ["relativity","einstein"], C_HOP, ["einstein","relativity"], expected="fail"),
    TC("what is the operating system created by the creator of linux",
       ["linux","kernel"],    C_HOP, ["linux","torvalds"], expected="fail"),

    # ══════════════════════════════════════════════════════════
    # NEGATION / TRICK  (10 questions)
    # expected=fail — documents failure mode
    # ══════════════════════════════════════════════════════════
    TC("what is not a programming language",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what is not a mammal",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("which country is not in europe",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what cannot do photosynthesis",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what is the opposite of gravity",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what is not an algorithm",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("name something that is not a computer",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what is false about einstein",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what does python not do",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
    TC("what is not stored in ram",
       ["__SHOULD_FAIL__"],   C_NEG, [], expected="fail"),
]


# ─────────────────────────────────────────────────────────────
# KEYWORD VALIDATOR
# ─────────────────────────────────────────────────────────────
def validate_keywords(suite: List[TC], anchor_path: str) -> List[str]:
    """
    For OOD/paraphrase/multi_token categories:
      Check that at least one keyword is NOT a verbatim substring of any anchor.
      (In-dist questions intentionally use anchor-derived keywords — skip those.)
    """
    IN_DIST = {C_TECH, C_GEO, C_SCI, C_PEOPLE}
    try:
        with open(anchor_path, encoding="utf-8", errors="ignore") as f:
            anchors = [l.strip().lower() for l in f if l.strip() and not l.startswith("#")]
    except FileNotFoundError:
        return [f"[WARN] anchor file not found: {anchor_path}"]

    warnings = []
    for tc in suite:
        if tc.category in IN_DIST or tc.category in EXCLUDED:
            continue
        # For adversarial/OOD: warn if ALL keywords are verbatim in anchors
        kws = [kw for kw in tc.keywords if kw != "__SHOULD_FAIL__" and len(kw) > 3]
        if not kws:
            continue
        all_in_anchor = all(
            any(kw.lower() in anchor for anchor in anchors)
            for kw in kws
        )
        if all_in_anchor:
            warnings.append(
                f"  ALL keywords trivially in anchors: {kws} (Q: {tc.question[:50]})"
            )
    return warnings


# ─────────────────────────────────────────────────────────────
# OOD CHECKER
# ─────────────────────────────────────────────────────────────
def check_ood(suite: List[TC], v2_vocab_path: str) -> dict:
    """For OOD questions, verify concept word is not in v2 vocab."""
    try:
        with open(v2_vocab_path, encoding="utf-8", errors="ignore") as f:
            v2 = set(l.strip().lower() for l in f if l.strip() and not l.startswith("#"))
    except FileNotFoundError:
        return {}

    results = {}
    for tc in suite:
        if tc.category != C_OOD:
            continue
        # extract concept = last meaningful word(s) in question
        words = re.sub(r"[^a-z ]", "", tc.question.lower()).split()
        content = [w for w in words if w not in {"what","is","a","an","the","where","who","how","does"}]
        concept = content[-1] if content else words[-1]
        results[tc.question] = (concept, concept not in v2)
    return results


# ─────────────────────────────────────────────────────────────
# INFERENCE
# ─────────────────────────────────────────────────────────────
def run_model(model: str, vocab: str, pairs: str, embed: str) -> dict:
    """
    Run eh_retrieve binary with all questions piped via stdin.
    Falls back to eh_query if pool not available.
    Returns dict: lowercase_question -> answer_text.
    """
    import os
    questions = "\n".join(tc.question for tc in TEST_SUITE) + "\n"

    # Use retrieval binary if pool exists, otherwise beam search
    if os.path.exists(POOL_PATH) and os.path.exists(RETRIEVE_BIN):
        cmd = [RETRIEVE_BIN, model, embed, vocab, POOL_PATH]
    else:
        cmd = [QUERY_BIN, model, embed, vocab, pairs]

    result = subprocess.run(
        cmd,
        input=questions,
        capture_output=True, text=True, cwd=PROJECT,
        timeout=120
    )
    output = result.stdout

    # Parse: "Q: <question> | A: <answer>  [sim=X →canonical]"
    by_q = {}
    pat = re.compile(r'^Q:\s*(.+?)\s*\| A:\s*(.*?)(?:\s*\[.*?\])?$')
    for line in output.splitlines():
        m = pat.match(line.strip())
        if m:
            q = m.group(1).strip().lower()
            a = m.group(2).strip().lower()
            by_q[q] = a
    return by_q
    for line in output.splitlines():
        m = pat.match(line.strip())
        if m:
            q = m.group(1).strip().lower()
            a = m.group(2).strip().lower()
            by_q[q] = a
    return by_q


# ─────────────────────────────────────────────────────────────
# GRADING
# ─────────────────────────────────────────────────────────────
def grade(answer: str, tc: TC) -> str:
    """Returns 'pass', 'partial', or 'fail'.

    Pass conditions (any match):
      1. Main keyword \b match
      2. Domain hint \b match (→ partial only if no main match)

    Keyword relaxation: if answer contains the concept name from the question
    AND at least one domain hint, count as partial (model knows topic, wrong words).
    """
    if not answer:
        return "fail"
    # Main keywords — word boundary match
    for kw in tc.keywords:
        if kw == "__SHOULD_FAIL__":
            continue
        if re.search(rf'\b{re.escape(kw)}\b', answer, re.IGNORECASE):
            return "pass"
    # Domain hints — partial credit
    for hint in tc.domain_hints:
        if re.search(rf'\b{re.escape(hint)}\b', answer, re.IGNORECASE):
            return "partial"
    return "fail"


# ─────────────────────────────────────────────────────────────
# MAIN EVAL
# ─────────────────────────────────────────────────────────────
def evaluate(model: str, vocab: str, pairs: str, embed: str,
             label: str = "model", verbose: bool = True) -> dict:

    answers = run_model(model, vocab, pairs, embed)

    # For questions not in the 20-question hardcoded binary output,
    # we need to check if the binary answered them. The binary only runs
    # its hardcoded 20. For all other test cases, we report "no output".
    cat_pass  = {}
    cat_fail  = {}
    cat_part  = {}
    cat_total = {}
    rows = []

    for tc in TEST_SUITE:
        # normalize question for lookup
        q_key = tc.question.lower()
        answer = answers.get(q_key, "")

        result = grade(answer, tc)
        rows.append((tc, answer, result))

        c = tc.category
        cat_total[c] = cat_total.get(c, 0) + 1
        if result == "pass":
            cat_pass[c]  = cat_pass.get(c, 0) + 1
        elif result == "partial":
            cat_part[c]  = cat_part.get(c, 0) + 1
        else:
            cat_fail[c]  = cat_fail.get(c, 0) + 1

    # ── Print results ─────────────────────────────────────────
    if verbose:
        print(f"\n{'='*68}")
        print(f"  {label}")
        print(f"{'='*68}")

        for category in [C_TECH, C_GEO, C_SCI, C_PEOPLE, C_OOD, C_PARA, C_MULTI, C_HOP, C_NEG]:
            cat_rows = [(tc, a, r) for tc, a, r in rows if tc.category == category]
            if not cat_rows:
                continue
            total   = len(cat_rows)
            passing = sum(1 for _, _, r in cat_rows if r == "pass")
            partial = sum(1 for _, _, r in cat_rows if r == "partial")
            pct     = 100 * passing // total
            bar     = "█" * passing + "▒" * partial + "░" * (total - passing - partial)
            excl    = " [excluded from accuracy]" if category in EXCLUDED else ""
            print(f"\n  ── {category} {pct}% ({passing}/{total}){excl}")
            print(f"     [{bar}]")
            for tc, a, r in cat_rows:
                mark = {"pass": "✅", "partial": "⚠️ ", "fail": "❌"}[r]
                ans_display = (a[:55] + "…") if len(a) > 55 else a
                if not a:
                    ans_display = "[no output]"
                print(f"     {mark} {tc.question:<50} → {ans_display}")

    # ── Aggregate metrics ─────────────────────────────────────
    in_dist_cats = [C_TECH, C_GEO, C_SCI, C_PEOPLE]
    scored_cats  = [C_TECH, C_GEO, C_SCI, C_PEOPLE, C_OOD, C_PARA, C_MULTI]

    def acc(cats):
        p = sum(cat_pass.get(c, 0) for c in cats)
        t = sum(cat_total.get(c, 0) for c in cats)
        return (p, t, 100*p//t if t else 0)

    in_p,  in_t,  in_pct  = acc(in_dist_cats)
    ood_p, ood_t, ood_pct = acc([C_OOD])
    adv_p, adv_t, adv_pct = acc([C_PARA, C_MULTI])
    all_p, all_t, all_pct = acc(scored_cats)

    hop_p,  hop_t,  _  = acc([C_HOP])
    neg_p,  neg_t,  _  = acc([C_NEG])
    part_total = sum(cat_part.get(c, 0) for c in scored_cats)

    if verbose:
        print(f"\n{'='*68}")
        print(f"  SUMMARY — {label}")
        print(f"{'='*68}")
        print(f"  In-distribution  : {in_p:3}/{in_t:3}  = {in_pct:3}%")
        print(f"  Out-of-dist      : {ood_p:3}/{ood_t:3}  = {ood_pct:3}%")
        print(f"  Adversarial      : {adv_p:3}/{adv_t:3}  = {adv_pct:3}%")
        print(f"  ─────────────────────────────")
        print(f"  OVERALL (scored) : {all_p:3}/{all_t:3}  = {all_pct:3}%")
        print(f"  ─────────────────────────────")
        print(f"  Partial (info)   : {part_total:3}/{all_t:3}")
        print(f"  Multi-hop        : {hop_p:3}/{hop_t:3}  [expected fail]")
        print(f"  Negation         : {neg_p:3}/{neg_t:3}  [expected fail]")
        print(f"\n  Target: overall >= 80%  → {'✅ PASS' if all_pct >= 80 else '❌ FAIL'}")

    return {
        "label":       label,
        "in_dist":     in_pct,
        "ood":         ood_pct,
        "adversarial": adv_pct,
        "overall":     all_pct,
        "partial":     part_total,
        "rows":        rows,
    }


# ─────────────────────────────────────────────────────────────
# COMPARISON TABLE
# ─────────────────────────────────────────────────────────────
def compare(results: list):
    print(f"\n{'='*68}")
    print(f"  COMPARISON TABLE")
    print(f"{'='*68}")
    print(f"  {'Model':<25} {'In-dist':>8} {'OOD':>6} {'Advers':>8} {'Overall':>9}")
    print(f"  {'-'*25} {'-'*8} {'-'*6} {'-'*8} {'-'*9}")
    for r in results:
        target = " ✅" if r["overall"] >= 80 else " ❌"
        print(f"  {r['label']:<25} {r['in_dist']:>7}% {r['ood']:>5}% {r['adversarial']:>7}% {r['overall']:>8}%{target}")


# ─────────────────────────────────────────────────────────────
# ENTRY POINT
# ─────────────────────────────────────────────────────────────
def main():
    import argparse
    parser = argparse.ArgumentParser(description="EH-G3 Adversarial Eval")
    parser.add_argument("--validate-only", action="store_true",
                        help="Only run keyword/OOD validation, no inference")
    parser.add_argument("--models", nargs="+",
                        default=["v15"],
                        help="Model versions to evaluate (e.g. v11 v15)")
    args = parser.parse_args()

    anchor_path  = f"{PROJECT}/training/definition_anchors.txt"
    v2_vocab     = f"{PROJECT}/training/trigram_v2_vocab.txt"

    # ── Keyword validation ────────────────────────────────────
    print("\n── Keyword Validation ───────────────────────────────")
    warnings = validate_keywords(TEST_SUITE, anchor_path)
    if warnings:
        print(f"  {len(warnings)} potential overlap(s):")
        for w in warnings[:20]:
            print(w)
        if len(warnings) > 20:
            print(f"  ... and {len(warnings)-20} more")
    else:
        print("  All keywords OK (no anchor overlap)")

    # ── OOD verification ──────────────────────────────────────
    print("\n── OOD Verification ─────────────────────────────────")
    ood_check = check_ood(TEST_SUITE, v2_vocab)
    confirmed_ood = sum(1 for _, is_ood in ood_check.values() if is_ood)
    in_v2 = [(q, c) for q, (c, is_ood) in ood_check.items() if not is_ood]
    print(f"  OOD confirmed (not in v2 vocab): {confirmed_ood}/{len(ood_check)}")
    if in_v2:
        print(f"  In v2 vocab (may not be truly OOD):")
        for q, c in in_v2:
            print(f"    '{c}' in question: {q}")

    total_questions = len(TEST_SUITE)
    scored = [tc for tc in TEST_SUITE if tc.category not in EXCLUDED]
    print(f"\n── Test Suite Stats ─────────────────────────────────")
    print(f"  Total questions   : {total_questions}")
    print(f"  Scored questions  : {len(scored)}")
    print(f"  Excluded (info)   : {total_questions - len(scored)}")
    for cat in [C_TECH, C_GEO, C_SCI, C_PEOPLE, C_OOD, C_PARA, C_MULTI, C_HOP, C_NEG]:
        n = sum(1 for tc in TEST_SUITE if tc.category == cat)
        excl = " [excl]" if cat in EXCLUDED else ""
        print(f"    {cat:<20}: {n}{excl}")

    if args.validate_only:
        return

    # ── Run inference ─────────────────────────────────────────
    all_results = []
    for ver in args.models:
        model = f"training/trigram_{ver}_model.ehdag"
        vocab = f"training/trigram_{ver}_vocab.txt"
        pairs = f"training/trigram_{ver}_vocab.txt.pairs"
        embed = "training/eh_g3_embeddings.bin"
        print(f"\n\n{'#'*68}")
        print(f"# Evaluating {ver}")
        print(f"{'#'*68}")
        r = evaluate(model, vocab, pairs, embed, label=ver)
        all_results.append(r)

    if len(all_results) > 1:
        compare(all_results)


if __name__ == "__main__":
    main()
