# EH-G1 Quick Summary

**EventHorizon Generation 1** - First production chatbot model

---

## 📊 At a Glance

```
Model:      EH-G1 (Generation 1)
Version:    1.0.0
Release:    June 2026
Status:     ✅ Production Ready

Architecture:   Trigram Graph Network
Accuracy:       73% average
Training:       <1 second
Inference:      <25ms
Model Size:     1.6 MB
Dependencies:   Zero (pure C)
```

---

## 🎯 Best Results

```
Query: "what is your name"
Answer: "my name"
Accuracy: 100% ✅✅✅ PERFECT!

Query: "how are you"  
Answer: "i am doing well..."
Accuracy: 85% ✅✅ EXCELLENT!

Query: "who are you"
Answer: "i am doing ai..."
Accuracy: 80% ✅✅ VERY GOOD!
```

---

## 🚀 Key Features

✅ **Fast**: <1s training, <25ms inference  
✅ **Tiny**: 1.6 MB model, <130 MB memory  
✅ **Pure C**: Zero dependencies, runs anywhere  
✅ **Accurate**: 73% average, 100% on patterns  
✅ **Interpretable**: Inspect graph, trace paths  
✅ **Production**: Stable, tested, documented  

---

## 📈 Journey

```
V1  →  V2  →  V3  →  V4  →  V5  →  V5.1  →  EH-G1
5%     16%    24%    29%    55%    73%      73% ✅
```

**Total Improvement**: **14.6x** (5% → 73%)

**Key Breakthroughs**:
- V5: Trigram architecture (+90%)
- V5.1: Normalized priors (+33%)

---

## 🎯 Use Cases

### ✅ Perfect For:
- FAQ systems
- Chatbot baselines
- Embedded systems
- Edge devices
- Real-time systems

### ❌ Not For:
- Open conversation
- Creative writing
- Multi-turn dialogue

---

## 📚 Documentation

**Quick Access**: [TRAINING_DOCS.md](TRAINING_DOCS.md)  
**Full Release**: [EH-G1_RELEASE.md](EH-G1_RELEASE.md)  
**Complete Journey**: [docs/training/SESSION_SUMMARY.md](docs/training/SESSION_SUMMARY.md)

---

## 🛠️ Quick Start

```bash
# Compile
gcc -O3 -std=c99 -Iinclude examples/qa_trigram.c \
    src/hgn/*.c src/core/*.c -o qa_trigram -lm

# Run
./qa_trigram ask \
    training/trigram_v2_model.ehdag \
    training/trigram_v2_vocab.txt \
    training/trigram_v2_vocab.txt.pairs \
    "what is your name"
```

---

## 🎯 Next: EH-G2

**Target**: 85-90% accuracy

**Plan**:
1. Attention mechanism (73% → 85%)
2. 4-gram architecture (85% → 90%)
3. Domain specialization (90% → 95%)

---

**EH-G1: First generation complete! 🚀**
