# Failing Concepts — v16 Adversarial Eval
# Generated from 160-question test. Grouped by drift pattern.

## ROOT CAUSE A: "molecule" super-path (kills all science)
# Beam: X is → molecule made of hydrogen...
# All science concepts drift to this single path.

gravity         → should: "force that attracts objects with mass"
photosynthesis  → should: "plants convert sunlight into food"
evolution       → should: "gradual change of species over generations"
atom            → should: "smallest unit of a chemical element"
electricity     → should: "flow of electric charge through conductor"
cell            → should: "basic unit of all living organisms"
molecule        → should: "group of atoms bonded together"
energy          → should: "capacity to do work or cause change"
black hole      → should: "region where gravity prevents light escaping"
star            → should: "massive ball of gas producing light by fusion"
virus           → should: "microscopic agent that infects living cells"
solar system    → should: "sun and everything that orbits it"
oxygen          → should: "gas essential for breathing and combustion"
wave            → should: "transfers energy through a medium"
entropy         → should: "measure of disorder or randomness in system"
photon          → should: "particle of light carrying electromagnetic energy"
nuclear energy  → should: "released by splitting or fusing atomic nuclei"
dna             → (partial) "carries genetic information"
gravity (paraphrase "define gravity")   → molecule drift

## ROOT CAUSE B: "computer program managing hardware" super-path (kills tech)
# Beam: X is → computer program managing hardware and loads operating system
# Comes from "operating system" anchor poisoning nearby paths.

git             → should: "version control system for tracking code"
python (partial)→ drifts to "computer program managing hardware"
javascript      → should: "scripting language for web development"
html            → should: "markup language for creating web pages"
compiler        → should: "translates source code to machine code"
encryption      → should: "converts data into secure unreadable format"
function        → should: "reusable block of code that performs a task"
variable (partial)→ should: "named storage that holds a value"
class           → should: "blueprint for creating objects"
debugging       → should: "finding and fixing errors in code"
version control → should: "tracks changes to code over time"
cpu             → should: "central processing unit that executes instructions"
ram             → should: "random access memory used by running programs"
firewall        → should: "blocks unauthorized network access"
blockchain      → should: "distributed ledger of digital transactions"
stack           → should: "last in first out data structure"
queue           → should: "first in first out data structure"
bandwidth       → should: "maximum data transfer rate of a network"
latency         → should: "delay in data transmission across a network"
api             → should: "interface that lets programs communicate"
server          → should: "provides services to client computers"
network         → should: "connects computers to share information"
internet        → should: "global network connecting computers worldwide"
pixel           → should: "smallest element of a digital display"
binary          → should: "base two number system using zeros and ones"
machine learning (paraphrase) → computer program drift
algorithm (paraphrase) → computer program drift
neural network  → should: "model inspired by brain neurons"
deep learning   → should: "multilayer neural networks learn patterns"
a computer (partial) → api drift
a database (partial) → api drift

## ROOT CAUSE C: "greatest writer in english literature" super-path (kills people)
# Beam: X is → known as one of the greatest writer in english literature
# Comes from Shakespeare anchor bleeding into all people queries.

einstein (partial) → "known as greatest writer" (wrong: should be physicist)
alan turing (partial) → "greatest writer" drift
darwin (partial) → same
curie (partial) → same
napoleon (partial) → freeing enslaved people (Lincoln's answer)
gandhi (partial) → invented theoretical foundations (Turing's answer)
mandela → tool used to buy goods (total drift)
edison (partial) → greatest writer
lincoln (partial) → greatest writer
caesar (partial) → greatest writer
cleopatra (partial) → greatest writer
author (partial) → eiffel tower drift

## ROOT CAUSE D: Geography — specific capitals still missing
# Partial or wrong for these:

italy capital   → "tool used to buy goods" (total drift)
brazil capital  → gets spain's capital (madrid)
canada capital  → tool drift
egypt capital   → tool drift
greece capital  → beliefs about paris
where is paris  → says china
where is everest → says china
largest country → lands on "by trees and open source"
longest river   → "language spoken in france"
highest mountain → "erupts lava"

## ROOT CAUSE E: OOD (no corpus coverage — expected fail)
jazz, origami, yoga, chess, meditation, cryptocurrency,
drone, caffeine, marathon, telescope, symphony, neuron

## ROOT CAUSE F: Paraphrase patterns
# "define X" works sometimes, "what does X mean" fails
# → last token is "mean" not the concept

what does python mean  → "mean a computer program" (last token = "mean")
what does algorithm mean → same
what does gravity mean → same
what does dna stand for → Darwin answer
what does a server do  → "do work or heat transfer"

## PRIORITY FIX ORDER

### P0 — Fix molecule super-path (blocks 17 science questions)
Need: strengthen individual science paths so they score HIGHER than molecule path.
Each science concept needs 5+ distinct continuations with high edge weight.

### P1 — Fix "computer program managing hardware" super-path (blocks 25 tech)
Need: "computer program managing" comes from OS anchor bleeding.
Fix: reduce OS anchor weight OR add stronger alternatives per concept.

### P2 — Fix "greatest writer" super-path (blocks 10 people)
Need: shakespeare "greatest writer" path is being applied to all people.
Each person needs their OWN distinctive descriptor path.

### P3 — Fix geography remaining (5 capitals + 4 fact questions)
italy/brazil/canada/egypt/greece capitals still missing.
"where is X" hitting China because geo domain picks wrong start.

### P4 — Fix paraphrase "what does X mean" pattern
Last token is "mean" not the concept. Need to strip trailing verb.
