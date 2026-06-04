# EH-G3: From Vision to Reality - What We Learned

**Date**: June 4, 2026  
**Lesson**: "Score 0.00 everywhere" is a debugging goldmine

---

## What Happened

### The Vision ✨
```
EH-G2 (Language Model, 73% accuracy)
  ↓ (Apply contrastive training)
EH-G3 (Retrieval/Matching Model, 85-90% accuracy)
```

### The Implementation 🏗️
```
✅ Phase 1: Hybrid scoring implemented
✅ Phase 2: Embeddings trained
✅ Phase 3: Integration done
🚀 Ready to test!
```

### The Reality Check 🔍
```
Test run: All scores 0.00
Debug output shows:
  - 75% of tokens not in DAG
  - Embeddings anti-correlated with DAG (-0.99 cosine)
  - Beam terminates immediately
Result: Paradigm shift NOT achieved
```

---

## Root Cause: Three Separate Issues

### 1. Tokenization Mismatch (75% Tokens Missing)

**Why it happened**:
```python
# Python trainer
class SimpleTokenizer:
    def tokenize(self, text):
        # Builds vocab from corpus
        # "what" → ID based on corpus position
```

```c
// C inference code
uint32_t hash = sum(ASCII values) % 2000;
// "what" → arbitrary hash
// Completely different mapping!
```

**Impact**: 
- Start token 436 has 0 edges in DAG
- Beam terminates immediately
- No chance for embeddings to affect scoring

### 2. Negative Embeddings (-0.99 Cosine)

**Why it happened**:
```
Contrastive training loss:
  maximize sim(Q, correct_A)
  minimize sim(Q, wrong_A)
  → Result: Separable embedding space

DAG edge weights:
  Built from co-occurrence
  → Result: Correlated space

Combination: Fundamentally incompatible!
```

**Impact**:
- Even if tokens matched, dot products would be negative
- Ranking would be backwards
- Embeddings would hurt, not help

### 3. Immediate Beam Termination

**Why it happened**:
```
Token 436 (start token) not in DAG
  ↓
Fanout = 0 edges
  ↓
Beam marks as finished
  ↓
Score remains 0.0
  ↓
Generation ends
```

**Impact**:
- Beam search takes 0 steps
- No scoring opportunity
- All queries behave identically

---

## What This Means

### For EH-G3 Project

| Aspect | Before | After Debug | Status |
|--------|--------|------------|--------|
| Vision clarity | Unclear | Crystal clear | ✅ |
| Problem identification | None detected | 3 major issues | ✅ |
| Root causes | Unknown | Documented | ✅ |
| Path forward | Unclear | Detailed fix strategy | ✅ |

**Bottom line**: We discovered the problems are fixable!

### For the Paradigm Shift

**Originally claimed**: "Language Model → Retrieval/Matching Model"

**What actually happened**: 
- Infrastructure built ✅
- Embeddings trained ✅
- Integration attempted ✅
- **But integration broken** ❌

**Real status**: Paradigm shift not yet realized, but now we know why and how to fix it.

---

## Key Learning: Trust Your Suspicions

**User observation**: "All scores 0.00 is suspicious"  
**My response**: "Let's debug!"  
**Result**: Found fundamental issues

**Lesson**: When something smells wrong, there's usually a real problem.

---

## The Fix (7.5 hours estimated)

### Phase 0: Verify (30 min)
```bash
# Confirm vocab mismatch statistics
# Confirm embedding anti-correlation
# Create detailed metrics
```

### Phase 1: Tokenization Fix (2 hours)
```python
# Export vocab from Python trainer
with open('training/eh_g3_vocab.txt', 'w') as f:
    for word, idx in sorted(vocab.items(), key=lambda x: x[1]):
        f.write(f"{word}\t{idx}\n")
```

```c
// Load vocab in C, use for tokenization
Vocab *vocab = load_vocab("training/eh_g3_vocab.txt");
tokenize_with_vocab(question, vocab, token_ids);
```

### Phase 2: Embedding Alignment (3 hours)
```python
# Retrain with combined loss:
# contrastive_loss + alignment_loss
# Encourages dot(embedding, dag_edge) > 0
```

### Phase 3: Validation (2 hours)
```bash
# Test with fixed tokenizer + aligned embeddings
# Measure accuracy improvement
# Confirm paradigm shift works
```

