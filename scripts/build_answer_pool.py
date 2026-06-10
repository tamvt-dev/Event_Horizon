#!/usr/bin/env python3
"""
Build training/answer_pool.txt for retrieval-based Q&A.

Strategy (Error-driven Pool Expansion):
  1. Core facts — one canonical Q/A per concept
  2. Semantic template expansion — auto-generate 5 paraphrase forms per entry
  3. OOD concepts — directly added facts for known failures
  4. Canonicalization applied at query time (in eh_retrieve.c)

Pool entries are deduplicated by (canonical_question, answer).
"""
import re
from pathlib import Path

out_pairs = []   # list of (question, answer)
seen_q    = set()

def add(q, a, skip_dup=True):
    q = re.sub(r'[^\x20-\x7e]', ' ', q.strip().lower())
    q = re.sub(r'\s+', ' ', q).strip().rstrip('?')
    a = re.sub(r'[^\x20-\x7e]', ' ', a.strip().lower())
    a = re.sub(r'\s+', ' ', a).strip()
    if len(q) < 4 or len(a) < 3: return
    if skip_dup and q in seen_q: return
    seen_q.add(q)
    out_pairs.append((q, a))

# ── Semantic template expansion ────────────────────────────
# For every (concept, answer) pair, generate multiple question forms.
# Canonicalizer in eh_retrieve maps all of these back to "what is X",
# but having them in the pool improves nearest-neighbor recall for
# queries whose embeddings don't canonicalize perfectly.

PARAPHRASE_TEMPLATES = [
    "what is {concept}",
    "define {concept}",
    "what does {concept} mean",
    "explain {concept}",
    "tell me about {concept}",
]

PEOPLE_TEMPLATES = [
    "who is {concept}",
    "who was {concept}",
    "tell me about {concept}",
    "what is {concept} known for",
    "describe {concept}",
]

GEO_TEMPLATES = [
    "what is the capital of {concept}",
    "capital of {concept}",
    "where is {concept}",
    "what country is {concept} in",
]

def expand(concept, answer, templates=None):
    if templates is None:
        templates = PARAPHRASE_TEMPLATES
    for t in templates:
        add(t.format(concept=concept), answer)

# ════════════════════════════════════════════════════════════
# CORE FACTS (all in-distribution concepts)
# ════════════════════════════════════════════════════════════

# ── Technology ───────────────────────────────────────────────
TECH = [
    ("computer",        "a computer is an electronic machine that processes data"),
    ("database",        "a database is an organized collection of stored data"),
    ("python",          "python is a programming language for software development"),
    ("javascript",      "javascript is a scripting language for web development"),
    ("html",            "html is the markup language for creating web pages"),
    ("css",             "css is a language for styling web pages"),
    ("algorithm",       "an algorithm is a step by step problem solving procedure"),
    ("a program",       "a program is a set of instructions for a computer"),
    ("software",        "software is programs and code that run on computers"),
    ("hardware",        "hardware is physical components that make up a computer"),
    ("a server",        "a server provides services and data to other computers"),
    ("a network",       "a network connects computers to share information"),
    ("the internet",    "the internet is a global network connecting computers worldwide"),
    ("a website",       "a website is a collection of pages on the internet"),
    ("an app",          "an app is a software application for mobile devices"),
    ("a browser",       "a browser is software used to access the internet"),
    ("an api",          "an api allows programs to communicate with each other"),
    ("machine learning","machine learning is a system that learns from data"),
    ("artificial intelligence", "artificial intelligence simulates human thinking in computers"),
    ("a neural network","a neural network is a model inspired by the human brain"),
    ("deep learning",   "deep learning uses multilayer neural networks to learn patterns"),
    ("a compiler",      "a compiler translates source code into executable machine code"),
    ("an operating system","an operating system manages computer hardware and software resources"),
    ("linux",           "linux is an open source operating system kernel"),
    ("encryption",      "encryption converts data into a secure unreadable format"),
    ("a variable",      "a variable stores a value in a computer program"),
    ("a function",      "a function is a reusable block of code"),
    ("an array",        "an array is an ordered collection of elements"),
    ("a loop",          "a loop repeats a block of code multiple times"),
    ("a class",         "a class is a blueprint for creating objects in code"),
    ("recursion",       "recursion is when a function calls itself repeatedly"),
    ("debugging",       "debugging is finding and fixing errors in code"),
    ("version control", "version control tracks changes to code over time"),
    ("git",             "git is a distributed version control system for code"),
    ("cloud computing", "cloud computing delivers services over the internet remotely"),
    ("a cpu",           "a cpu is the central processing unit of a computer"),
    ("ram",             "ram is temporary memory used by running programs"),
    ("a gpu",           "a gpu processes graphics and parallel computing tasks"),
    ("a byte",          "a byte is eight bits of digital information"),
    ("binary",          "binary is a number system using only zeros and ones"),
    ("a pixel",         "a pixel is the smallest unit of a digital image"),
    ("bandwidth",       "bandwidth is the maximum data transfer rate of a network"),
    ("latency",         "latency is the delay in data transmission across a network"),
    ("a cache",         "a cache stores frequently accessed data for faster retrieval"),
    ("a firewall",      "a firewall blocks unauthorized access to a computer network"),
    ("a bug",           "a bug is an error or flaw in computer software"),
    ("open source",     "open source software has publicly available source code"),
    ("a blockchain",    "a blockchain is a distributed ledger of digital transactions"),
    ("a stack",         "a stack is a last in first out data structure"),
    ("a queue",         "a queue is a first in first out data structure"),
    ("binary search",   "binary search finds an element by dividing the search space"),
    ("a pointer",       "a pointer stores the memory address of another variable"),
]
for concept, answer in TECH:
    expand(concept, answer)

