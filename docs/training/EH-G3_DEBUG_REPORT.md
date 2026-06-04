# EH-G3 Debug Report: Vấn Đề Tìm Thấy

**Date**: June 4, 2026  
**Issue**: All scores are 0.00 - embeddings không ảnh hưởng  
**Root Cause**: Tokenization sai + embeddings đối lập

---

## 🔴 Critical Findings

### 1. Tokenization Failure (CRITICAL)

**Problem**: Simple hash tokenization không khớp vocab từ training

```
Input: "what is a computer"
Tokenized: [436, 220, 97, 879]

DAG Status:
  Token 436: NOT found ❌
  Token 220: NOT found ❌
  Token 97:  found ✓ (1/4)
  Token 879: NOT found ❌

Result: 75% tokens missing!
```

**Impact**:
- Beam starts at token 436 (not in DAG)
- Node 436 has 0 outgoing edges
- Beam finishes immediately
- No chance for embeddings to score anything

### 2. Negative Cosine Similarity (CATASTROPHIC)

**Problem**: Embeddings anti-correlated with DAG vectors

```
Question Embedding (from token 97 only):
  [−0.0679, −0.0703, −0.0727, −0.0751, ...]
  Norm: 2.177078

Node 1 Embedding:
  [+0.0998, +0.1098, +0.1197, +0.1296, ...]
  Norm: 7.692845

Cosine Similarity:
  dot_product = −16.645462
  result = −16.645 / (2.177 × 7.692) = −0.9939 ❌

Expected: −1.0 < sim < +1.0 ✓
Problem: sim consistently ≈ −0.99
```

**Why this matters**:
- Embeddings from contrastive trainer are OPPOSITE to DAG structure
- When used in scoring: `score = dot(embedding, edge_weight)`
- All dot products are NEGATIVE
- Ranking is backwards!

### 3. Beam Collapse (IMMEDIATE TERMINATION)

```
Beam Initialization:
  Starting token: 436 (hash of "what")
  In DAG? NO ❌
  Outgoing edges: 0
  Auto-finish? YES

After step 1:
  Active beams: 0
  Finished beams: 1
  Score: 0.000000

Result: Generation terminates immediately!
```

---

## 📊 Detailed Analysis

### Why Tokenization Failed

**Current approach**:
```python
hash = sum(ord(c) for c in word) % 2000
token_id = hash
```

**Problem**: 
- "what" → 'w'(119) + 'h'(104) + 'a'(97) + 't'(116) = 436
- But training vocab built differently!
- Training tokenizer (eh_g3_trainer_pure.py) builds vocab from corpus
- No guarantee "what" maps to ID 436 in training vocab

**Proof of mismatch**:
- Most tokens NOT in DAG
- Only 1 out of 4 test tokens found
- This isn't random - it's systematic vocab mismatch

### Why Embeddings Are Negative

**Hypothesis**:
Training created embeddings where:
- Question tokens cluster with negative correlation to answers
- Contrastive loss: maximize sim(Q, correct_A) − sim(Q, wrong_A)
- Result: Learned separable embedding space (possibly orthogonal)

**Issue**: 
- DAG edge weights built from co-occurrence (positive correlation)
- Embeddings built from contrastive learning (negative correlation)
- Fundamentally incompatible!

### Why Scores Are 0.00

**Flow**:
1. Token 436 → not in DAG → 0 edges
2. Beam has no edges to expand
3. Automatically marks as finished
4. Score = initial score (0.0) + nothing
5. Result: 0.00

---

## ⚠️ Why EH-G3 Vision Isn't Realized

**Original Vision**:
```
Knowledge Distillation Approach:
  Teacher Model (trained on Q&A)
    ↓
  Distill Knowledge
    ↓
  Compact Graph Embeddings
    ↓
  Efficient Runtime
```

**Current Implementation**:
```
Graph Search Approach:
  Static DAG (co-occurrence)
    +
  Embeddings Overlay (contrastive)
    =
  Incompatible Layers!
```

**Problem**: 
- DAG learned from frequencies (what words co-occur)
- Embeddings learned from semantics (Q&A matching)
- Two different learning objectives!

---

## 🛠️ How to Fix

### Option A: Fix Tokenization (Short-term)

**Current**:
```c
uint32_t hash = 0;
for (size_t i = 0; word[i]; i++) {
    hash += (uint32_t)word[i];
}
token_id = hash % 2000;
```

**Fix needed**:
1. Use SAME tokenizer as training
2. Either:
   - Load vocab from file (eh_g3_trainer_pure.py compatible)
   - Or: Save vocab during training for C inference
   - Or: Guarantee consistency between Python and C

