# HGN Module - Heuristic Graph Network

## 📦 Module Overview

**HGN (Heuristic Graph Network)** là module mới độc lập, chuyên xử lý **large-scale sparse graph inference** với:
- **Zero-copy loading** từ binary format
- **CSR (Compressed Sparse Row)** representation
- **AVX2-optimized** scoring và embedding
- **Arena-based memory** management (zero malloc)

---

## 🗂️ Module Structure

```
include/hgn/
  ├── eh_hgn_dag.h         # Layer 1: Base DAG với CSR format
  ├── eh_hgn_collapse.h    # Layer 2: Dynamic Collapse Gating
  ├── eh_beam_search.h     # Layer 3: Autoregressive Beam Search
  ├── eh_hgn_engine.h      # Layer 4: Unified Inference Engine (Wiring)
  └── eh_adaptive.h        # (deprecated - use eh_hgn_collapse.h)

src/hgn/
  ├── eh_hgn_dag.c         # Layer 1 implementation
  ├── eh_hgn_collapse.c    # Layer 2 implementation
  ├── eh_beam_search.c     # Layer 3 implementation
  ├── eh_hgn_engine.c      # Layer 4 implementation (orchestration)
  └── eh_adaptive.c        # (deprecated)

tests/
  ├── test_eh_hgn_dag.c      # Layer 1: 33 tests ✅
  ├── test_eh_hgn_collapse.c # Layer 2: 37 tests ✅
  ├── test_eh_hgn_beam.c     # Layer 3: 6 tests ✅
  ├── test_eh_hgn_engine.c   # Layer 4: 41 tests ✅
  └── test_eh_adaptive.c     # (deprecated - 29 tests)
```

---

## 🎯 Design Goals

### 1. **Memory Efficiency**
- **Split arrays**: Node embeddings, adjacency, edges, weights riêng biệt
- **Alignment**: 32-byte aligned cho AVX2 (`_mm256_load_ps`)
- **CSR format**: Chỉ lưu edges tồn tại (sparse graph friendly)

### 2. **Cache Optimization**
- **Two-phase scan**:
  - Phase 1: Scan `edge_compact` (12B/edge) → filter by prior
  - Phase 2: Load `weight_pool` (512B/edge) chỉ cho top candidates
- **Sequential access**: CSR layout → prefetch-friendly

### 3. **Zero-Copy Loading**
- `fread()` trực tiếp vào arena
- Không có `malloc()` / `memcpy()` overhead
- Toàn bộ graph trong 1 contiguous memory block

---

## 📊 Data Structures

### File Format: `ehdag.bin`

```
[EH_HGN_DagFileHeader    — 32 bytes]
[EH_HGN_NodeEmbed × V    — align-32]  ← Node embeddings (128D vectors)
[EH_HGN_NodeAdj × V                ]  ← CSR adjacency list
[EH_HGN_EdgeCompact × E             ]  ← Edge metadata (dst + prior + weight_idx)
[padding to 32-byte boundary        ]
[EH_HGN_EdgeWeight × E   — align-32]  ← Edge weights (128D vectors)
```

### Memory Layout

| Component | Size | Alignment | Purpose |
|-----------|------|-----------|---------|
| `NodeEmbed` | V × 512B | 32-byte | Embedding vectors (AVX2) |
| `NodeAdj` | V × 8B | 8-byte | CSR offsets + counts |
| `EdgeCompact` | E × 12B | 4-byte | Lightweight edge scan |
| `EdgeWeight` | E × 512B | 32-byte | Full weights (loaded on-demand) |

**Example**: V=32768, E=1M (fanout=32)
- NodeEmbed: 16 MB
- NodeAdj: 256 KB
- EdgeCompact: 12 MB
- EdgeWeight: 512 MB
- **Total**: ~540 MB

---

## 🔧 API

### Layer 1: Load DAG from File

```c
#include "hgn/eh_hgn_dag.h"
#include "core/eh_arena.h"

EH_Arena *arena = eh_arena_create(1024 * 1024 * 1024);  // 1GB arena
EH_HGN_BaseDag dag;

EH_HGN_Status rc = eh_hgn_dag_load(arena, "graph.ehdag", &dag);
if (rc != EH_HGN_OK) {
    fprintf(stderr, "Load failed: %d\n", rc);
    return -1;
}
```

### Layer 2: Dynamic Collapse Gating

**Collapse Gating** quyết định giữa hai inference modes:
- **COLLAPSE**: Mean pooling O(N) khi context tuyến tính (`cos(h_t, h_{t-1}) > 0.92`)
- **EXPAND**: Full DAG traversal khi context thay đổi (`cos(h_t, h_{t-1}) ≤ 0.92`)

