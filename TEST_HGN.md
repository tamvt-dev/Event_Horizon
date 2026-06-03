# 🧪 How to Test HGN Engine

3 ways to test the HGN module - from quickest to most detailed.

---

## ⚡ Option 1: Quick Test (30 seconds)

Run unit tests to verify all layers work:

```bash
cd tests
make test_eh_hgn_engine
./test_eh_hgn_engine
```

**Expected:**
```
=== 41/41 passed ===
```

---

## 🚀 Option 2: Demo App (2 minutes)

### 2.1. Compile demo

```bash
cd /mnt/c/Users/Administrator/Desktop/my_project/eventhorizon

gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I include \
    examples/hgn_inference_demo.c \
    src/hgn/eh_hgn_engine.c \
    src/hgn/eh_hgn_collapse.c \
    src/hgn/eh_hgn_dag.c \
    src/hgn/eh_beam_search.c \
    src/core/eh_arena.c \
    -o hgn_inference_demo -lm
```

### 2.2. Run demo

```bash
# Use test DAG from unit tests
./hgn_inference_demo /tmp/test_engine_dag.bin 0
```

**Output will show:**
- ✅ DAG loading (Layer 1)
- ✅ Collapse gating stats (Layer 2)
- ✅ Beam search results (Layer 3)
- ✅ Full inference pipeline (Layer 4)

**Example output:**
```
=== Generation Results ===

Beam 0: score=1394.156, len=3, finished=YES
  Tokens: [0, 2, 3]

Beam 1: score=639.716, len=3, finished=YES
  Tokens: [0, 1, 3]

=== EH_HGN_InferenceSession Stats ===
  Steps executed    : 2
  Collapse enabled  : YES
  collapse   : 1 (50.0% FLOPs saved)  ← Layer 2 working!
  mutants    : 0 spawned               ← Layer 2 monitoring
```

---

## 🔬 Option 3: Test All Layers (5 minutes)

Run complete test suite:

```bash
cd tests
make clean
make test
```

**Output:**
```
Core Module:
  • test_arena:    12/12 ✅
  • test_dag:       3/3  ✅
  • test_engine:    2/2  ✅

HGN Module:
  • Layer 1 (DAG):       33/33 ✅
  • Layer 2 (Collapse):  37/37 ✅
  • Layer 3 (Beam):       6/6  ✅
  • Layer 4 (Engine):    41/41 ✅

Total: 134/134 tests passing (100%)
```

---

## 📊 Understanding Results

### Demo Output Explained:

**1. Generation Results:**
```
Beam 0: score=1394.156, len=3, finished=YES
  Tokens: [0, 2, 3]
```
- Engine returns **all K=4 beams** (doesn't force top-1)
- You can choose beam based on score, length, or other heuristics

**2. Collapse Stats:**
```
collapse   : 1 (50.0% FLOPs saved)
expand     : 1
```
- **50% steps use mean pooling** (Layer 2 collapse)
- **50% steps use full DAG** (Layer 1)
- Adaptive: auto-decides based on `cos(h_t, h_{t-1})`

**3. Mutant Stats:**
```
mutants    : 0 spawned, 0 active
```
- No OOD signal (beam entropy low)
- If entropy > 1.80 → mutants will spawn

**4. Memory:**
```
Used: 6.20 KB (0.0%)
Allocs: 5 total
```
- **Zero malloc per step** after init
- All from arena (Layer 0)

---

## 🎯 What's Being Tested?

### Layer 1: CSR DAG
- ✅ Zero-copy loading
- ✅ Edge iteration
- ✅ Node embeddings
- ✅ AVX2-aligned structures

### Layer 2: Collapse Gating
- ✅ Cosine similarity gating
- ✅ Mean pooling (COLLAPSE mode)
- ✅ Mutant node spawning
- ✅ Entropy calculation

### Layer 3: Beam Search
- ✅ K=4 beam tracking
- ✅ Autoregressive expansion
- ✅ AVX2 scoring
- ✅ Terminal detection

### Layer 4: Unified Engine
- ✅ Session lifecycle
- ✅ Config management
- ✅ Full pipeline orchestration
- ✅ Multi-beam output

---

## 🐛 If Tests Fail?

### Compilation Error?
```bash
# Check for AVX2 support
gcc -march=native -dM -E - < /dev/null | grep AVX2

# If no AVX2, remove -mavx2 flag
gcc -O3 -std=c99 ... (remove -mavx2)
```

### Test DAG Not Found?
```bash
# Unit test auto-creates /tmp/test_engine_dag.bin
# Run test first to create file
cd tests
./test_eh_hgn_engine  # Creates /tmp/test_engine_dag.bin

# Then run demo
cd ..
./hgn_inference_demo /tmp/test_engine_dag.bin 0
```

### Arena OOM?
```c
// Increase arena size in code
EH_Arena *arena = eh_arena_create(1024 * 1024 * 1024);  // 1GB
```

---

## 📚 Next Steps

1. ✅ **Tests pass** → Read `HGN_QUICKSTART.md` for integration
2. ✅ **Demo works** → Read `HGN_MODULE.md` for API details
3. ✅ **Understand architecture** → Study source code in `src/hgn/`

**Want to customize?**
- Thresholds: `config.collapse_thresh`, `config.entropy_thresh`
- Beam width: Edit `EH_BEAM_WIDTH` in `eh_beam_search.h`
- Max steps: `config.max_steps = 100;`

---

## 🎓 Understanding the Output

**When you see:**
```
collapse   : 10 (83.3% FLOPs saved)
expand     : 2
```

**It means:**
- 10 steps used **mean pooling O(N)** instead of full DAG traversal
- 2 steps used **full expansion** (context changed significantly)
- **83.3% FLOPs reduction** compared to always-expand baseline

**This is Layer 2 (Collapse Gating) working!**

---

**Ready to integrate?** → See `HGN_QUICKSTART.md`  
**Want details?** → See `HGN_MODULE.md`  
**Questions?** → Check source comments in `src/hgn/eh_hgn_engine.c`