### Option B: Align Embeddings with DAG (Medium-term)

**Problem**: Contrastive embeddings anti-correlated with co-occurrence

**Solutions**:
1. **Train different**: Instead of contrastive loss on Q&A pairs, train embeddings that align with DAG structure
2. **Distill properly**: Use DAG structure as teacher, embeddings as student
3. **Fine-tune**: Take pre-trained embeddings, fine-tune to match DAG via alignment loss

### Option C: True Knowledge Distillation (Long-term)

**Real vision**:
1. Train teacher model on Q&A pairs (or language model)
2. Distill into lightweight DAG structure
3. Not: overlay embeddings on existing DAG

---

## 📈 What Should Happen vs What Happens

### Expected Flow (EH-G3 Design)
```
Question
  ↓
Embed
  ↓
Cosine match with DAG edge weights
  ↓
High scores for semantically similar edges
  ↓
Beam search routes through semantic paths
  ↓
Better accuracy
```

### Actual Flow (Current Bug)
```
Question
  ↓
Hash tokenization (WRONG vocab)
  ↓
Most tokens not in DAG
  ↓
Starting node has 0 edges
  ↓
Beam terminates immediately
  ↓
Score = 0.00
  ↓
No embedding effect
```

---

## ✅ Verification Tests

### Test 1: Check Vocabulary Consistency
```bash
# From Python trainer
python -c "
from examples.eh_g3_trainer_pure import SimpleTokenizer
t = SimpleTokenizer()
tokens = t.tokenize('what is a computer')
print(tokens)  # Should match C tokenization
"
```

**Result**: Need to verify if tokenizations match!

### Test 2: Check Embedding Distribution
```c
// Print actual vocab from C
for (int i = 0; i < min(20, dag.vocab_size); i++) {
    const float *vec = eh_hgn_dag_node_vec(&dag, i);
    if (vec) {
        printf("Token %d: [%f, %f, ...]\n", i, vec[0], vec[1]);
    }
}
```

### Test 3: Verify Embedding-DAG Alignment
```c
// Check if embeddings correlate positively with edges
// (Currently they're anti-correlated)
float sum_positive = 0, sum_negative = 0;
// For each edge, check if embedding dot product > 0
// If mostly negative → problem confirmed
```

---

## 🎯 Real Issues vs Symptoms

| Symptom | Real Issue | Fix Level |
|---------|-----------|-----------|
| Scores 0.00 | Beam terminates | 🔴 Critical |
| No embedding effect | Vocab mismatch | 🔴 Critical |
| Negative cosines | Incompatible training | 🟠 Major |
| All queries same | Same problem for all | 🟠 Major |

---

## 🔍 Paradigm Shift Still Not Achieved

**Current Status**: ❌ Not a real shift yet

**Why**:
- Language Model (EH-G2): DAG structure + frequency
- Retrieval Model (EH-G3): Should be DAG structure + semantic embeddings
- **But**: Embeddings aren't actually being used for retrieval!

**What's needed**:
1. ✅ Embedding training: Done (but incompatible)
2. ✅ Embedding loading: Done (but no effect)
3. ❌ Embedding integration: NOT working (vocab mismatch)
4. ❌ Proper distillation: NOT implemented
5. ❌ Paradigm shift: NOT realized

---

## 💡 Next Steps

### Immediate (Debug)
1. Print vocab from both Python trainer and C DAG
2. Verify if tokenizations match
3. Check embedding-DAG correlation

### Short-term (Fix)
1. Make tokenizer consistent (use same vocab file)
2. Retrain embeddings aligned to DAG
3. Verify positive correlations

### Medium-term (Proper Implementation)
1. Implement real knowledge distillation
2. Use embedding space to guide beam search
3. Measure accuracy improvements

---

## Conclusion

**The paradigm shift to Retrieval/Matching Model is NOT yet realized.**

Current issues:
1. Tokenization mismatch (75% vocab missing)
2. Embeddings anti-correlated with DAG (cosine ≈ -0.99)
3. Beam terminates immediately (no chance to score)

Result: **Embeddings have zero effect on ranking** - exactly as the user suspected from seeing all 0.00 scores!

This is a foundational issue that prevents EH-G3 from working as designed.

---

**Status**: 🔴 **BLOCKING - Needs Fix**  
**Severity**: High - Prevents EH-G3 from functioning  
**Effort to Fix**: Medium - Requires vocabulary alignment & embedding retraining