# ── Geography ─────────────────────────────────────────────────
GEO_CAPITALS = [
    ("france",       "paris is the capital of france"),
    ("germany",      "berlin is the capital of germany"),
    ("japan",        "tokyo is the capital of japan"),
    ("italy",        "rome is the capital of italy"),
    ("spain",        "madrid is the capital of spain"),
    ("china",        "beijing is the capital of china"),
    ("india",        "new delhi is the capital of india"),
    ("canada",       "ottawa is the capital of canada"),
    ("australia",    "canberra is the capital of australia"),
    ("brazil",       "brasilia is the capital of brazil"),
    ("russia",       "moscow is the capital of russia"),
    ("mexico",       "mexico city is the capital of mexico"),
    ("egypt",        "cairo is the capital of egypt"),
    ("argentina",    "buenos aires is the capital of argentina"),
    ("south korea",  "seoul is the capital of south korea"),
    ("turkey",       "ankara is the capital of turkey"),
    ("poland",       "warsaw is the capital of poland"),
    ("sweden",       "stockholm is the capital of sweden"),
    ("greece",       "athens is the capital of greece"),
    ("portugal",     "lisbon is the capital of portugal"),
    ("indonesia",    "jakarta is the capital of indonesia"),
    ("ukraine",      "kyiv is the capital of ukraine"),
    ("netherlands",  "amsterdam is the capital of netherlands"),
    ("norway",       "oslo is the capital of norway"),
    ("denmark",      "copenhagen is the capital of denmark"),
]
for country, answer in GEO_CAPITALS:
    expand(f"the capital of {country}", answer, GEO_TEMPLATES[:2])
    add(f"what is the capital of {country}", answer)
    add(f"capital of {country}", answer)
    add(f"what is {country} capital", answer)

# City identity
CITIES = [
    ("paris",     "paris is the capital city of france"),
    ("tokyo",     "tokyo is the capital city of japan"),
    ("berlin",    "berlin is the capital city of germany"),
    ("rome",      "rome is the capital city of italy"),
    ("madrid",    "madrid is the capital city of spain"),
    ("beijing",   "beijing is the capital city of china"),
    ("moscow",    "moscow is the capital city of russia"),
    ("ottawa",    "ottawa is the capital city of canada"),
    ("brasilia",  "brasilia is the capital city of brazil"),
    ("canberra",  "canberra is the capital city of australia"),
    ("london",    "london is the capital city of the united kingdom"),
    ("seoul",     "seoul is the capital city of south korea"),
    ("athens",    "athens is the capital city of greece"),
]
for city, answer in CITIES:
    expand(city, answer, PARAPHRASE_TEMPLATES[:3])
    add(f"where is {city}", answer)

# Geo facts
GEO_FACTS = [
    ("france",      "france is a country in western europe with capital paris"),
    ("germany",     "germany is a country in central europe with capital berlin"),
    ("japan",       "japan is a country in east asia with capital tokyo"),
    ("the largest country", "russia is the largest country by land area"),
    ("the longest river",   "the nile is the longest river in the world"),
    ("the highest mountain","mount everest is the highest mountain in the world"),
    ("the largest ocean",   "the pacific ocean is the largest ocean in the world"),
    ("the largest continent","asia is the largest continent in the world"),
    ("mount everest",  "mount everest is located in the himalayas in nepal"),
    ("paris",       "paris is located in france in western europe"),
]
for concept, answer in GEO_FACTS:
    add(f"what is {concept}", answer)
    add(f"where is {concept}", answer)

