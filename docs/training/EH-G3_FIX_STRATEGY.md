# EH-G3 Fix Strategy: From Debug to Working System

**Date**: June 4, 2026  
**Status**: 🔴 Blocking issues identified, fixes planned  
**Priority**: Critical for EH-G3 paradigm shift

---

## Problem Summary

| Issue | Severity | Impact | Status |
|-------|----------|--------|--------|
| Vocab mismatch (75% tokens missing) | 🔴 Critical | Beam terminates immediately | ⚠️ Root cause |
| Negative embeddings (cosine ≈ -0.99) | 🔴 Critical | Embeddings anti-correlated | ⚠️ Root cause |
| No embedding effect on scoring | 🔴 Critical | Paradigm shift not realized | ⚠️ Confirmed |
| Incorrect tokenization algorithm | 🔴 Critical | Vocab IDs completely wrong | ⚠️ Design flaw |

---

## Root Cause Analysis

### Why Tokenization Failed

**Python Trainer** (`eh_g3_trainer_pure.py`):
```python
class SimpleTokenizer:
    def __init__(self):
        self.vocab = {}  # Builds from corpus
        self.id_to_word = {}
    
    def tokenize(self, text):
        words = text.split()
        token_ids = []
        for word in words:
            if word in self.vocab:
                token_ids.append(self.vocab[word])
            # Unknown words get None or skipped
```

**C Code** (`qa_attention_g3.c`):
```c
uint32_t hash = 0;
for (size_t i = 0; word[i]; i++) {
    hash += (uint32_t)word[i];  // Just sum ASCII values!
}
token_id = hash % 2000;
```

**Result**: Completely different vocab mappings!
- Python: "what" → 127 (if 127th in vocab)
- C: "what" → 436 (ASCII sum)

### Why Embeddings Are Negative

**Training Loss** (contrastive):
```python
loss = -sim(Q, correct_A) + sim(Q, wrong_A)
# Minimize: make correct answers similar, wrong answers dissimilar
# Result: Embeddings learn to maximize spread
```

**DAG Vectors** (co-occurrence):
```c
edge.weight = log(co_occurrence_count + 1)
// Positive correlation: words that appear together have high weight
```

**Incompatibility**:
- Contrastive: Learn separable space (orthogonal vectors)
- Co-occurrence: Learn correlated space (high dot products)
- Result: Negative dot products!

---

## Fix Strategy: 3-Phase Approach

### Phase 0: Verify the Problem (30 min)

**Goal**: Confirm vocab mismatch before fixing

**Steps**:
1. Export vocab from Python trainer
   ```python
   # In eh_g3_trainer_pure.py after training
   with open('training/vocab.json', 'w') as f:
       json.dump(tokenizer.vocab, f)
   ```

2. Load vocab in C and verify
   ```c
   // Load JSON vocab, check if token IDs match
   // Print mismatch statistics
   ```

3. Check embedding-DAG correlation
   ```c
   // For each edge, compute dot(edge_weight, embedding)
   // Should be mostly positive if aligned
   ```

**Expected outcome**: Confirm 75% vocab mismatch + negative correlations

---

### Phase 1: Fix Tokenization (2 hours)

**Goal**: Make C tokenization match Python training

**Option A**: Save vocab from trainer
```python
# eh_g3_trainer_pure.py - add at end
import json
with open('training/eh_g3_vocab.json', 'w') as f:
    json.dump({
        'vocab': tokenizer.vocab,
        'id_to_word': tokenizer.id_to_word,
        'vocab_size': len(tokenizer.vocab)
    }, f)
```

**Option B**: Load vocab in C
```c
// Load JSON vocab in qa_attention_g3.c
// Create tokenizer that uses loaded vocab
typedef struct {
    char* words[2000];
    uint32_t ids[2000];
    uint32_t count;
} Vocab;

Vocab vocab = load_vocab_from_json("training/eh_g3_vocab.json");
```

**Option C**: Use lookup table
```c
// Simpler: Just hardcode common words or load from text file
// "what" → 0
// "is" → 1
// "a" → 2
// etc.
```

**Recommendation**: Option A + B (cleanest, reproducible)

**Time**: 1-2 hours (JSON parsing may need custom implementation to avoid dependencies)

---

### Phase 2: Realign Embeddings (3 hours)

**Goal**: Make embeddings positively correlated with DAG

**Option A**: Retrain embeddings with alignment
```python
# Modified contrastive loss to also match DAG structure
loss = contrastive_loss + alignment_loss
# alignment_loss: encourage dot(embedding, dag_edge) > 0
```

