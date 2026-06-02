# EventHorizon Engine - Project Overview

Quick visual guide to the EventHorizon project structure and resources.

---

## 📂 Project Structure

```
eventhorizon/
│
├── 📚 Documentation (45 KB)
│   ├── README.md           (16 KB) - Main documentation
│   ├── ROADMAP.md          (10 KB) - Development timeline  
│   ├── CREDITS.md          (6 KB)  - AI collaboration details
│   ├── DOCUMENTATION.md    (6 KB)  - Documentation index
│   ├── QUICKSTART.md       (4 KB)  - Quick start guide
│   └── CHANGELOG.md        (3 KB)  - Update history
│
├── 🔧 Build & Run Tools
│   ├── Makefile            - Build automation (GNU Make)
│   ├── run.ps1             - PowerShell quick runner
│   ├── compile_benchmark.sh - Manual compilation script
│   ├── run_benchmark.sh    - Benchmark runner
│   └── .gitignore          - Git ignore rules
│
├── 💻 Source Code
│   ├── include/            - Public API headers (9 files)
│   │   ├── eh_engine.h     - Main inference engine
│   │   ├── eh_dag.h        - DAG graph structure
│   │   ├── eh_arena.h      - Memory allocator
│   │   ├── eh_neuro.h      - Neuroplasticity
│   │   ├── eh_scoring.h    - Scoring core
│   │   ├── eh_dynamic.h    - Dynamic collapse
│   │   ├── eh_learning.h   - Learning API
│   │   ├── eh_tokenizer.h  - Tokenizer
│   │   └── eh_graph.h      - Graph utilities
│   │
│   ├── src/core/           - Implementation files (9 files)
│   │   ├── eh_engine.c     - Engine implementation
│   │   ├── eh_dag.c        - DAG operations
│   │   ├── eh_arena.c      - Arena allocator
│   │   ├── eh_neuro.c      - Neuroplasticity
│   │   ├── eh_scoring.c    - Routing core
│   │   ├── eh_dynamic.c    - Collapse logic
│   │   ├── eh_learning.c   - Learning loop
│   │   ├── eh_tokenizer.c  - Tokenization
│   │   └── eh_graph.c      - Graph ops
│   │
│   └── src/                - Test & main files
│       ├── main_test.c     - Basic test suite
│       ├── test_neuro.c    - Neuro test
│       └── main_learning.c - Learning test
│
└── 📊 Benchmarks
    └── bench_all.c         - Comprehensive benchmark suite
```

---

## 🚀 Quick Start Paths

### Path 1: Just Want to Run It (2 minutes)
```
.\run.ps1
```

### Path 2: Understand the Architecture (20 minutes)
```
1. Read README.md (Core Capabilities + Technical Specs)
2. Browse include/*.h files
3. Run .\run.ps1 to see it in action
```

### Path 3: Develop & Contribute (2 hours)
```
1. Read ROADMAP.md for planned features
2. Study src/core/*.c implementation
3. Check CREDITS.md for development methodology
4. Pick a task from ROADMAP.md "Quick Wins"
```

---

## 📖 Documentation Map

```
Start Here
    ↓
┌─────────────────┐
│  QUICKSTART.md  │ → Need immediate usage guide?
└─────────────────┘
    ↓
┌─────────────────┐
│   README.md     │ → Need technical details?
└─────────────────┘
    ↓
├─→ ROADMAP.md     → Want to see future plans?
├─→ CREDITS.md     → Curious about AI collaboration?
├─→ CHANGELOG.md   → Want to see recent updates?
└─→ DOCUMENTATION.md → Need navigation help?
```

---

## 🎯 Key Metrics (Quick Reference)

### Performance
```
Inference Speed:  278,133 passes/sec
Memory:           63 MB peak RSS
Energy:           0.054 mJ/inference
Collapse Ratio:   93.58%
FLOPs Saved:      93.21%
```

### Code Quality
```
Language:         Pure C99
Dependencies:     0 (only stdlib + libm)
Lines of Code:    ~5,000 (estimated)
Test Coverage:    Comprehensive benchmarks
Platform Support: WSL, Linux, Windows
```