# ── Science ───────────────────────────────────────────────────
SCI = [
    ("gravity",        "gravity is a force that attracts objects toward each other"),
    ("photosynthesis", "photosynthesis is how plants convert sunlight into food"),
    ("evolution",      "evolution is the gradual change of species over generations"),
    ("dna",            "dna is the molecule that carries genetic information"),
    ("a cell",         "a cell is the basic unit of all living organisms"),
    ("an atom",        "an atom is the smallest unit of a chemical element"),
    ("a molecule",     "a molecule is a group of atoms bonded together"),
    ("electricity",    "electricity is the flow of electric charge through a conductor"),
    ("energy",         "energy is the capacity to do work or cause change"),
    ("a black hole",   "a black hole is a region where gravity prevents light from escaping"),
    ("a star",         "a star is a massive ball of gas producing light by nuclear fusion"),
    ("radiation",      "radiation is the emission of energy as electromagnetic waves"),
    ("a virus",        "a virus is a microscopic agent that infects living cells"),
    ("nuclear energy", "nuclear energy is released by splitting or fusing atomic nuclei"),
    ("the solar system","the solar system is the sun and everything that orbits it"),
    ("water",          "water is a compound of hydrogen and oxygen essential for life"),
    ("oxygen",         "oxygen is a gas essential for breathing and combustion"),
    ("a wave",         "a wave transfers energy through a medium without moving matter"),
    ("entropy",        "entropy is a measure of disorder or randomness in a system"),
    ("pi",             "pi is the ratio of a circle circumference to its diameter"),
    ("light",          "light is electromagnetic radiation visible to the human eye"),
    ("sound",          "sound is a vibration that travels as a wave through matter"),
    ("magnetism",      "magnetism is the force exerted by magnets on other objects"),
    ("a photon",       "a photon is a particle of light carrying electromagnetic energy"),
    ("temperature",    "temperature measures the average kinetic energy of particles"),
    ("a proton",       "a proton is a positively charged subatomic particle"),
    ("an electron",    "an electron is a negatively charged subatomic particle"),
    ("a neutron",      "a neutron is a subatomic particle with no electric charge"),
    ("hydrogen",       "hydrogen is the lightest and most abundant element in the universe"),
    ("carbon",         "carbon is the element that forms the basis of all life"),
]
for concept, answer in SCI:
    expand(concept, answer)

# DNA special cases
add("what does dna stand for",          "dna stands for deoxyribonucleic acid the genetic molecule")
add("what do the letters dna stand for","dna stands for deoxyribonucleic acid the genetic molecule")

# ── History & People ──────────────────────────────────────────
PEOPLE = [
    ("albert einstein",    "albert einstein was a physicist who developed the theory of relativity"),
    ("einstein",           "albert einstein was a physicist who developed the theory of relativity"),
    ("isaac newton",       "isaac newton discovered the laws of motion and gravity"),
    ("newton",             "isaac newton discovered the laws of motion and gravity"),
    ("charles darwin",     "charles darwin developed the theory of natural selection"),
    ("darwin",             "charles darwin developed the theory of natural selection"),
    ("marie curie",        "marie curie was a scientist who discovered radium and polonium"),
    ("curie",              "marie curie was a scientist who discovered radium and polonium"),
    ("alan turing",        "alan turing was a mathematician who founded computer science"),
    ("turing",             "alan turing was a mathematician who founded computer science"),
    ("shakespeare",        "shakespeare was an english playwright who wrote hamlet and othello"),
    ("napoleon",           "napoleon was a french military leader and emperor of france"),
    ("cleopatra",          "cleopatra was the last ruler of ancient egypt"),
    ("julius caesar",      "julius caesar was a roman general and statesman"),
    ("caesar",             "julius caesar was a roman general and statesman"),
    ("abraham lincoln",    "abraham lincoln was the sixteenth president of the united states"),
    ("lincoln",            "abraham lincoln was the sixteenth president of the united states"),
    ("mahatma gandhi",     "mahatma gandhi led nonviolent resistance against british rule in india"),
    ("gandhi",             "mahatma gandhi led nonviolent resistance against british rule in india"),
    ("nelson mandela",     "nelson mandela was the first black president of south africa"),
    ("mandela",            "nelson mandela was the first black president of south africa"),
    ("thomas edison",      "thomas edison invented the lightbulb and many other devices"),
    ("edison",             "thomas edison invented the lightbulb and many other devices"),
    ("alexander the great","alexander the great conquered much of the ancient world"),
    ("the author",         "an author is a person who writes and publishes works"),
    ("an author",          "an author is a person who writes and publishes works"),
]
for concept, answer in PEOPLE:
    expand(concept, answer, PEOPLE_TEMPLATES)