**Option B**: Fine-tune embeddings
```python
# Start with contrastive embeddings
# Fine-tune with alignment objective
# Minimize negative dot products
```

**Option C**: New training objective
```python
# Instead of pure contrastive:
# Maximize: sim(Q, correct_A) + sim(Q, dag_edges) - sim(Q, wrong_A)
# This directly optimizes for DAG alignment
```

**Recommendation**: Option A (retrain with combined loss)

**Implementation**:
```python
def combined_loss(q, correct_a, wrong_a, dag_edges):
    # Contrastive part
    l1 = -cosine(q, correct_a) + cosine(q, wrong_a)
    
    # DAG alignment part
    l2 = -mean([cosine(q, e) for e in dag_edges if relevant(e, q)])
    
    # Combined
    return l1 + alpha * l2  # alpha=0.5 or tune
```

**Time**: 1-2 hours training + 1 hour to implement

---

### Phase 3: Integration & Validation (2 hours)

**Goal**: Get everything working together

**Steps**:
1. Use fixed tokenizer with loaded vocab
2. Use realigned embeddings
3. Test on benchmark questions
4. Verify scores are non-zero and make sense
5. Measure accuracy improvement

**Success criteria**:
- ✅ Tokens found in DAG (>80%)
- ✅ Positive cosine similarities
- ✅ Non-zero scores
- ✅ Different scores for different queries
- ✅ Scores correlate with embedding relevance

---

## Implementation Roadmap

### Immediate (Today - Phase 0)
- [x] Identify vocab mismatch (DONE via debug)
- [x] Confirm negative embeddings (DONE via debug)
- [ ] Measure exact correlation statistics
- [ ] Create fix priority list

### Short-term (Tomorrow - Phases 1-3)
- [ ] Implement vocab export/import
- [ ] Fix tokenization in C
- [ ] Retrain embeddings with alignment loss
- [ ] Validate fixes
- [ ] Measure accuracy improvement

### Medium-term (Next iteration)
- [ ] Explore Option B & C approaches
- [ ] Optimize alignment loss weight
- [ ] Tune hyperparameters
- [ ] Benchmark against EH-G2

---

## Detailed Implementation (Phase 1: Tokenizer)

### Step 1: Export Vocab from Trainer

**File**: `examples/eh_g3_trainer_pure.py` (add to end)
```python
def save_vocab(tokenizer, output_file):
    """Save tokenizer vocab to file for C to use"""
    vocab_data = {
        'word_to_id': tokenizer.vocab,
        'vocab_size': len(tokenizer.vocab),
        'format': 'text'  # Simple text format
    }
    
    # Save as newline-delimited: word\tid
    with open(output_file, 'w') as f:
        for word, word_id in sorted(tokenizer.vocab.items(), key=lambda x: x[1]):
            f.write(f"{word}\t{word_id}\n")
    
    print(f"Vocab saved: {len(tokenizer.vocab)} words → {output_file}")
```

**Call**: Add after training loop
```python
save_vocab(tokenizer, 'training/eh_g3_vocab.txt')
```

**Output format**:
```
the	0
is	1
a	2
what	3
computer	4
...
```

### Step 2: Load Vocab in C

**File**: `examples/qa_attention_g3.c` (new function)
```c
typedef struct {
    char* words[2000];
    uint32_t count;
} SimpleVocab;

SimpleVocab* load_vocab(const char *vocab_file) {
    SimpleVocab *vocab = malloc(sizeof(SimpleVocab));
    memset(vocab, 0, sizeof(*vocab));
    
    FILE *f = fopen(vocab_file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open vocab file %s\n", vocab_file);
        return NULL;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f) && vocab->count < 2000) {
        // Parse: word\tid
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        
        *tab = '\0';
        vocab->words[vocab->count] = strdup(line);
        vocab->count++;
    }
    
    fclose(f);
    printf("Loaded vocab: %u words\n", vocab->count);
    return vocab;
}

uint32_t tokenize_with_vocab(const char *text, SimpleVocab *vocab, 
                             uint32_t *token_ids, uint32_t *count) {
    *count = 0;
    char *copy = strdup(text);
    
    char *word = strtok(copy, " \t\n");
    while (word && *count < 100) {
        // Binary search or linear search in vocab
        for (uint32_t i = 0; i < vocab->count; i++) {
            if (strcmp(vocab->words[i], word) == 0) {
                token_ids[*count] = i;
                (*count)++;
                break;
            }
        }
        word = strtok(NULL, " \t\n");
    }
    
    free(copy);
    return *count;
}
```

