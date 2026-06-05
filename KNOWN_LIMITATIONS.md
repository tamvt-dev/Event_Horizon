# Known Limitations (Pre-Release)

**Last Updated**: EH-G3 optimization phase
**Status**: Documented issues before v0.3.0 release

---

## 🚨 Critical Issues (Must Address Before v1.0)

### 1. Test Coverage Insufficient

**Issue**: Only 9-question test set for production claims

**Impact**: "100% coherent" and "44% accuracy" based on very small sample

**Risk**: High - May not generalize to real usage

**Fix Required**:
- Minimum 50 questions before claiming production-ready
- Split: 30 train/dev, 20 test (unseen)
- Cover multiple domains: general, technical, conversational

**Timeline**: 1 week

**Priority**: P0 (blocking v1.0 release)

---

### 2. Embedding Quality Poor

**Issue**: Embedding-based fallback finds semantically wrong pairs
- "what is a computer" → (low,low) 
- "what is a database" → (low,low)
- Not related to query at all

**Root Cause**: Embeddings trained for sequential prediction, not query matching

**Impact**: 7/9 queries use fallback → quality depends on embeddings

**Fix Required**:
- Query-aware training (EH-G4): Train with (Q,A) pairs
- Contrastive loss: maximize similarity(Q, correct_A)
- OR: Expand corpus to reduce fallback rate

**Timeline**: 2 weeks (query-aware training)

**Priority**: P0 (quality issue)

---

### 3. Coherence vs Semantic Accuracy Conflated

**Issue**: "100% coherent" is misleading

**Examples**:
- "made questions afloat maintain" - grammatically structured ✅
- But semantically wrong for "what is a computer" ❌

**Fix Required**: Split metrics:
- **Structural coherence**: Grammar, no repetition (current 100%)
- **Semantic relevance**: Correct domain/topic (current ~40%)
- **Factual accuracy**: Correct answer (current ~11%)

**Timeline**: Documentation update (30 min)

**Priority**: P1 (clarity)

---

## ⚠️ Architecture Issues

### 4. max_steps=6 is Magic Number

**Issue**: Tuned on 9-question test set, not validated on broader distribution

**Risk**: May not generalize to:
- Longer answers (explanations)
- Shorter answers (yes/no)
- Different domains

**Fix Required**:
- Test on 100+ questions across domains
- Make max_steps adaptive based on question type
- OR: Document as "optimized for 5-7 token answers"

**Timeline**: 2 days testing

**Priority**: P1

---

### 5. Prior Dominance Blocks Further Tuning

**Issue**: All parameter changes (temp, rep_penalty, hub_penalty) have zero effect

**Root Cause**: Prior weights (10-20 range) >> context (1-5) >> penalties (0-2)

**Impact**: Hit tuning ceiling at 44% accuracy

**Fix Required**: Architectural change needed:
- Retrain with balanced priors
- Two-stage inference (attention filter → prior generation)
- Query-aware embeddings (EH-G4)

**Timeline**: 2-4 weeks (EH-G4)

**Priority**: P2 (documented limitation)

---

### 6. Graph Coverage Too Small

**Issue**: 1345 pairs in graph, but vocab=913 words → 99.8% of possible pairs missing

**Impact**: 7/9 queries hit fallback

**Examples of Missing Patterns**:
- (what, is) - universal question pattern
- (is, a) - hub pattern
- (a, computer), (a, database) - technical domain

**Fix Required**:
- Expand corpus with targeted Q&A pairs (500+)
- Focus on common question patterns
- Add domain coverage (tech, geography, general knowledge)

**Timeline**: 1 week corpus curation + 2 hours retraining

**Priority**: P0 (quality blocker)

---

## 🔧 Technical Debt

### 7. Arena Memory Not Reclaimed

**Issue**: Arena allocator uses bump pointer, no reclaim on reset

**Impact**: Multiple inference sessions exhaust arena without process restart

**Current Workaround**: Single-shot inference or process restart

**Fix Required**:
- Implement arena checkpoint/rewind
- OR: Document single-use limitation clearly
- OR: Switch to malloc/free for production

**Timeline**: 4 hours

**Priority**: P1 (usability)

---

### 8. EH_BEAM_WIDTH Hardcoded

**Issue**: Beam width K=8 compiled-in, can't configure at runtime

**Impact**: Edge devices (ESP32, K=2) must recompile

**Fix Required**:
- Add to `EH_HGN_EngineConfig`
- Allocate beam arrays dynamically
- Document memory scaling: RAM = K × max_steps × embedding_dim

**Timeline**: 2 hours

**Priority**: P2 (edge deployment)

---

### 9. Deprecated Code Still in Build

**Issue**: `eh_adaptive.c` deprecated but still linked (29 tests)

**Impact**: 
- Binary size bloat
- Confusion for users
- Maintenance burden

**Fix Required**:
- Remove deprecated code
- OR: Move to `legacy/` folder with clear docs
- Update build system

**Timeline**: 1 hour

**Priority**: P2 (cleanup)

---

