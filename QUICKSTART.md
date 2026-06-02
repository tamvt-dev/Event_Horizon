# EventHorizon Engine - Quick Start Guide

> **Note**: This project was developed with AI assistance (Claude, GPT, Gemini), showcasing human-AI collaboration in systems programming.

---

## 🚀 Get Started in 30 Seconds

### Option 1: Using Make (Linux/WSL)
```bash
# Run benchmark
make run-bench

# Run tests
make run-test
```

### Option 2: Using PowerShell (Windows)
```powershell
# Run benchmark
.\run.ps1

# Run tests
.\run.ps1 test
```

### Option 3: Direct Execution
```bash
# Run benchmark directly
./eh_engine_ultimate_bench

# Run neuro test
./eh_neuro_test
```

---

## 📋 Common Commands

### Build Commands
```bash
make bench        # Build benchmark suite
make test         # Build test suite
make neuro        # Build neuro test
make examples     # Build all examples
make build-all    # Build everything
make clean        # Clean artifacts
make help         # Show all commands
```

### Run Tests
```bash
cd tests
make              # Build all tests
make test         # Run all tests
make clean        # Clean test artifacts
```

### Run Examples
```bash
cd examples
make              # Build all examples
./hello_world     # Run minimal example
./arena_demo      # Run arena demo
./graph_demo      # Run graph demo
python python_demo.py  # Run Python binding demo
```

### PowerShell Commands
```powershell
.\run.ps1 bench   # Run benchmark
.\run.ps1 test    # Run tests
.\run.ps1 neuro   # Run neuro test
.\run.ps1 build   # Build all
.\run.ps1 clean   # Clean
.\run.ps1 help    # Show help
```

---

## 🔧 Manual Compilation

### Benchmark Suite
```bash
gcc -O3 -std=c99 -march=native -Wall -Wextra \
    -Iinclude src/core/*.c bench_all.c \
    -o eh_engine_ultimate_bench -lm
```

### Test Suite
```bash
gcc -O3 -std=c99 -Wall -Wextra \
    -Iinclude src/core/*.c src/main_test.c \
    -o eh_test -lm
```

### Neuro Test
```bash
gcc -O3 -std=c99 -Wall -Wextra \
    -Iinclude src/core/*.c src/test_neuro.c \
    -o eh_neuro_test -lm
```

---

## 📊 Expected Benchmark Results

```
Memory Arena:     24.84 Million allocs/sec
Tokenizer:        213 MB/s, 133M tokens/sec
Graph Ops:        377 Million ops/sec
Neuroplasticity:  9.42 Million feedbacks/sec
Inference Speed:  278,133 passes/sec
Collapse Ratio:   93.58%
FLOPs Saved:      93.21%
Energy:           0.054 mJ/inference
```

---

## 🎯 Quick API Example

```c
#include "eh_engine.h"
#include "eh_arena.h"

int main(void) {
    // 1. Initialize arena
    EH_Arena *arena = eh_arena_create(64 * 1024 * 1024);

    // 2. Create DAG
    EH_DAGNode *root = eh_dag_create_node(0, 128, 128);
    
    // 3. Initialize scoring core
    EH_ScoringCore *scorer = eh_scoring_init(128);

    // 4. Setup engine
    EH_Context *ctx = eh_engine_setup(root, scorer, 0.5f);

    // 5. Run inference
    float input[128] = {0};
    float output[128] = {0};
    eh_engine_inference(ctx, input, 128, output, 128);

    // 6. Cleanup
    eh_engine_shutdown(ctx);
    eh_arena_destroy(arena);
    
    return 0;
}
```

---

## 🐛 Troubleshooting

### "make: command not found"
```bash
# Install build tools
sudo apt-get update
sudo apt-get install build-essential
```

### "gcc: command not found"
```bash
# Install GCC
sudo apt-get install gcc
```

### Permission denied
```bash
# Make executable
chmod +x eh_engine_ultimate_bench
chmod +x run.ps1
```

### WSL not found
```powershell
# Install WSL
wsl --install
```

---

## 📚 Next Steps

1. **Run Examples**: Check `examples/` directory for working demos
   - `hello_world.c` - Minimal working example
   - `arena_demo.c` - Memory arena usage
   - `graph_demo.c` - DAG construction
   - `python_demo.py` - Python bindings

2. **Run Tests**: Check `tests/` directory for unit tests
   - `test_arena.c` - Arena allocator tests (7 tests)
   - `test_dag.c` - DAG operations tests (3 tests)
   - `test_engine.c` - Engine tests (2 tests)

3. Read [README.md](README.md) for detailed documentation
4. Check [CHANGELOG.md](CHANGELOG.md) for recent updates
5. Explore [ROADMAP.md](ROADMAP.md) for planned features
6. Review header files in `include/` for API reference

---

## 💡 Tips

- Use `-march=native` for best performance on your CPU
- Enable performance governor on Linux for consistent benchmarks
- Adjust `EH_ARENA_SIZE` for memory-constrained devices
- Use `EH_DEBUG` flag during development for verbose logging

---

## 🆘 Need Help?

- Check the [README.md](README.md) for comprehensive documentation
- Review example code in `src/main_test.c` and `src/test_neuro.c`
- Examine benchmark code in `bench_all.c` for advanced usage
