# HGN Quick Start Guide

## 🚀 Test HGN Engine in 3 Steps

### Step 1: Run Unit Tests

Test all 4 layers (117 tests):

```bash
cd tests
make test
```

Or test Engine layer only (41 tests):

```bash
cd tests
make test_eh_hgn_engine
./test_eh_hgn_engine
```

**Expected output:**
```
=== 41/41 passed ===
```

---

### Step 2: Build Demo App

Compile demo inference app:

```bash
# From EventHorizon root directory
gcc -O3 -std=c99 -mavx2 -Wall -Wextra -I include \
    examples/hgn_inference_demo.c \
    src/hgn/eh_hgn_engine.c \
    src/hgn/eh_hgn_collapse.c \
    src/hgn/eh_hgn_dag.c \
    src/hgn/eh_beam_search.c \
    src/core/eh_arena.c \
    -o hgn_inference_demo -lm
```

---

### Step 3: Run Demo

**Using test DAG:**

```bash
# Demo app uses /tmp/test_engine_dag.bin from unit tests
./hgn_inference_demo /tmp/test_engine_dag.bin 0
```

**Or with complex prompt:**

```bash
./hgn_inference_demo /tmp/test_engine_dag.bin 0 1
```

**Expected output:**

```
=== HGN Inference Demo ===
DAG file     : /tmp/test_engine_dag.bin
Prompt tokens: [0]

[1/5] Creating arena...
[2/5] Loading DAG from /tmp/test_engine_dag.bin...
  Vocab size : 4
  Total edges: 4
  Embed dim  : 128
[3/5] Configuring inference engine...
  Collapse gating: ON (threshold=0.92)
  Mutant nodes   : ON (entropy_thresh=1.80)
  Max steps      : 20
[4/5] Initializing inference session...
  Session ready!

[5/5] Running autoregressive generation...
────────────────────────────────────────
Step  1: active=2, best_score=150.568, seq_len=2
Step  2: active=0, best_score=1394.156, seq_len=3
  → All beams finished (generation complete)
────────────────────────────────────────

=== Generation Results ===

Beam 0: score=1394.156, len=3, finished=YES
  Tokens: [0, 2, 3]

Beam 1: score=639.716, len=3, finished=YES
  Tokens: [0, 1, 3]

=== EH_HGN_InferenceSession Stats ===
  Steps executed    : 2
  Generation done   : YES
  Active beams      : 2
  Collapse enabled  : YES
  Mutants enabled   : YES
  Max steps limit   : 20 (0=unlimited)
=====================================
=== EH_HGN_CollapseCtx Stats ===
  steps      : 2
  collapse   : 1 (50.0% FLOPs saved)
  expand     : 1
  mutants    : 0 spawned, 0 active
  thresh     : collapse=0.920  entropy=1.800
================================

=== Demo Complete ===
```

---

## 📊 Understanding Output

### Generation Results

**2 beams returned:**
- **Beam 0**: score=1394.156, path=[0→2→3] (best)
- **Beam 1**: score=639.716, path=[0→1→3]

Engine **does not force top-1 selection** - you get all K=4 beams!

### Stats Explanation

**Collapse Stats:**
- `collapse   : 1 (50.0% FLOPs saved)` → 1/2 steps used mean pooling
- `expand     : 1` → 1/2 steps used full DAG
- `mutants    : 0 spawned` → No OOD signal (low entropy)

**Memory:**
- `Used: 6.20 KB (0.0%)` → Extremely lightweight with tiny DAG
- `Allocs: 5 total` → Zero malloc per step after init

---

## 🎯 Test with Real DAG

### Create Your Own DAG File

See `tests/test_eh_hgn_engine.c` function `build_test_dag()` for format reference:

**DAG binary format:**
```
[Header: 32 bytes]
  - magic: 0x48474E44 ('HGND')
  - version: 1
  - vocab_size: number of tokens
  - total_edges: total edge count
  - embed_dim: 128
  - max_fanout: K

[Node Embeddings: V × 512 bytes, aligned 32]
  - vec[128]: float embeddings

[Node Adjacency: V × 8 bytes]
  - edge_offset: uint32
  - edge_count: uint32

[Edge Compact: E × 12 bytes]
  - dst: uint32
  - prior: float
  - weight_idx: uint32

[Padding to 32-byte boundary]

[Edge Weights: E × 512 bytes, aligned 32]
  - vec[128]: float weights
```

### Python Script to Generate DAG