**Mutant Nodes** xử lý OOD (Out-Of-Distribution) signals:
- **Trigger**: `beam_entropy > 1.80` (high uncertainty)
- **Perturbation**: LCG-based deterministic noise (`seed = score XOR step`)
- **Lifetime**: 1 step, auto-deactivate sau mỗi context shift

```c
#include "hgn/eh_hgn_collapse.h"

// 1. Initialize collapse context
EH_HGN_CollapseCtx ctx;
eh_hgn_collapse_ctx_init(&ctx, arena);

// 2. Gating decision (mỗi autoregressive step)
float h_t[EH_HGN_EMBED_DIM];  // Current hidden state
EH_HGN_CollapseDecision decision = eh_hgn_collapse_gate(&ctx, h_t);

if (decision == EH_HGN_COLLAPSE) {
    // Fast path: mean pooling O(N)
    float pooled[EH_HGN_EMBED_DIM];
    eh_hgn_collapse_mean_pool(&dag, token_id, pooled);
    // Use pooled as next context
} else {
    // Full path: DAG traversal with AVX2 scoring
    // ... (normal beam search)
}

// 3. Check OOD signal from beam scores
float beam_scores[4] = {...};  // Top-K beam scores
float entropy = eh_hgn_beam_entropy(beam_scores, 4);

if (entropy > ctx.entropy_thresh) {
    // Spawn temporary mutant node
    EH_HGN_MutantNode *mutant = eh_hgn_mutant_spawn(
        &ctx, src_token, dst_token, beam_scores[0]);
    
    if (mutant) {
        // Use mutant->vec as alternative bridge path
        // Lifetime: 1 step only
    }
}

// 4. End of step: deactivate all mutants
eh_hgn_collapse_step_end(&ctx);

// 5. Dump stats
eh_hgn_collapse_dump_stats(&ctx);
```

**Key Functions**:
- `eh_hgn_collapse_gate()`: Cosine similarity-based gating
- `eh_hgn_collapse_mean_pool()`: O(N) alternative to full DAG
- `eh_hgn_beam_entropy()`: OOD detection via entropy
- `eh_hgn_mutant_spawn()`: Deterministic perturbation for exploration
- `eh_hgn_collapse_step_end()`: Reset mutants after 1 step

### Layer 4: Unified Inference Engine (WIRING LAYER)

**`eh_hgn_engine.h`** orchestrates tất cả layers thành một unified API:
- **Session-based**: Stateful inference với prompt → step() → results
- **Zero malloc per step**: Tất cả memory từ arena
- **Configurable**: Enable/disable collapse gating, mutants, max steps
- **Full beam access**: Trả về toàn bộ K=4 beams, caller tự chọn

```c
#include "hgn/eh_hgn_engine.h"

// 1. Create session với prompt
EH_HGN_InferenceSession session;
uint32_t prompt[] = {42, 17};  // Start tokens

int rc = eh_hgn_session_init(
    &session, &dag, arena, prompt, 2, NULL  /* NULL = default config */);

// 2. Generation loop
while (!eh_hgn_session_is_done(&session)) {
    uint32_t active = eh_hgn_session_step(&session);
    if (active == 0) break;  // All beams finished
}

// 3. Get results (toàn bộ beams)
const EH_HGN_BeamPath *beams = eh_hgn_session_get_beams(&session);
for (uint32_t i = 0; i < EH_BEAM_WIDTH; i++) {
    if (beams[i].seq_len > 0) {
        printf("Beam %u: score=%.3f, tokens=[", i, beams[i].score);
        for (uint32_t j = 0; j < beams[i].seq_len; j++) {
            printf("%u ", beams[i].tokens[j]);
        }
        printf("]\n");
    }
}

// 4. Or get best beam only
const EH_HGN_BeamPath *best = eh_hgn_session_get_best(&session);
printf("Best: score=%.3f\n", best->score);

// 5. Reset for new prompt
uint32_t new_prompt[] = {99};
eh_hgn_session_reset(&session, new_prompt, 1);
```

**Key Functions**:
- `eh_hgn_session_init()`: Initialize với DAG + prompt + config
- `eh_hgn_session_step()`: Thực thi 1 autoregressive step
- `eh_hgn_session_get_beams()`: Lấy tất cả K beams (caller chọn)
- `eh_hgn_session_get_best()`: Shortcut lấy top-1 beam
- `eh_hgn_session_reset()`: Reset state cho prompt mới
- `eh_hgn_session_dump_stats()`: Debug stats (collapse counts, mutants, etc.)

**Pipeline trong mỗi step**:
1. Lấy last token từ top beam → node embedding = h_current
2. **Collapse gate**: cos(h_current, h_prev) → COLLAPSE/EXPAND
3. **Beam expansion**: AVX2 scoring trên DAG edges
4. **Entropy check**: beam_entropy > threshold → spawn mutants
5. **Top-K selection**: Sort và giữ lại K=4 beams tốt nhất
6. **Cleanup**: Deactivate mutants (lifetime = 1 step)

