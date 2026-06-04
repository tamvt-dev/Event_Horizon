# EH-G3 Action Plan

## Current Status

✅ **Branch Created**: `feature/eh-g3-parameter-tuning`

✅ **Breakthrough Found**: Config 6 (max_steps=6) eliminates repetition!

✅ **Proof of Concept**: "what is the capital of france" → "france paris your" ✅ CORRECT!

---

## Immediate Next Steps (Priority Order)

### 1. Validate max_steps=6 Globally (15 min) ⭐⭐⭐

**Goal**: Confirm Config 6 works across all 9 test questions

**Action**: Modify Phase 2 baseline parameters to use `max_steps=6`

**Expected**:
- All questions terminate cleanly (7 tokens avg)
- "capital of france" still correct
- Hub collisions still fail (tokenization issue)

**Files to modify**:
```c
// examples/qa_attention_g3.c, line ~560
TuningParams baseline_params = {0.3f, 0.0f, 0.6f, 5.0f, 6};  // Change 10 → 6
```

---

### 2. Test Aggressive Hub Penalty (30 min) ⭐⭐

**Goal**: Find penalty strength that affects output

**Test Matrix**:
```
hub_penalty = 5.0  (2.5× current)
hub_penalty = 10.0 (5× current)
hub_penalty = 20.0 (10× current)
hub_penalty = 50.0 (25× current)
```

**Implementation**: Already supported, just change parameter

**Expected**: At 20.0+, hub nodes should be penalized enough to change paths

---

### 3. Test Attention Mix Extremes (30 min) ⭐⭐

**Goal**: See if high global attention helps hub collisions

**Test Matrix**:
```
attention_mix = 0.6 (60% global)
attention_mix = 0.7 (70% global)
attention_mix = 0.8 (80% global)
attention_mix = 1.0 (100% global, pure query matching)
```

**Expected**: May fix "what is a computer" if embeddings are good

---

### 4. Fix Tokenization for Multi-Word Pairs (2 hours) ⭐⭐⭐

**Problem**: "what is a computer" starts from token fallback instead of pair

**Current logic**:
```c
// Only tries last 2 tokens
if (question_tokens.count >= 2) {
    pair_id = find_pair_id(tokens[n-2], tokens[n-1]);
}
```

**Solution A: Try All Consecutive Pairs**
```c
// Try all pairs from right to left
int start_pair_id = -1;
for (int i = question_tokens.count - 2; i >= 0 && start_pair_id < 0; i--) {
    start_pair_id = find_pair_id(tokens[i], tokens[i+1]);
}
```

**Solution B: Skip Common Hub Pairs**
```c
// Blacklist: (is, a), (are, the), etc.
int blacklist[] = {hub_pair_1, hub_pair_2, ...};
if (is_blacklisted(pair_id)) {
    try_next_pair();
}
```

**Recommendation**: Try Solution A first (most general)

---

### 5. Combined Best Configuration (1 hour) ⭐⭐⭐

**After finding optimal parameters, test combined config**:

**Predicted best config**:
```
max_steps: 6
attention_mix: 0.7 (higher global)
hub_penalty: 20.0 (strong penalty)
temperature: 0.6 (unchanged)
rep_penalty: 5.0 (unchanged)
```

**Expected**: Best overall accuracy on 9-question benchmark

---

## Test Execution Plan

### Round 1: Quick Validation (15 min)
```bash
# Modify qa_attention_g3.c: baseline_params max_steps = 6
# Recompile
wsl bash -c "cd eventhorizon && gcc -O3 ... -o qa_attention_g3"

# Run
./qa_attention_g3 trigram_v2_model.ehdag

# Verify:
# - "capital of france" still correct
# - All answers ~7 tokens
# - Clean termination
```

---

### Round 2: Hub Penalty Sweep (30 min)

**Create test script**: `test_hub_penalty.sh`

```bash
#!/bin/bash
for penalty in 5.0 10.0 20.0 50.0; do
    echo "Testing hub_penalty=$penalty"
    # Modify source or add command-line parameter
    # Run and capture output
done
```

**Analysis**: Check if any config changes "my your" output

---

### Round 3: Attention Sweep (30 min)

**Create test script**: `test_attention_mix.sh`

```bash
#!/bin/bash
for mix in 0.6 0.7 0.8 1.0; do
    echo "Testing attention_mix=$mix"
    # Run and capture output
done
```

**Analysis**: Check if high attention fixes hub collisions

---

### Round 4: Tokenization Fix (2 hours)

**Steps**:
1. Implement multi-word pair matching in `qa_attention_g3.c`
2. Add debug output to show which pair was selected
3. Recompile and test
4. Verify "what is a computer" starts from correct pair

**Success criteria**:
- "what is a computer" → NOT "my your"
- Proper pair found (e.g., (a, computer))
- Answer is semantically relevant

---

## Success Metrics

### Phase 1: Validation ✅
- [x] Config 6 confirmed working
- [ ] max_steps=6 validated globally
- [ ] "capital of france" robust across configs