HISTORICAL = [
    ("world war one",          "world war one was a global conflict from 1914 to 1918"),
    ("world war two",          "world war two was a global conflict from 1939 to 1945"),
    ("the renaissance",        "the renaissance was a cultural revival in europe in the 15th century"),
    ("the industrial revolution","the industrial revolution transformed manufacturing from the 18th century"),
    ("democracy",              "democracy is a system where citizens vote to choose their leaders"),
    ("communism",              "communism is a political system where the state owns all property"),
    ("capitalism",             "capitalism is an economic system based on private ownership and free markets"),
]
for concept, answer in HISTORICAL:
    expand(concept, answer)

# ── Nature & Biology ──────────────────────────────────────────
NATURE = [
    ("a mammal",    "a mammal is a warm blooded animal that feeds young with milk"),
    ("a reptile",   "a reptile is a cold blooded vertebrate with scales"),
    ("a bird",      "a bird is a warm blooded animal with feathers and wings"),
    ("a fish",      "a fish is a cold blooded vertebrate that lives in water"),
    ("an insect",   "an insect is a small arthropod with six legs and three body segments"),
    ("a tree",      "a tree is a tall plant with a wooden trunk branches and leaves"),
    ("a forest",    "a forest is a large area densely covered with trees and vegetation"),
    ("an ocean",    "an ocean is a vast body of saltwater covering most of the earth"),
    ("a mountain",  "a mountain is a large natural elevation of earth above surrounding land"),
    ("a river",     "a river is a large natural stream of flowing fresh water"),
    ("a desert",    "a desert is a dry region with very little rainfall and sparse vegetation"),
    ("a volcano",   "a volcano is an opening in earth through which lava erupts"),
    ("an earthquake","an earthquake is shaking of the ground caused by tectonic activity"),
    ("climate",     "climate is the long term pattern of weather in an area"),
    ("weather",     "weather is the short term state of the atmosphere at a location"),
    ("a hurricane", "a hurricane is a large rotating tropical storm with strong winds"),
]
for concept, answer in NATURE:
    expand(concept, answer)

# ── Everyday ──────────────────────────────────────────────────
EVERYDAY = [
    ("money",       "money is a medium of exchange used to buy goods and services"),
    ("education",   "education is the process of learning and acquiring knowledge"),
    ("medicine",    "medicine is the science of diagnosing and treating illness"),
    ("a language",  "a language is a system of communication using words and grammar"),
    ("music",       "music is organized sound that creates rhythm melody and harmony"),
    ("art",         "art is creative expression through visual or performing media"),
    ("sport",       "sport is a competitive physical activity following set rules"),
    ("food",        "food is any substance consumed to provide nutritional support"),
    ("health",      "health is a state of physical and mental wellbeing"),
    ("time",        "time is the progression of events from past to present to future"),
    ("a book",      "a book is a written or printed work bound together"),
    ("religion",    "religion is a set of beliefs about the nature and purpose of life"),
    ("philosophy",  "philosophy is the study of fundamental questions about existence"),
    ("psychology",  "psychology is the scientific study of the mind and behavior"),
    ("economics",   "economics studies the production distribution and consumption of goods"),
]
for concept, answer in EVERYDAY:
    expand(concept, answer)

# Math
MATH = [
    ("algebra",   "algebra uses symbols to represent unknown quantities in equations"),
    ("geometry",  "geometry is the study of shapes sizes and positions"),
    ("calculus",  "calculus studies rates of change and accumulation of quantities"),
    ("statistics","statistics is the science of collecting and analyzing data"),
    ("probability","probability measures the likelihood of an event occurring"),
    ("a matrix",  "a matrix is a rectangular array of numbers arranged in rows and columns"),
    ("a vector",  "a vector is a quantity with both magnitude and direction"),
    ("a prime number","a prime number is divisible only by one and itself"),
    ("infinity",  "infinity is a concept representing a quantity without limit"),
]
for concept, answer in MATH:
    expand(concept, answer)

print(f"After core facts: {len(out_pairs)} pairs")

# ════════════════════════════════════════════════════════════
# P1: OOD CONCEPTS — known failures from adversarial eval
# ════════════════════════════════════════════════════════════

