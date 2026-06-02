# EventHorizon Engine - GitHub Wiki Setup Guide

This document outlines the recommended structure for the EventHorizon Engine GitHub Wiki.

---

## 📚 Wiki Structure

### Home Page
**URL**: `/wiki/Home`

```markdown
# Welcome to EventHorizon Engine Wiki

    ◉
   ╱ ╲
  ╱   ╲    Sub-millisecond Edge AI Inference
 ◉─────◉   Zero Allocation • Adaptive • Production Ready
  ╲   ╱
   ╲ ╱
    ◉

---

## Quick Navigation

### 🚀 Getting Started
- [Installation Guide](Installation-Guide)
- [Quick Start Tutorial](Quick-Start-Tutorial)
- [First Application](First-Application)
- [Hello World Example](Hello-World-Example)

### 📖 Core Concepts
- [Architecture Overview](Architecture-Overview)
- [Memory Arena System](Memory-Arena-System)
- [DAG and State Collapse](DAG-and-State-Collapse)
- [Beam Search Algorithm](Beam-Search-Algorithm)
- [Neuroplasticity](Neuroplasticity)

### 🔧 Development
- [API Reference](API-Reference)
- [Performance Tuning](Performance-Tuning)
- [Debugging Guide](Debugging-Guide)
- [Testing Guide](Testing-Guide)

### 🎯 Use Cases
- [Game AI Development](Game-AI-Development)
- [Edge AI Deployment](Edge-AI-Deployment)
- [Network Routing](Network-Routing)
- [Robotics Applications](Robotics-Applications)

### 🤝 Contributing
- [Contributing Guidelines](Contributing-Guidelines)
- [Code Style](Code-Style)
- [Building from Source](Building-from-Source)
- [Roadmap](Roadmap)

---

## Project Stats

- **Performance**: 278,133 inferences/sec
- **Memory**: 63 MB baseline
- **Latency**: <1ms sub-millisecond
- **License**: Apache-2.0
- **Language**: Pure C99

---

## External Resources

- [GitHub Repository](https://github.com/[username]/eventhorizon)
- [Issue Tracker](https://github.com/[username]/eventhorizon/issues)
- [Discussions](https://github.com/[username]/eventhorizon/discussions)
```

---

## 📄 Wiki Pages (Detailed Content)

### 1. Installation Guide
**URL**: `/wiki/Installation-Guide`

**Content Outline:**
```markdown
# Installation Guide

## Prerequisites
- GCC 7+ or Clang 10+
- Make
- Git

## Platform-Specific Installation

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential git
git clone https://github.com/[username]/eventhorizon.git
cd eventhorizon
make build-all
```

### macOS
```bash
brew install gcc make git
git clone ...
make build-all
```

### Windows (WSL)
```powershell
wsl --install
# Inside WSL:
git clone ...
make build-all
```

### Docker
```bash
docker build -t eventhorizon:latest .
docker run eventhorizon:latest
```

## Verification
```bash
./eh_engine_ultimate_bench
# Expected: 278K+ inferences/sec
```

## Troubleshooting
- Issue: "gcc not found"
  Solution: Install build-essential
...
```

---

### 2. Quick Start Tutorial
**URL**: `/wiki/Quick-Start-Tutorial`

**Content Outline:**
```markdown
# Quick Start Tutorial

## 30-Second Example

Step 1: Create arena
Step 2: Build graph
Step 3: Run inference
Step 4: Cleanup

## Complete Code Example
[Link to hello_world.c]

## What's Happening?
- Zero allocation explanation
- State collapse visualization
- Performance metrics

## Next Steps
- Try arena_demo.c
- Try graph_demo.c
- Read Performance Tuning Guide
```

---

### 3. Architecture Overview
**URL**: `/wiki/Architecture-Overview`

**Content Outline:**
```markdown
# Architecture Overview

## System Design

```
┌─────────────────────────────────────┐
│         Application Layer           │
├─────────────────────────────────────┤
│    EH_Context (Engine Orchestrator) │
├─────────────┬───────────────────────┤
│ Beam Search │  Neuroplasticity      │
├─────────────┼───────────────────────┤
│  DAG Graph  │  Scoring Core         │
├─────────────┴───────────────────────┤
│       Memory Arena (Flat Buffer)    │
└─────────────────────────────────────┘
```

## Core Components

### Memory Arena
- Zero-allocation design
- O(1) pointer-bump allocation
- Lifetime: entire session

### DAG (Directed Acyclic Graph)
- Nodes: computation units
- Edges: routing paths
- State: collapsed or dense

### Beam Search
- Width: configurable
- Pruning: automatic
- Scoring: real-time

