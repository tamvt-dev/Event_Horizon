# HGN Training Architecture Plan

## 🎯 Goal

Train HGN graph from data:
- Input: Text corpus / token sequences
- Output: `model.ehdag` binary file
- Method: Gradient-based learning + graph construction

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────┐
│  Training Pipeline (Python/C++)             │
├─────────────────────────────────────────────┤
│                                             │
│  1. Data → Token Sequences                  │
│     [corpus.txt] → [[t1,t2,t3], ...]        │
│                                             │
│  2. Graph Construction                      │
│     Token sequences → edges + weights       │
│     Co-occurrence → edge creation           │
│                                             │
│  3. Gradient Descent                        │
│     Loss = prediction error                 │
│     Update: edge weights, embeddings        │
│                                             │
│  4. Binary Export                           │
│     [eh_hgn_io.h] → model.ehdag             │
│                                             │
└─────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────┐
│  Inference (C only - existing)              │
├─────────────────────────────────────────────┤
│  eh_hgn_dag_load() → EH_HGN_BaseDag        │
│  eh_hgn_session_step() → inference         │
└─────────────────────────────────────────────┘
```

---

## 📦 New Modules

### Module 1: I/O Layer (`eh_hgn_io.h/.c`)

**Purpose**: Separate binary format from inference

```c
// Read EHDAG format → populate BaseDag
int eh_hgn_io_load(EH_Arena *arena, 
                   const char *path, 
                   EH_HGN_BaseDag *dag);

// Write BaseDag → EHDAG format
int eh_hgn_io_save(const EH_HGN_BaseDag *dag, 
                   const char *path);

// Validate EHDAG file
int eh_hgn_io_validate(const char *path);
```

**Benefits:**
- Decouple format from inference
- Easy to add new formats (JSON, protobuf, etc.)
- Existing `eh_hgn_dag_load()` → `eh_hgn_io_load()`

---

### Module 2: Graph Builder (`eh_hgn_builder.h/.c`)

**Purpose**: Construct graph from scratch (training mode)

```c
// Initialize empty graph
EH_HGN_Builder *eh_hgn_builder_create(uint32_t vocab_size,
                                      uint32_t max_fanout);

// Add edge: src → dst with weight vector
int eh_hgn_builder_add_edge(EH_HGN_Builder *builder,
                            uint32_t src,
                            uint32_t dst,
                            float prior,
                            const float *weight);

// Set node embedding
int eh_hgn_builder_set_embedding(EH_HGN_Builder *builder,
                                 uint32_t token_id,
                                 const float *embedding);

// Finalize → EH_HGN_BaseDag
int eh_hgn_builder_finalize(EH_HGN_Builder *builder,
                            EH_Arena *arena,
                            EH_HGN_BaseDag *dag);
```

**Usage (Training):**
```python
# Python side
builder = HGNBuilder(vocab_size=10000, max_fanout=32)

# Co-occurrence analysis
for (src, dst) in token_pairs:
    weight = compute_context_vector(src, dst)
    builder.add_edge(src, dst, prior=0.5, weight=weight)

# Export
dag = builder.finalize()
save_ehdag(dag, "model.ehdag")
```

---

### Module 3: Training (`eh_hgn_train.h/.c`) - OPTIONAL

**Purpose**: Gradient-based learning (if doing in C)

```c
// Mutable training graph
typedef struct {
    float **embeddings;       // [vocab_size][128]
    float **edge_weights;     // [num_edges][128]
    float *edge_priors;       // [num_edges]
    uint32_t vocab_size;
    uint32_t num_edges;
} EH_HGN_TrainGraph;

// Forward pass: compute loss
float eh_hgn_train_forward(EH_HGN_TrainGraph *graph,
                           const uint32_t *sequence,
                           uint32_t seq_len);

// Backward pass: compute gradients
void eh_hgn_train_backward(EH_HGN_TrainGraph *graph,
                           float lr);

// SGD step
void eh_hgn_train_step(EH_HGN_TrainGraph *graph,
                       const uint32_t *batch,
                       uint32_t batch_size,
                       float lr);
```

**Note**: Training có thể làm ở Python (easier), chỉ export result sang C.

---

## 🔄 Training Workflow

### Option A: Python Training (Recommended)

```
┌────────────────────────────────────────┐
│  Python Training                       │
├────────────────────────────────────────┤
│  1. Load corpus                        │
│  2. Tokenize → sequences               │
│  3. Build co-occurrence matrix         │
│  4. Initialize embeddings (random)     │
│  5. Gradient descent loop:             │
│     - Forward: predict next token      │
│     - Loss: cross-entropy              │
│     - Backward: update weights         │
│  6. Export graph → eh_hgn_builder      │
│  7. Save → model.ehdag                 │
└────────────────────────────────────────┘
         ↓
    model.ehdag
         ↓
