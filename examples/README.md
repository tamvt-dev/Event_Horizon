# EventHorizon Engine - Examples

This directory contains practical examples demonstrating EventHorizon Engine features.

## 📋 Available Examples

### C Examples

| Example | Description | Features Demonstrated |
|---------|-------------|----------------------|
| `hello_world.c` | Minimal working example | Basic inference workflow |
| `arena_demo.c` | Memory arena usage | O(1) allocation, reset, statistics |
| `graph_demo.c` | DAG construction | Multi-node graphs, state collapse |
| `game_ai_npc.c` | **Real-world Game AI** | **NPC behavior, adaptive learning** |

### Python Examples

| Example | Description | Features Demonstrated |
|---------|-------------|----------------------|
| `python_demo.py` | Python binding usage | NumPy integration, benchmarking |

## 🚀 Building Examples

### Build All
```bash
make
```

### Build Specific Example
```bash
make hello_world
make arena_demo
make graph_demo
make game_ai_npc
```

### Clean
```bash
make clean
```

## 📖 Running Examples

### Hello World
```bash
./hello_world
```

**Output:**
```
EventHorizon Engine - Hello World
==================================

Step 1: Creating DAG with single node...
   ✓ DAG created (1 node, 16x16 weights)
...
Success! EventHorizon is working.
```

### Arena Demo
```bash
./arena_demo
```

Demonstrates:
- O(1) pointer-bump allocation
- Million allocations/sec throughput
- O(1) reset regardless of allocation count
- Memory statistics

### Graph Demo
```bash
./graph_demo
```

Demonstrates:
- Building multi-node DAG
- State collapse evaluation
- Frobenius norm computation
- Adaptive O(N) vs O(N²) activation

### Python Demo
```bash
python3 python_demo.py
```

Requires: Python bindings installed
```bash
cd ../bindings/python
pip install -e .
```

## 💡 Learning Path

### Beginners Start Here
1. **hello_world.c** - Understand basic workflow
2. **arena_demo.c** - Learn memory management
3. **graph_demo.c** - Understand DAG structure

### Intermediate
4. **python_demo.py** - Language bindings
5. Check `../src/main_test.c` - More complex scenarios
6. Study `../bench_all.c` - Performance optimization

### Advanced
7. Modify examples to experiment
8. Build custom DAG structures
9. Implement domain-specific pipelines

## 🎯 Example Use Cases

### hello_world.c
**Use when:** Learning the basics, testing installation

**Teaches:**
- Minimal setup code
- Single inference execution
- Proper resource cleanup

### arena_demo.c
**Use when:** Understanding memory management

**Teaches:**
- Zero-allocation hot paths
- Memory arena patterns
- O(1) operations

### graph_demo.c
**Use when:** Building complex models

**Teaches:**
- DAG construction
- State collapse mechanism
- Expert routing patterns

### python_demo.py
**Use when:** Integrating with Python/NumPy workflows

**Teaches:**
- Python bindings usage
- Batch processing
- Benchmarking

## 🔧 Customizing Examples

### Modify Dimensions
```c
const int INPUT_DIM = 32;   // Change to your size
const int OUTPUT_DIM = 64;  // Different output size
```

### Add More Nodes
```c
EH_DAGNode *node3 = eh_dag_create_node(3, DIM, DIM);
eh_dag_connect_nodes(node2, node3);
```

### Change Collapse Threshold
```c
float threshold = 0.3f;  // More aggressive collapse
eh_dag_evaluate_collapse(node, threshold);
```

## 📚 Further Reading

- [QUICKSTART.md](../QUICKSTART.md) - Quick start guide
- [README.md](../README.md) - Complete documentation
- [CONTRIBUTING.md](../CONTRIBUTING.md) - Contributing guidelines

## 🐛 Troubleshooting

### Compilation Errors
```bash
# Make sure you're in examples/ directory
cd examples

# Verify core library is built
ls ../src/core/*.c

# Try rebuilding from scratch
make clean
make
```

### Runtime Errors
**"Failed to create node"**
- Check memory availability
- Verify dimensions are valid (> 0)

**"Inference failed"**
- Verify input/output dimensions match
- Check context initialization succeeded

## 💬 Questions?

- Check [DOCUMENTATION.md](../DOCUMENTATION.md) for navigation
- Open an issue on GitHub
- See [CONTRIBUTING.md](../CONTRIBUTING.md) for community guidelines