## 📚 Documentation Gaps

### 10. EH_HGN_FOR_EDGES Macro Undocumented

**Issue**: Edge iteration macro not explained in API docs

**Confusion**: Is `edge` a pointer or copy?

**Fix Required**:
- Add to API documentation
- Document that `edge` is pointer to internal struct
- Show example usage

**Timeline**: 30 min

**Priority**: P2

---

### 11. File Format Has No Version Field

**Issue**: Model files (`.ehdag`) lack version/magic bytes

**Impact**: Quantization (INT8/INT4) will break backward compatibility

**Fix Required**:
- Add file format version to header
- Magic bytes: "EHGN" + version (uint32_t)
- Document format spec

**Timeline**: 2 hours (format change + migration script)

**Priority**: P1 (future-proofing)

---

## 🔮 Roadmap Gaps

### 12. No Multi-threading Plan

**Issue**: Arena allocator not thread-safe (single pointer bump)

**Impact**: Cannot parallelize inference on multi-core systems

**Options**:
1. Thread-local arenas (simplest)
2. Lock-free arena with CAS
3. Per-query arena allocation

**Timeline**: 1 week design + 2 weeks implementation

**Priority**: P3 (future feature)

---

### 13. Quantization Plan Incomplete

**Issue**: INT8/INT4 listed as future work but no concrete plan

**Blockers**:
- File format needs versioning (issue #11)
- Need accuracy vs size tradeoff benchmarks
- AVX2/NEON quantized ops not implemented

**Timeline**: 2-4 weeks

**Priority**: P3 (ESP32 deployment)

---

### 14. No ONNX/TFLite Converter

**Issue**: Cannot export to standard formats

**Impact**: Limited ecosystem integration

**Options**:
- Write custom exporter (2 weeks)
- OR: Document as "native format only"
- OR: Provide Python API for inference

**Timeline**: 2 weeks

**Priority**: P3 (ecosystem)

---

## 🎯 Pre-Merge Action Items

### Must Fix (P0):
- [ ] Expand test set: 9 → 50+ questions
- [ ] Expand corpus: Add 500+ Q&A pairs with common patterns
- [ ] Document coherence vs semantic accuracy split
- [ ] Fix embedding quality OR reduce fallback rate

### Should Fix (P1):
- [ ] Add file format version field
- [ ] Document arena memory limitation
- [ ] Split coherence metrics in reporting
- [ ] Validate max_steps=6 on broader dataset

### Can Defer (P2):
- [ ] Make EH_BEAM_WIDTH configurable
- [ ] Remove deprecated code
- [ ] Document EH_HGN_FOR_EDGES
- [ ] Prior dominance architectural fix (EH-G4)

### Future Work (P3):
- [ ] Multi-threading support
- [ ] Quantization implementation
- [ ] ONNX/TFLite export

---

## 📊 Current Quality Metrics (Honest Assessment)

| Metric | Value | Interpretation |
|--------|-------|----------------|
| **Structural coherence** | 100% (9/9) | No repetition, clean termination ✅ |
| **Semantic relevance** | 56% (5/9) | Correct domain/topic 🟡 |
| **Factual accuracy** | 11% (1/9) | Correct answer ❌ |
| **Test coverage** | 9 questions | Too small for production claims ❌ |
| **Corpus coverage** | 0.2% of pairs | Missing 99.8% of possible patterns ❌ |
| **Fallback rate** | 78% (7/9) | Most queries use semantic fallback 🟡 |

---

## 🎓 Lessons Learned

### What Worked:
1. **max_steps=6** eliminated repetition (critical insight!)
2. **Multi-word pair matching** improved from 0% → 56% coherence
3. **Embedding-based fallback** better than random (but still poor quality)

### What Didn't Work:
1. **Parameter tuning** hit ceiling (prior dominance)
2. **Hub penalty** had zero effect
3. **Small corpus** limits quality fundamentally

### What's Next:
1. **Expand corpus** (fastest path to 70%)
2. **Query-aware training** (EH-G4, path to 85%+)
3. **Better test coverage** (honest quality assessment)

---

## ⚠️ Disclaimer for Current Release

**EventHorizon EH-G3 is a research prototype, not production software.**

**Use cases it's good for**:
- Educational projects
- TinyML research
- Limited-domain FAQ (with corpus expansion)
- Embedded systems experimentation

**Use cases it's NOT ready for**:
- Open-domain Q&A
- Production chatbots
- Mission-critical systems
- Factual information retrieval

**Minimum requirements before production**:
1. 50+ question test set with honest metrics
2. 500+ Q&A pair corpus (cover common patterns)
3. Query-aware embeddings OR 90%+ valid pair rate
4. Arena memory management documented/fixed

---

**Status**: Pre-alpha research code

**Confidence**: Medium for research, Low for production

**Recommendation**: Use for learning and experimentation only

**Target**: v0.3.0-alpha (research release)

**Next milestone**: v0.5.0-beta (50+ test set, expanded corpus)

**Production target**: v1.0.0 (85%+ accuracy, 100+ test set)