### Neuroplasticity
- Mutation: error-driven
- Learning: parametric + structural
- Adaptation: runtime

## Data Flow
[Diagram showing input → scoring → beam search → output]

## Performance Characteristics
- Time Complexity: O(N) with collapse, O(N²) without
- Space Complexity: O(N) nodes + O(E) edges
- Cache Efficiency: <0.01% misses
```

---

### 4. Memory Arena System
**URL**: `/wiki/Memory-Arena-System`

**Content:**
```markdown
# Memory Arena System

## Concept

Traditional allocation:
```
malloc() → OS → fragmentation → GC pauses
```

Arena allocation:
```
arena_alloc() → bump pointer → no fragmentation → no GC
```

## API

### Create Arena
```c
EH_Arena *arena = eh_arena_create(size);
```

### Allocate
```c
void *ptr = eh_arena_alloc(arena, size);
```

### Reset (O(1))
```c
eh_arena_reset(arena);  // Instant, no free() calls
```

### Destroy
```c
eh_arena_destroy(arena);
```

## Performance

- **Allocation Speed**: 24.84 Million allocs/sec
- **Reset Time**: O(1) regardless of allocation count
- **Fragmentation**: Zero (linear memory)

## Best Practices

1. Size arena appropriately
2. Reset between sessions
3. Never free individual allocations
4. Destroy only at shutdown

## Example: Game Loop
```c
EH_Arena *arena = eh_arena_create(32 * 1024 * 1024);

while (game_running) {
    // Frame start
    
    // Use arena for frame allocations
    void *temp = eh_arena_alloc(arena, size);
    
    // Frame end
    eh_arena_reset(arena);  // O(1) cleanup!
}

eh_arena_destroy(arena);
```
```

---

### 5. Performance Tuning
**URL**: `/wiki/Performance-Tuning`

**Content:**
```markdown
# Performance Tuning

See [PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md) for comprehensive guide.

## Quick Wins

1. Enable -march=native
2. Tune collapse threshold
3. Optimize arena size
4. CPU performance mode
5. Disable THP

## Platform-Specific

### x86_64
- Use AVX2/AVX-512
- Enable FMA
- Compiler: GCC 12+ or Clang 15+

### ARM
- Use NEON
- Reduce memory footprint
- Compiler: GCC 11+ with ARM optimizations

### RISC-V
- Minimize memory usage
- Aggressive collapse
- Test on real hardware

## Benchmarking

```bash
make bench
./eh_engine_ultimate_bench
```

## Profiling

```bash
perf record -g ./your_app
perf report
```
```

---

### 6. Game AI Development
**URL**: `/wiki/Game-AI-Development`

**Content:**
```markdown
# Game AI Development with EventHorizon

## Use Case: NPC Behavior

### Problem
- 1000 NPCs need decisions at 60 FPS
- Budget: 16.6ms / 1000 = 16.6μs per NPC
- Traditional AI: too slow, GC pauses

### Solution
- EventHorizon: 4.8μs per NPC
- Can handle 3,458 NPCs @ 60 FPS
- Zero GC pauses

## Example: PATROL → CHASE → ATTACK

See [examples/game_ai_npc.c]

### Setup
```c
// Create behavior graph
EH_DAGNode *patrol = ...;
EH_DAGNode *chase = ...;
EH_DAGNode *attack = ...;

eh_dag_connect_nodes(patrol, chase);
eh_dag_connect_nodes(chase, attack);
```

### Runtime
```c
// Every frame (60 FPS)
SensorData sensors = get_npc_sensors(npc);
sensors_to_vector(&sensors, input, 32);

eh_engine_inference(ctx, input, 32, output, 32);

NPCBehavior behavior = vector_to_behavior(output, 32);
execute_behavior(npc, behavior);
```

### Adaptive Learning
```c
// Learn from mistakes
float target[32] = get_correct_behavior(situation);
eh_neuro_process_feedback(nctx, parent, output, target, 32, ...);
```

## Performance Tips

1. Use aggressive collapse threshold (1.0-1.2)
2. Small arena (16-32 MB)
3. Batch NPCs when possible
4. Prune mutants periodically

## Integration

### Unity (C#)
```csharp
[DllImport("eventhorizon")]
extern static IntPtr eh_engine_setup(...);
```

### Unreal (C++)
```cpp
extern "C" {
    #include "eh_engine.h"
}
```
```

---

### 7. API Reference
**URL**: `/wiki/API-Reference`

**Content:**
```markdown
# API Reference

## Header Files

- `eh_arena.h` - Memory arena
- `eh_dag.h` - Graph structure
- `eh_engine.h` - Inference engine
- `eh_scoring.h` - Routing/scoring
- `eh_neuro.h` - Neuroplasticity
- `eh_dynamic.h` - Dynamic collapse

