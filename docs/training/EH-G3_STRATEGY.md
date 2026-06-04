# EH-G3 Query-Aware Training Strategy

## Mission: 73% → 85-90% Accuracy

**Duration**: 2 days  
**Approach**: Contrastive query-aware embedding training  
**Key Insight**: Shift from "language modeling" to "retrieval/matching" paradigm

---

## Architecture Changes

### Before (EH-G2 Hybrid)
```
Question
  ↓ tokenize + embed question pairs
  ↓ compute question_embedding (avg pool)
  ↓ beam search with hybrid scoring:
      score = 0.7*local_context + 0.3*global_attention
  ↓
Answer
```

### After (EH-G3 Query-Aware)
```
Question
  ↓ query_embedding (trained for matching)
  ↓ beam search with hub-aware scoring:
      score = local_context + hub_penalty + query_similarity
  ↓ candidate selection
  ↓
Answer (more relevant to question)
```

---

## Phase 1: Infrastructure (4 hours, Day 1)

### ✅ Completed
- [x] Hybrid scoring in beam_search (70% local + 30% global)
- [x] EH_QueryContext struct
- [x] Hub-aware penalty (reduce hub node dominance)
- [x] Compilation verified

### Next Steps
1. **Contrastive Trainer** (`eh_g3_trainer.py`)
   - Loads Q&A pairs
   - Tokenizes text
   - Contrastive loss: `max_sim(Q, correct_A) - min_sim(Q, wrong_A)`
   - Outputs embedding vectors

2. **Corpus Preparation**
   - Extract from mega_qa.txt (baseline)
   - Create domain-specific:
     - medical_qa.txt (200+ pairs)
     - legal_qa.txt (200+ pairs)
     - technical_qa.txt (200+ pairs)
   - Total: 1000+ curated pairs

---

## Phase 2: Training & Integration (1 day)

### Workflow
1. Tokenize corpus (2000 vocab)
2. Train embeddings (5-10 epochs)
3. Save embeddings as numpy array
4. Load in `qa_attention.c` or beam_search
5. Evaluate on test questions

### Expected Results
- Hub collision cases improve ("what is a computer" → better)
- Strong patterns maintained ("what is your name" → still 100%)
- Overall: 73% → 80-85%
- Domain-specific: 90%+ in narrow domains

---

## Critical Points

### Problem 1: Hub Node Collision
**What**: Words like "is a" have 1000+ outgoing edges  
**Why it hurts**: Beam search gets trapped in high-prior paths  
**Solution**: Penalize high-fanout nodes proportionally

### Problem 2: Training-Inference Mismatch (EH-G2 Issue)
**What**: Original embeddings trained for sequential prediction  
**Why it hurts**: Query matching needs different embeddings  
**Solution**: Contrastive loss trains for matching, not prediction

### Problem 3: Generalization
**What**: 1000 pairs might overfit to specific patterns  
**Why it hurts**: New questions fail even if similar  
**Solution**: Diverse corpus + regularization

---

## Success Metrics

### Must Have
- No regression on good questions (>70% must stay >70%)
- Hub collision cases improve
- Inference speed unchanged (<25ms)

### Nice to Have
- Domain-specific accuracy >90%
- Broader generalization (OOV questions better)
- Embedding quality measurable (embedding norm, rank, etc.)

---

## Fallback Plan (If Stuck)

### If contrastive loss unstable
→ Use simple cosine similarity instead of hinge loss

### If hub penalty too aggressive
→ Reduce `hub_penalty` scaling factor (default 2.0)

### If training slow
→ Reduce epochs, use subset of corpus

### If embedding file too large
→ Quantize to int8 (512 embeddings × 128 dim → 64KB)

---

## Timeline

### Day 1 (Today) - Infrastructure
- [x] Modify beam_search.h/c (4-5 hours)
- [x] Verify compilation (0.5 hours)
- [ ] Create trainer skeleton (1 hour)
- [ ] Test trainer on small corpus (1 hour)

### Day 2 - Data & Training
- [ ] Curate 1000+ Q&A corpus (2-3 hours)
- [ ] Train embeddings (1-2 hours, mostly automatic)
- [ ] Integrate embeddings into beam (1 hour)
- [ ] Evaluate accuracy (1 hour)
- [ ] Document results (1 hour)

---

## Files Created/Modified

### Modified
- `include/hgn/eh_beam_search.h` - Added EH_QueryContext, hub_penalty
- `src/hgn/eh_beam_search.c` - Hybrid scoring, hub penalty, init

### New
- `examples/eh_g3_trainer.py` - Contrastive trainer
- `training/medical_qa.txt` - Domain corpus (to create)
- `training/legal_qa.txt` - Domain corpus (to create)
- `training/technical_qa.txt` - Domain corpus (to create)

---

## Next Immediate Steps

1. Test trainer on mega_qa.txt:
   ```bash
   python examples/eh_g3_trainer.py training/mega_qa.txt training/eh_g3_embeddings.npy
   ```

2. Create domain-specific corpus scripts

3. Train full 1000+ corpus

4. Integrate embeddings into beam_search callback

5. Benchmark and report accuracy