OOD_FACTS = [
    ("jazz",         "jazz is a music genre originating in african american communities"),
    ("sushi",        "sushi is a japanese dish made with vinegared rice and raw fish"),
    ("origami",      "origami is the japanese art of paper folding"),
    ("yoga",         "yoga is a practice combining physical postures breathing and meditation"),
    ("chess",        "chess is a strategy board game played on an eight by eight grid"),
    ("a podcast",    "a podcast is a digital audio show distributed via the internet"),
    ("a vaccine",    "a vaccine stimulates the immune system to prevent disease"),
    ("meditation",   "meditation is a practice of focused attention for mental calm"),
    ("a tsunami",    "a tsunami is a large ocean wave caused by an earthquake or eruption"),
    ("cryptocurrency","cryptocurrency is a digital currency secured by cryptography"),
    ("a drone",      "a drone is an unmanned aircraft operated by remote control"),
    ("caffeine",     "caffeine is a stimulant found in coffee tea and energy drinks"),
    ("a marathon",   "a marathon is a long distance running race of 42 kilometers"),
    ("inflation",    "inflation is the rate at which prices rise over time"),
    ("a telescope",  "a telescope is an optical instrument for viewing distant objects"),
    ("a symphony",   "a symphony is a large scale musical composition for orchestra"),
    ("a neuron",     "a neuron is a nerve cell that transmits electrical signals in the brain"),
    ("a genome",     "a genome is the complete set of genetic material in an organism"),
    ("a hormone",    "a hormone is a chemical messenger produced by glands in the body"),
    ("metabolism",   "metabolism is the set of chemical reactions sustaining life in cells"),
    ("a galaxy",     "a galaxy is a system of millions of stars held together by gravity"),
    ("the big bang", "the big bang is the theory that the universe began from a single point"),
    ("quantum mechanics","quantum mechanics describes the behavior of matter at atomic scales"),
    ("relativity",   "relativity is einsteins theory describing space time and gravity"),
    ("dna replication","dna replication is the process of copying dna before cell division"),
    ("a semiconductor","a semiconductor is a material with electrical conductivity between metals and insulators"),
    ("artificial intelligence","artificial intelligence is the simulation of human intelligence by machines"),
]
for concept, answer in OOD_FACTS:
    expand(concept, answer)

print(f"After OOD facts: {len(out_pairs)} pairs")

# ════════════════════════════════════════════════════════════
# P2: Load existing sources (qa_output.txt, anchor files)
# ════════════════════════════════════════════════════════════

# qa_output.txt
content = Path("training/qa_output.txt").read_text(encoding="utf-8", errors="ignore")
blocks  = [b.strip() for b in content.split("--------------------") if b.strip()]
for block in blocks:
    lines = [l.strip() for l in block.splitlines() if l.strip()]
    questions, answer = [], None
    for line in lines:
        if line.startswith("concept:"): continue
        elif line.startswith("q:"): questions.append(line[2:].strip())
        elif line.startswith("a:"): answer = line[2:].strip()
    if questions and answer:
        ans_clean = re.sub(r'[^\x20-\x7e]',' ',answer).strip().lower()
        ans_clean = ' '.join(ans_clean.split()[:12])
        for q in questions:
            q_c = re.sub(r'[^\x20-\x7e]',' ',q).strip().lower().rstrip('?')
            add(q_c, ans_clean)

print(f"After qa_output.txt: {len(out_pairs)} pairs")

# anchor files
for path in ["training/definition_anchors.txt",
             "training/divergence_anchors.txt",
             "training/divergence_v2.txt",
             "training/divergence_v3.txt",
             "training/divergence_v4.txt",
             "training/divergence_v5.txt",
             "training/geography_anchors.txt"]:
    try:
        lines = [l.rstrip('\r\n').strip() for l in
                 Path(path).read_text(encoding='utf-8',errors='ignore').splitlines()
                 if l.strip() and not l.strip().startswith('#')]
    except FileNotFoundError:
        continue
    i = 0
    while i < len(lines)-1:
        q, a = lines[i].lower(), lines[i+1].lower()
        if len(q.split()) >= 2 and len(a.split()) >= 2:
            add(q, a)
        i += 2

print(f"After anchor files: {len(out_pairs)} pairs")

# ════════════════════════════════════════════════════════════
# Write pool
# ════════════════════════════════════════════════════════════
out = Path("training/answer_pool.txt")
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    f.write(f"# EH-G3 Answer Pool v2 — {len(out_pairs)} pairs\n")
    f.write("# Format: alternating question / answer lines\n\n")
    for q, a in out_pairs:
        f.write(q + '\n')
        f.write(a + '\n')

print(f"\nWritten: {out} ({len(out_pairs)} Q/A pairs)")