### Query Node Embedding

```c
const float *embedding = eh_hgn_dag_node_vec(&dag, token_id);
if (embedding) {
    // embedding[0..127] là 128D vector
    float score = dot_product(embedding, query_vec, 128);
}
```

### Iterate Edges

```c
EH_HGN_FOR_EDGES(&dag, node_id, edge) {
    uint32_t dst = edge->dst;
    float prior = edge->prior;
    
    // Load weight chỉ khi cần (hot path)
    const float *weight = eh_hgn_dag_edge_weight(&dag, edge);
    float score = dot_product(weight, context_vec, 128);
}
```

### Inline Accessors (Zero Overhead)

```c
// Fanout của node
uint32_t fanout = eh_hgn_dag_fanout(&dag, node_id);

// Edge iterators
const EH_HGN_EdgeCompact *begin = eh_hgn_dag_edges_begin(&dag, node_id);
const EH_HGN_EdgeCompact *end   = eh_hgn_dag_edges_end(&dag, node_id);

for (const EH_HGN_EdgeCompact *e = begin; e != end; e++) {
    // Process edge
}
```

---

## ✅ Test Coverage

### Layer 1: Base DAG (test_eh_hgn_dag.c)
**Test Groups**:
- **T0**: Binary file generation + arena init
- **T1**: Load from file
- **T2**: Header field validation
- **T3**: Edge iteration
- **T4**: Node vector accessors
- **T5**: Edge weight accessors
- **T6**: `EH_HGN_FOR_EDGES` macro
- **T7**: Full graph iteration
- **T8**: Fanout helpers
- **T9**: Out-of-bounds handling
- **T10**: Info dumping

**Status**: ✅ **33/33 tests passing (100%)**

### Layer 2: Collapse Gating (test_eh_hgn_collapse.c)
**Test Groups**:
- **T0**: DAG binary generation + arena setup
- **T1**: CollapseCtx initialization
- **T2**: First step always EXPAND (no h_prev)
- **T3**: Identical vectors → COLLAPSE
- **T4**: Orthogonal vectors → EXPAND
- **T5**: Mean pooling O(N) computation
- **T6**: Beam entropy calculation
- **T7**: Mutant node spawning (LCG-based perturbation)
- **T8**: Step-end mutant deactivation
- **T9**: Context reset preserves thresholds
- **T10**: Stats dump no crash

**Status**: ✅ **37/37 tests passing (100%)**

### Layer 3: Beam Search (test_eh_hgn_beam.c)
**Test Groups**:
- **T1**: Arena initialization
- **T2**: CSR binary loaded
- **T3**: Beam tracker initialized with prompt
- **T4**: Step 1 autoregressive expansion (prior + AVX2 scoring)
- **T5**: Sequence convergence to terminal token
- **T6**: Multi-beam parent tracking
- **T7**: Beam step returns 0 when all finished

**Status**: ✅ **6/6 tests passing (100%)**

### Layer 4: Unified Engine (test_eh_hgn_engine.c)
**Test Groups**:
- **T0**: DAG setup and arena creation
- **T1**: Default config validation
- **T2**: Session initialization with prompt
- **T3**: Initial beam state after init
- **T4**: First inference step (beam expansion)
- **T5**: Second step (convergence)
- **T6**: Terminal step (all beams finished)
- **T7**: Session reset with new prompt
- **T8**: Custom config (disable collapse/mutants)
- **T9**: Max steps enforcement
- **T10**: Stats and beam dumping
- **T11**: Null safety checks

**Status**: ✅ **41/41 tests passing (100%)**

**Total HGN Tests**: ✅ **117/117 passing (100%)**

---

## 🚀 Build & Test

### Build HGN Module

```bash
make bench        # Builds with HGN support
```

### Run HGN Tests

```bash
cd tests
make test_eh_hgn_dag
./test_eh_hgn_dag
```

**Expected output**:
```
=== EH_HGN_BaseDag Tests ===
...
=== 33/33 passed ===
```

### Run All Tests

```bash
cd tests
make test
```

Tests: `test_arena`, `test_dag`, `test_engine`, `test_eh_hgn_dag`, `test_eh_hgn_beam`, `test_eh_hgn_collapse`, `test_eh_hgn_engine` (134 total tests)

---

## 🔬 Technical Details

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `EH_HGN_VOCAB_SIZE` | 32768 | Max tokens (2^15) |
| `EH_HGN_EMBED_DIM` | 128 | Embedding dimension |
| `EH_HGN_MAX_FANOUT` | 32 | Max edges per node |
| `EH_HGN_MAX_EDGES` | 1M | Max total edges |
| `EH_SCORE_DIM` | 128 | Scoring dimension (must match EMBED_DIM) |