## Core APIs

### Memory Arena

```c
EH_Arena *eh_arena_create(size_t capacity);
void *eh_arena_alloc(EH_Arena *arena, size_t size);
void eh_arena_reset(EH_Arena *arena);
void eh_arena_destroy(EH_Arena *arena);
```

### DAG

```c
EH_DAGNode *eh_dag_create_node(int node_id, int rows, int cols);
void eh_dag_connect_nodes(EH_DAGNode *parent, EH_DAGNode *child);
float eh_dag_compute_frobenius_norm(const EH_Matrix *m);
```

### Engine

```c
EH_Context *eh_engine_setup(EH_DAGNode *root, EH_ScoringCore *core, float threshold);
EH_Context *eh_engine_setup_dynamic(EH_DAGNode *root, EH_ScoringCore *core, float threshold, float low, float high);
bool eh_engine_inference(const EH_Context *ctx, const float *input, int input_dim, float *output, int output_dim);
void eh_engine_shutdown(EH_Context *ctx);
```

### Neuroplasticity

```c
EH_NeuroContext *eh_neuro_init(EH_Arena *arena, float error_threshold, float learning_rate, float mutation_rate);
EH_FeedbackResult eh_neuro_process_feedback(EH_NeuroContext *nctx, EH_DAGNode *parent, const float *output, const float *target, int dim, ...);
void eh_neuro_free(EH_NeuroContext *nctx);
```

## Data Structures

[Full struct definitions with field descriptions]

## Constants

[All #define constants with explanations]
```

---

## 🔗 Wiki Navigation Template

**Add to every page:**

```markdown
---

## Wiki Navigation

**◀ Previous**: [Previous Page Title](Previous-Page)  
**▲ Home**: [Wiki Home](Home)  
**▶ Next**: [Next Page Title](Next-Page)

---

**Related Pages:**
- [Related Topic 1](Topic-1)
- [Related Topic 2](Topic-2)

**External Resources:**
- https://github.com/tamvt-dev/Event_Horizon
- [Performance Tuning Guide](../PERFORMANCE_TUNING.md)
```

---

## 📝 Wiki Maintenance

### Update Frequency
- **Core Pages**: Update with every major release
- **API Reference**: Update with every API change
- **Performance Guides**: Update when benchmarks change
- **Examples**: Add with new use cases

### Style Guide
- Use code blocks for examples
- Include visual diagrams (ASCII art)
- Link to source code liberally
- Keep pages focused (single topic)
- Use consistent formatting

### Contributors
- Anyone can suggest edits via Issues
- Maintainers approve wiki changes
- Document all major edits in changelog

---

## 🚀 Getting Started with Wiki

### For Repository Owners

1. **Enable Wiki**
   - Go to Settings → Features
   - Check "Wikis"

2. **Create Home Page**
   - Copy content from this guide
   - Add project-specific info

3. **Create Core Pages**
   - Start with: Home, Installation, Quick Start, API Reference
   - Add use cases as examples grow

4. **Link from README**
   ```markdown
   **📚 [Documentation Wiki](https://github.com/[user]/eventhorizon/wiki)**
   ```

### For Contributors

1. **Suggest Content**
   - Open Issue with "Wiki:" prefix
   - Describe proposed page/changes

2. **Submit via PR**
   - Clone wiki: `git clone https://github.com/[user]/eventhorizon.wiki.git`
   - Edit markdown files
   - Push changes

3. **Discuss in Discussions**
   - Use Discussions tab for major wiki changes
   - Get community feedback

---

## 📊 Wiki Metrics (Track these)

- **Page Views**: Which pages are most visited?
- **Search Terms**: What are users looking for?
- **Missing Links**: Where do users click 404s?
- **External Traffic**: Where do users come from?

Use GitHub Insights to optimize wiki structure.

---

## 🎨 Wiki Customization

### Sidebar (_Sidebar.md)
```markdown
## Navigation

**Getting Started**
- [Installation](Installation-Guide)
- [Quick Start](Quick-Start-Tutorial)
- [Examples](Examples)

**Core Concepts**
- [Architecture](Architecture-Overview)
- [Memory Arena](Memory-Arena-System)
- [DAG Graphs](DAG-and-State-Collapse)

**Advanced**
- [Performance](Performance-Tuning)
- [API Reference](API-Reference)
- [Contributing](Contributing-Guidelines)
```

### Footer (_Footer.md)
```markdown
---
EventHorizon Engine | Apache-2.0 License | [GitHub]
https://github.com/tamvt-dev/Event_Horizon
```

---

**Version:** 1.0  
**Last Updated:** June 2, 2026  
**Maintainer:** EventHorizon Engine Contributors