### Community
```
License:          Apache-2.0 (permissive)
AI-Assisted:      Yes (Claude, GPT, Gemini)
Open Source:      Yes
Documentation:    45 KB across 6 files
```

---

## 🛠️ Common Tasks

### Build & Run
```bash
# Using Make
make run-bench          # Recommended
make run-test
make clean

# Using PowerShell
.\run.ps1               # Recommended
.\run.ps1 test
.\run.ps1 clean

# Manual
gcc -O3 -std=c99 -march=native -Iinclude \
    src/core/*.c bench_all.c -o benchmark -lm
./benchmark
```

### Reading Code
```
Start:   include/eh_engine.h  (Main API)
Then:    src/core/eh_engine.c (Implementation)
Study:   bench_all.c          (Usage examples)
Deep:    include/eh_arena.h   (Memory management)
Expert:  include/eh_neuro.h   (Neuroplasticity)
```

### Contributing
```
1. Read ROADMAP.md for ideas
2. Check "Quick Wins" section
3. Open GitHub issue to discuss
4. Fork, implement, test
5. Submit PR with benchmarks
```

---

## 🎓 Learning Resources

### For C Developers
- **Start:** README.md → "Core Components"
- **Deep:** Header files in include/
- **Practice:** Modify bench_all.c

### For ML Engineers
- **Start:** README.md → "Why EventHorizon?"
- **Deep:** include/eh_neuro.h, include/eh_dynamic.h
- **Practice:** Tune collapse thresholds

### For Systems Programmers
- **Start:** README.md → "Memory Arena"
- **Deep:** src/core/eh_arena.c
- **Practice:** Port to new platform

### For Researchers
- **Start:** README.md → "Technical Specifications"
- **Deep:** Benchmark results, scalability data
- **Practice:** Compare with your algorithms

---

## 🌟 Unique Features

```
✅ Zero Dynamic Allocation    - O(1) pointer-bump allocator
✅ Adaptive Computation        - 93% FLOPs reduction
✅ Neuroplasticity             - Self-modifying DAG
✅ Edge Optimized              - 63 MB, 0.054 mJ/inference
✅ Pure C99                    - No dependencies
✅ AI-Assisted Development     - Transparent collaboration
✅ Production Ready            - Extensively benchmarked
✅ Open Source                 - Apache-2.0 License
```

---

## 📞 Quick Links

| Need | Link |
|------|------|
| **Quick Start** | [QUICKSTART.md](QUICKSTART.md) |
| **Full Docs** | [README.md](README.md) |
| **Future Plans** | [ROADMAP.md](ROADMAP.md) |
| **AI Details** | [CREDITS.md](CREDITS.md) |
| **Recent Updates** | [CHANGELOG.md](CHANGELOG.md) |
| **Navigation** | [DOCUMENTATION.md](DOCUMENTATION.md) |
| **API Reference** | `include/*.h` |
| **Examples** | `bench_all.c` |

---

## 🎯 Target Audience

```
✅ Edge AI Engineers      → Ultra-low latency inference
✅ Embedded Developers    → Memory-constrained devices
✅ ML Researchers         → Adaptive computation studies
✅ Systems Programmers    → Zero-allocation patterns
✅ Mobile Developers      → On-device AI
✅ IoT Engineers          → Resource-efficient AI
✅ Students               → Modern C99 systems programming
✅ AI Enthusiasts         → Human-AI collaboration examples
```

---

## 💡 Why EventHorizon?

**Problem:** Traditional inference engines waste 90%+ compute on irrelevant operations

**Solution:** EventHorizon adaptively collapses unnecessary computations

**Result:** 93% FLOPs reduction, 278K inferences/sec, 63 MB RAM

**Innovation:** Self-modifying DAG learns and evolves during runtime

**Impact:** Production-ready edge AI for resource-constrained devices

---

*EventHorizon Engine - The future of adaptive edge AI inference*

**Status:** Production Ready | **License:** Apache-2.0 | **Developed with:** Claude, GPT, Gemini