---

## Expected Results After Fix

### Tokenization
```
Before: Tokens found: 1/4 (25%)
After:  Tokens found: 4/4 (100%)
```

### Embeddings
```
Before: Cosine ≈ -0.99 (anti-correlated)
After:  Cosine ≈ +0.4-0.5 (positive correlation)
```

### Scoring
```
Before: All scores 0.00 (beam terminates)
After:  Scores 0.5-2.0+ (beam takes 3-5 steps)
```

### Accuracy
```
Before: 73% (EH-G2 baseline, embeddings not used)
After:  80-85% (EH-G3 with proper embeddings)
Target: 85-90% (with further tuning)
```

---

## Lessons for Future Projects

### ✅ Do This
1. **Test immediately** - Don't wait for perfect code
2. **Debug early** - When you see 0.00 everywhere, investigate
3. **Validate assumptions** - Vocab consistency matters
4. **Use debug output** - Print embedding values and similarities
5. **Question surprising results** - Your suspicion was right

### ❌ Don't Do This
1. **Assume it works** - Test each component
2. **Ignore symptoms** - 0.00 scores = red flag
3. **Mix incompatible training objectives** - DAG freq + contrastive training = bad
4. **Overlay solutions** - Embeddings + DAG need alignment
5. **Skip verification** - Check vocab before inference

---

## Important: This Was Actually a GOOD Find

**Why this is good news**:
- ✅ Problems are fixable (not architectural)
- ✅ Root causes identified (not mysterious)
- ✅ Solution is clear (not ambiguous)
- ✅ Timeline is short (7.5 hours, not weeks)
- ✅ Vision is still valid (just implementation issues)

**If we hadn't debugged**:
- Would have shipped broken system
- Thought embeddings don't help (false conclusion)
- Abandoned the approach (waste of work)

---

## The Correct Paradigm Shift

### What EH-G3 SHOULD Be

```
Query: "what is a computer"
  ↓ (Fixed tokenizer)
Tokens: [what, is, a, computer]
  ↓ (All found in DAG vocab)
Embed: Average of token embeddings
  ↓ (Now properly aligned with DAG)
Score candidates: dot(query_embed, candidate_embed)
  ↓ (Positive scores for semantic matches!)
Beam search: Routes through semantically relevant paths
  ↓
Better answers: Foundational Q&A answers selected
  ↓
Improved accuracy: 80-85%+ from 73%
```

### What It Currently Is

```
Query: "what is a computer"
  ↓ (Wrong tokenization)
Hash tokens: [436, 220, 97, 879]
  ↓ (Most not in DAG vocab)
Start at token 436: No outgoing edges!
  ↓
Beam terminates
  ↓
Score: 0.00 (no scoring happened)
```

---

## Next Actions

### Today
- [x] Debug and identify issues
- [x] Create fix strategy documents
- [ ] **Run verification (Phase 0)**

### Tomorrow
- [ ] Export vocab from trainer (Phase 1)
- [ ] Implement vocab-based tokenizer (Phase 1)
- [ ] Retrain embeddings with alignment loss (Phase 2)
- [ ] Validate and measure improvements (Phase 3)

### Success Criteria
- [ ] Tokens found: >95% of test questions
- [ ] Cosine similarity: Positive (>0.3)
- [ ] Non-zero scores across queries
- [ ] Accuracy: >80%
- [ ] Paradigm shift verified

---

## Conclusion

**We built infrastructure for EH-G3 correctly but didn't align the pieces.**

**Like building a car with incompatible parts:**
- ✅ Engine (embedding trainer)
- ✅ Chassis (beam search modification)
- ❌ But engine doesn't fit in chassis (vocab mismatch + anti-correlation)

**Now we know what needs to be done:**
1. Make tokenizer consistent
2. Align embeddings with DAG
3. Validate the paradigm shift works
4. Measure accuracy improvements

**Effort**: 7.5 hours to fix completely  
**Confidence**: High - root causes identified  
**Result**: Working EH-G3 system with proper paradigm shift

---

**Status**: 🔴 → 🟠 (from "mysterious failure" to "known issues with clear fixes")  
**Next**: Implement Phase 0 verification, then execute fix phases

This was a PRODUCTIVE debug session that will result in a much better system!