### Compile-time Checks

```c
_Static_assert(EH_HGN_EMBED_DIM == EH_SCORE_DIM,
               "EMBED_DIM must equal SCORE_DIM");
               
_Static_assert(sizeof(EH_HGN_DagFileHeader) == 32,
               "Header must be 32 bytes");
```

### Error Codes

```c
EH_HGN_OK           = 0   // Success
EH_HGN_ERR_IO       = 1   // File I/O error
EH_HGN_ERR_MAGIC    = 2   // Bad magic number
EH_HGN_ERR_VERSION  = 3   // Version mismatch
EH_HGN_ERR_OVERFLOW = 4   // Exceeds compile-time limits
EH_HGN_ERR_ARENA    = 5   // Arena out of memory
EH_HGN_ERR_CORRUPT  = 6   // Data corruption detected
```

---

## 📈 Performance Characteristics

### Load Performance
- **I/O bound**: Limited by disk speed
- **Zero-copy**: No malloc/memcpy overhead
- **Validation**: O(V+E) CSR integrity check after load

### Query Performance
- **node_vec()**: O(1) array lookup
- **edges_begin/end()**: O(1) pointer arithmetic
- **EH_HGN_FOR_EDGES**: Linear scan, prefetch-friendly
- **edge_weight()**: O(1) indexed load (cache-sensitive)

### Memory Overhead
- **Struct overhead**: 16 bytes (`EH_HGN_BaseDag`)
- **Alignment padding**: Minimal (pre-calculated in file)
- **No fragmentation**: Single arena allocation

---

## 🔗 Dependencies

### From Core Module
- `eh_arena.h` - Memory arena management
- `eh_scoring.h` - `EH_SCORE_DIM` constant

### External
- `<stdint.h>` - Fixed-width integers
- `<stdio.h>` - File I/O
- `<string.h>` - `memset`
- `<errno.h>` - Error reporting

---

## 🎓 Design Philosophy

### **Separation of Concerns**
- **Core module**: Generic utilities (arena, scoring)
- **HGN module**: Domain-specific graph structures

### **Zero-Cost Abstractions**
- Inline accessors → no function call overhead
- Macros for iteration → compiler optimizes to tight loops
- CSR format → cache-friendly sequential access

### **Data-Oriented Design**
- Split arrays → better cache utilization
- Alignment-aware → AVX2 vectorization ready
- Arena-based → predictable memory layout

---

## 🛠️ Future Extensions

### Planned Features (v1.1+)
- **Multi-level CSR**: Hierarchical sparse matrices
- **Dynamic edge pruning**: Runtime graph simplification
- **Quantized weights**: INT8/FP16 for memory reduction
- **CUDA support**: GPU-accelerated scoring
- **Compressed embeddings**: PQ (Product Quantization)

### Integration Points
- **Beam Search**: HGN DAG as base for inference engine
- **Learning**: Gradient-based edge weight updates
- **Distillation**: Teacher model → HGN compression

---

## 📝 Usage Example

```c
#include "core/eh_arena.h"
#include "core/eh_scoring.h"
#include "hgn/eh_hgn_dag.h"

int main(void) {
    // 1. Setup arena
    EH_Arena *arena = eh_arena_create(512 * 1024 * 1024);  // 512MB
    
    // 2. Load graph
    EH_HGN_BaseDag dag;
    if (eh_hgn_dag_load(arena, "model.ehdag", &dag) != EH_HGN_OK) {
        return 1;
    }
    
    // 3. Dump stats
    eh_hgn_dag_dump_info(&dag);
    
    // 4. Query inference
    uint32_t current_token = 42;
    const float *emb = eh_hgn_dag_node_vec(&dag, current_token);
    
    // 5. Iterate edges
    EH_HGN_FOR_EDGES(&dag, current_token, edge) {
        printf("→ token %u (prior=%.3f)\n", edge->dst, edge->prior);
    }
    
    // 6. Cleanup
    eh_arena_destroy(arena);
    return 0;
}
```

---

## 📚 References

- **CSR Format**: [Wikipedia - Sparse Matrix](https://en.wikipedia.org/wiki/Sparse_matrix#Compressed_sparse_row_(CSR,_CRS_or_Yale_format))
- **AVX2**: [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- **Arena Allocators**: [Ginger Bill - Memory Allocation Strategies](https://www.gingerbill.org/series/memory-allocation-strategies/)

---

**Status**: ✅ Production-ready (v1.0.0)  
**Test Coverage**: 33/33 tests passing  
**Build**: gcc -O3 -std=c99 -mavx2  
**License**: Apache-2.0