### Phase 2: Optimization 🎯
- [ ] Hub penalty has measurable effect
- [ ] Attention_mix 0.7-0.8 improves hub collisions
- [ ] No repetition in any answers

### Phase 3: Fix Tokenization 🔧
- [ ] Multi-word pair matching implemented
- [ ] "what is a computer" starts from correct pair
- [ ] Hub collision questions produce relevant answers

### Phase 4: Final Config ⭐
- [ ] 70%+ accuracy on 9-question benchmark
- [ ] "capital of france" still perfect
- [ ] All answers < 10 tokens
- [ ] No repetition loops

---

## Timeline Estimate

| Task | Duration | Priority |
|------|----------|----------|
| Validate max_steps=6 | 15 min | ⭐⭐⭐ |
| Hub penalty sweep | 30 min | ⭐⭐ |
| Attention mix sweep | 30 min | ⭐⭐ |
| Fix tokenization | 2 hours | ⭐⭐⭐ |
| Combined testing | 1 hour | ⭐⭐⭐ |
| Documentation | 30 min | ⭐ |
| **TOTAL** | **~5 hours** | - |

---

## Git Workflow

### Current State
```
Branch: feature/eh-g3-parameter-tuning
Commits: 2
  - Initial commit (EH-G2 baseline)
  - Parameter tuning results
```

### Upcoming Commits

**Commit 3: Validate max_steps=6**
```
"EH-G3: Validate max_steps=6 across all questions

- Modified baseline_params to use steps=6
- Confirmed clean termination for all cases
- capital of france still correct
- Ready for aggressive parameter testing"
```

**Commit 4: Hub penalty and attention sweeps**
```
"EH-G3: Aggressive parameter exploration

- Tested hub_penalty: 5.0, 10.0, 20.0, 50.0
- Tested attention_mix: 0.6, 0.7, 0.8, 1.0
- Found optimal: penalty=X, mix=Y
- Results documented in PARAMETER_TUNING_RESULTS.md"
```

**Commit 5: Tokenization fix**
```
"EH-G3: Fix multi-word pair matching for tokenization

- Implemented try-all-pairs strategy
- what is a computer now starts from correct pair
- Hub collision cases improved
- Added debug output for pair selection"
```

**Commit 6: Final configuration**
```
"EH-G3: Final optimized configuration

- Best config: steps=6, mix=X, penalty=Y
- Accuracy: Z% on 9-question benchmark
- All answers clean, no repetition
- Ready for merge to master"
```

---

## Questions to Answer

### Q1: Why does Config 6 work?
**Hypothesis**: Model trained on answers ~5-7 tokens long. Forcing longer causes loops.

**Validation**: Check training corpus average answer length.

---

### Q2: Why do Configs 1-5 produce identical output?
**Answer**: Prior dominance. Prior weights (10-20 range) overwhelm small parameter changes.

**Implication**: Need 10-20× stronger penalties to compete with priors.

---

### Q3: Why does "capital of france" work perfectly?
**Answer**: 
1. Proper pair found: (of, france)
2. Training corpus has this pattern
3. Unique path (low ambiguity)

**Lesson**: Model quality is good when starting point is correct!

---

### Q4: Why do hub collisions all produce "my your"?
**Answer**: Tokenization fallback to token 18 → wrong starting point → trained pattern bypassed.

**Fix**: Multi-word pair matching to find correct start.

---

## Expected Outcomes

### Best Case 🌟
- Tokenization fix solves hub collisions
- Combined config achieves 80%+ accuracy
- All 9 questions produce relevant answers
- Ready for production testing

### Realistic Case ✅
- max_steps=6 eliminates repetition
- Hub penalty/attention helps some cases
- 60-70% accuracy on benchmark
- Clear path to further improvement

### Worst Case ⚠️
- Only "capital of france" works
- Hub collisions unsolvable without retraining
- Tokenization fix doesn't help
- Need query-aware training (EH-G3 Phase 2)

---

## Documentation Checklist

### Created ✅
- [x] EH-G3_PARAMETER_TUNING_RESULTS.md (comprehensive analysis)
- [x] EH-G3_ACTION_PLAN.md (this document)

### To Create 📝
- [ ] EH-G3_OPTIMAL_CONFIG.md (final best parameters)
- [ ] EH-G3_TOKENIZATION_FIX.md (pair matching implementation)
- [ ] EH-G3_SUMMARY.md (executive summary for merge)

### To Update 📝
- [ ] README.md (add EH-G3 section)
- [ ] ROADMAP.md (mark EH-G3 complete)
- [ ] CHANGELOG.md (add EH-G3 release notes)

---

## Ready to Proceed!

**Status**: ✅ Branch created, analysis complete, action plan ready

**Next command**: Modify `qa_attention_g3.c` to validate max_steps=6

**Expected time to completion**: ~5 hours of focused work

**Confidence**: Very high - have working proof of concept ("capital of france")

---

🎯 **Goal: Achieve 70%+ accuracy with clean, repetition-free answers!** 🎯