┌────────────────────────────────────────┐
│  C Inference (existing)                │
│  eh_hgn_io_load() → inference          │
└────────────────────────────────────────┘
```

**Pros:**
- Python có numpy, PyTorch ecosystem
- Fast prototyping
- Easy debugging

**Cons:**
- Need Python dependency for training

---

### Option B: Pure C Training

```
┌────────────────────────────────────────┐
│  C Training                            │
├────────────────────────────────────────┤
│  1. Load corpus (text file)            │
│  2. Tokenize → sequences               │
│  3. EH_HGN_TrainGraph init             │
│  4. Training loop:                     │
│     - eh_hgn_train_forward()          │
│     - eh_hgn_train_backward()         │
│  5. Finalize → EH_HGN_BaseDag         │
│  6. eh_hgn_io_save() → model.ehdag    │
└────────────────────────────────────────┘
```

**Pros:**
- No Python dependency
- Consistent C codebase
- Embedded-friendly

**Cons:**
- Manual backprop implementation
- Slower development

---

## 📊 Data Structures

### Training Graph (Mutable)

```c
typedef struct {
    // Mutable arrays (malloc'd, can update)
    float **node_embeddings;    // [V][128]
    float **edge_weights;       // [E][128]
    float  *edge_priors;        // [E]
    
    // Structure
    uint32_t *edge_src;         // [E]
    uint32_t *edge_dst;         // [E]
    
    // Metadata
    uint32_t vocab_size;
    uint32_t num_edges;
    
    // Gradients (for backprop)
    float **emb_grads;          // [V][128]
    float **weight_grads;       // [E][128]
} EH_HGN_TrainGraph;
```

### Inference Graph (Immutable)

```c
// Existing structure (arena-backed, read-only)
typedef struct {
    const EH_HGN_NodeEmbed  *node_embed;
    const EH_HGN_NodeAdj    *node_adj;
    const EH_HGN_EdgeCompact *edge_compact;
    const EH_HGN_EdgeWeight *weight_pool;
    // ...
} EH_HGN_BaseDag;
```

**Conversion:**
```
TrainGraph (mutable) → Builder → BaseDag (immutable)
```

---

## 🎓 Training Algorithm

### Simple Co-occurrence

```python
# Build graph from token sequences
vocab = set()
edges = defaultdict(lambda: {"count": 0, "contexts": []})

for sequence in corpus:
    for i in range(len(sequence) - 1):
        src, dst = sequence[i], sequence[i+1]
        edges[(src, dst)]["count"] += 1
        context = get_context_vector(sequence, i)
        edges[(src, dst)]["contexts"].append(context)

# Convert to HGN
builder = HGNBuilder(vocab_size=len(vocab))
for (src, dst), data in edges.items():
    prior = data["count"] / total_transitions
    weight = np.mean(data["contexts"], axis=0)
    builder.add_edge(src, dst, prior, weight)
```

### Gradient-Based (Advanced)

```python
# Loss: next token prediction
def forward(graph, sequence):
    h = graph.embeddings[sequence[0]]
    loss = 0
    for i in range(len(sequence) - 1):
        # Score all edges from current token
        scores = []
        for edge in graph.edges_from(sequence[i]):
            score = edge.prior + dot(edge.weight, h)
            scores.append((edge.dst, score))
        
        # Cross-entropy loss
        target = sequence[i+1]
        loss += -log(softmax(scores)[target])
        
        # Update h
        h = graph.embeddings[sequence[i+1]]
    
    return loss

def backward(graph, sequence, lr):
    # Compute gradients (chain rule)
    # Update: embeddings, edge weights, priors
    pass
```

---

## 🔧 Implementation Priority

### Phase 1: I/O Refactor (Current PR)
- [ ] Create `eh_hgn_io.h/.c`
- [ ] Move `eh_hgn_dag_load()` → `eh_hgn_io_load()`
- [ ] Add `eh_hgn_io_save()`
- [ ] Update tests

### Phase 2: Builder (Next PR)
- [ ] Create `eh_hgn_builder.h/.c`
- [ ] API: create, add_edge, set_embedding, finalize
- [ ] Tests: build small graph, export to file
- [ ] Python bindings

### Phase 3: Training (Future)
- [ ] Option A: Python training script
- [ ] Option B: C training module
- [ ] Dataset: Wikipedia subset
- [ ] Benchmark: perplexity, inference speed

---

## 📝 File Structure (After Training Module)

```
include/hgn/
  ├── eh_hgn_dag.h          # Layer 1: DAG structure
  ├── eh_hgn_io.h           # NEW: Binary I/O
  ├── eh_hgn_builder.h      # NEW: Graph construction
  ├── eh_hgn_train.h        # NEW: Training (optional)
  ├── eh_hgn_collapse.h     # Layer 2: Collapse
  ├── eh_beam_search.h      # Layer 3: Beam
  └── eh_hgn_engine.h       # Layer 4: Engine

src/hgn/
  ├── eh_hgn_dag.c
  ├── eh_hgn_io.c           # NEW
  ├── eh_hgn_builder.c      # NEW
  ├── eh_hgn_train.c        # NEW (optional)
  ├── eh_hgn_collapse.c
  ├── eh_beam_search.c
  └── eh_hgn_engine.c

training/                    # NEW directory
  ├── train.py              # Python training script
  ├── export_ehdag.py       # Convert to binary
  └── datasets/
      └── wiki_sample.txt
```

---

## 🤔 Decisions Needed

1. **Training in Python or C?**
   - Python: Faster development, numpy/PyTorch
   - C: No dependencies, embedded-friendly

2. **I/O first or Builder first?**
   - I/O: Clean up existing code
   - Builder: Start training experiments

3. **Dataset?**
   - Small: 1MB text file (fast iteration)
   - Large: Wikipedia dump (production quality)

4. **Training objective?**
   - Simple: Co-occurrence counts
   - Advanced: Cross-entropy + backprop

---

## 🚀 Next Steps

**What do you want to do first?**

1. **Refactor I/O** (`eh_hgn_io.h`) - Clean separation
2. **Create Builder** (`eh_hgn_builder.h`) - Start training
3. **Python training script** - Quick experiments

Bạn muốn bắt đầu từ đâu? 🤔