### Step 3: Use Fixed Tokenizer

**In main()**:
```c
SimpleVocab *vocab = load_vocab("training/eh_g3_vocab.txt");
if (!vocab) {
    fprintf(stderr, "Error loading vocab\n");
    return 1;
}

// For each question
uint32_t q_tokens[100];
uint32_t q_count = 0;
tokenize_with_vocab(question, vocab, q_tokens, &q_count);

printf("Question: %s\n", question);
printf("Tokens: %u found / %u total (%.1f%%)\n", q_count_found, q_count, 
       100.0 * q_count_found / q_count);
```

**Expected result after fix**:
- Before: 1/4 tokens (25%)
- After: 4/4 tokens (100%)

---

## Detailed Implementation (Phase 2: Embedding Alignment)

### New Training Objective

**File**: `examples/eh_g3_trainer_pure.py` (modify training loop)

```python
def compute_alignment_loss(q_embedding, dag_edge_embeddings, alpha=1.0):
    """
    Encourage embeddings to align with DAG structure
    Goal: Make dot(query, edge) > 0 for typical edges
    """
    losses = []
    for edge_emb in dag_edge_embeddings:
        sim = dot_product(q_embedding, edge_emb)
        # We want sim > 0, so minimize -sim
        loss = -sim if sim < 0 else 0
        losses.append(loss)
    
    return alpha * mean(losses) if losses else 0

# In training loop
for epoch in range(num_epochs):
    for i, (q_tok, a_tok) in enumerate(zip(q_tokens, a_tokens)):
        # Contrastive loss (existing)
        neg_answers = [pairs[(i + j + 1) % len(pairs)][1] for j in range(2)]
        neg_toks = [tokenizer.tokenize(neg) for neg in neg_answers]
        c_loss = model.contrastive_loss(q_tok, a_tok, neg_toks)
        
        # NEW: Alignment loss
        # Note: Need access to DAG edge embeddings (problem!)
        # For now: Use answer embeddings as proxy for DAG edges
        a_emb = model.get_embedding(a_tok)
        align_loss = compute_alignment_loss(q_emb, [a_emb])
        
        # Combined loss
        total_loss = c_loss + 0.3 * align_loss  # Weight the alignment
        
        # Backprop and update
```

**Result**: Embeddings trained to be positive correlation with DAG structure

---

## Success Metrics

### Before Fix
```
Query: "what is a computer"
Tokens found: 1/4 (25%)
Embedding norm: 0.0679
DAG dot products: -0.9939, -0.9938, -0.9520
Average cosine: -0.987 ❌
Beam terminations: Immediate (0 steps)
Scores: 0.00 for all queries ❌
```

### After Fix (Expected)
```
Query: "what is a computer"
Tokens found: 4/4 (100%) ✅
Embedding norm: 0.X (TBD)
DAG dot products: +0.45, +0.38, +0.52
Average cosine: +0.45 ✅
Beam steps: 3-5 steps before termination ✅
Scores: Non-zero, varying by query ✅
Accuracy: 75-85% (up from 73%) ✅
```

---

## Timeline

| Phase | Task | Time | Status |
|-------|------|------|--------|
| 0 | Verify problem | 30m | ⏸️ Ready |
| 1 | Fix tokenization | 2h | ⏳ Blocked |
| 2 | Realign embeddings | 3h | ⏳ Blocked |
| 3 | Validate | 2h | ⏳ Blocked |
| **Total** | **From broken to working** | **7.5h** | |

---

## Recommendation

**Current Status**: 🔴 Critical blocking issues identified

**Action**: Implement Phase 0 verification + Phase 1 (tokenizer fix) first
- Phase 0: Confirm all hypotheses with stats
- Phase 1: Should fix 90% of the problem (vocab mismatch)
- Phase 2: Additional improvement from alignment

**Expected Outcome**: EH-G3 paradigm shift actually realized with proper vocabulary and embedding alignment

---

## Alternative: Start Fresh with Knowledge Distillation

**If fix feels like patch work**, consider true knowledge distillation:

```
Real Goal: "Teacher Model → Compact DAG"

Current Approach: "DAG + Embeddings overlay (incompatible)"

Better Approach:
1. Train semantic teacher (transformer or similar)
2. Distill to lightweight student
3. Encode student in DAG structure
4. Optimize for edge AI runtime

This is the REAL paradigm shift!
```

**But this requires more time/expertise, so Phase 1-3 fixes are good immediate wins.**

---

**Next Step**: Run Phase 0 verification, then proceed with Phase 1 (tokenizer fix)