```python
import struct
import numpy as np

def create_dag(vocab_size, edges, output_path):
    """
    edges: list of (src, dst, prior, weight_vector[128])
    """
    with open(output_path, 'wb') as f:
        # Header
        magic = 0x48474E44
        version = 1
        total_edges = len(edges)
        embed_dim = 128
        max_fanout = max(len([e for e in edges if e[0] == i]) 
                        for i in range(vocab_size))
        
        f.write(struct.pack('IIIIII', magic, version, vocab_size,
                           total_edges, embed_dim, max_fanout))
        f.write(b'\x00' * 8)  # padding to 32 bytes
        
        # Node embeddings (random for demo)
        for i in range(vocab_size):
            vec = np.random.randn(128).astype(np.float32)
            f.write(vec.tobytes())
        
        # Node adjacency (CSR)
        edge_map = {}
        for src, dst, prior, weight in edges:
            edge_map.setdefault(src, []).append((dst, prior, weight))
        
        offset = 0
        for i in range(vocab_size):
            count = len(edge_map.get(i, []))
            f.write(struct.pack('II', offset, count))
            offset += count
        
        # Edge compact
        for i in range(vocab_size):
            for idx, (dst, prior, weight) in enumerate(edge_map.get(i, [])):
                weight_idx = sum(len(edge_map.get(j, [])) 
                               for j in range(i)) + idx
                f.write(struct.pack('IfI', dst, prior, weight_idx))
        
        # Padding to 32-byte
        pos = f.tell()
        aligned = (pos + 31) & ~31
        f.write(b'\x00' * (aligned - pos))
        
        # Edge weights
        for i in range(vocab_size):
            for dst, prior, weight in edge_map.get(i, []):
                f.write(np.array(weight, dtype=np.float32).tobytes())

# Example: create simple graph
edges = [
    (0, 1, 0.8, np.random.randn(128)),  # 0→1 high prior
    (0, 2, 0.2, np.random.randn(128)),  # 0→2 low prior
    (1, 3, 0.9, np.random.randn(128)),  # 1→3
    (2, 3, 0.1, np.random.randn(128)),  # 2→3
]

create_dag(vocab_size=4, edges=edges, output_path='my_graph.ehdag')
```

---

## 🔧 Customize Config

In your code:

```c
EH_HGN_EngineConfig config = eh_hgn_default_config();

// Disable collapse gating (full DAG every step)
config.enable_collapse = false;

// Disable mutant nodes
config.enable_mutants = false;

// Limit generation length
config.max_steps = 50;

// Adjust thresholds
config.collapse_thresh = 0.95f;  // Stricter collapse
config.entropy_thresh = 2.0f;    // Harder to trigger mutants

eh_hgn_session_init(&session, &dag, arena, prompt, len, &config);
```

---

## 📝 Project Integration

**Minimal example:**

```c
#include "hgn/eh_hgn_engine.h"

int main() {
    // 1. Setup
    EH_Arena *arena = eh_arena_create(512 * 1024 * 1024);
    EH_HGN_BaseDag dag;
    eh_hgn_dag_load(arena, "model.ehdag", &dag);
    
    // 2. Init session
    EH_HGN_InferenceSession session;
    uint32_t prompt[] = {42};
    eh_hgn_session_init(&session, &dag, arena, prompt, 1, NULL);
    
    // 3. Generate
    while (!eh_hgn_session_is_done(&session)) {
        eh_hgn_session_step(&session);
    }
    
    // 4. Get results
    const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
    printf("Generated: ");
    for (uint32_t i = 0; i < best->seq_len; i++) {
        printf("%u ", best->tokens[i]);
    }
    printf("\n");
    
    // 5. Cleanup
    eh_arena_destroy(arena);
    return 0;
}
```

---

## 🐛 Troubleshooting

**Issue: "Failed to load DAG"**
- Check magic number: `0x48474E44`
- Verify alignment: Node embeddings and weights must be 32-byte aligned
- Check file size matches header

**Issue: "Arena OOM"**
- Increase arena size: `eh_arena_create(1024 * 1024 * 1024)` (1GB)
- DAG size ≈ `vocab_size * 512 + total_edges * 524` bytes

**Issue: Generation doesn't terminate**
- Set `config.max_steps` to limit
- Check DAG has sink nodes (fanout=0)

---

## 📚 Next Steps

1. **Read full docs**: `HGN_MODULE.md`
2. **Study tests**: `tests/test_eh_hgn_engine.c`
3. **Customize**: Modify thresholds, beam width, scoring
4. **Integrate**: Add to existing pipeline

**Questions?** Check `HGN_MODULE.md` or source code comments!
