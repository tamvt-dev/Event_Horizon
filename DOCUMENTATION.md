# EventHorizon Engine - Documentation Index

Welcome to the EventHorizon Engine documentation! This guide will help you navigate all available resources.

---

## 📚 Documentation Structure

### 🚀 Getting Started
- **[QUICKSTART.md](QUICKSTART.md)** - Get up and running in 30 seconds
- **[README.md](README.md)** - Comprehensive project overview and technical specifications

### 📖 Detailed Information
- **[CHANGELOG.md](CHANGELOG.md)** - Recent updates and performance improvements
- **[CREDITS.md](CREDITS.md)** - AI-assisted development acknowledgments and philosophy

### 💻 Code Organization
- **include/** - Header files with API definitions
- **src/core/** - Core implementation files
- **bench_all.c** - Comprehensive benchmark suite

---

## 🎯 Quick Navigation

### New Users Start Here:
1. Read [QUICKSTART.md](QUICKSTART.md) for immediate usage
2. Run `.\run.ps1` or `make run-bench` to see it in action
3. Check [README.md](README.md) for detailed concepts

### Developers Start Here:
1. Read [README.md](README.md) for architecture overview
2. Browse header files in `include/` for API reference
3. Study `bench_all.c` for usage examples
4. Check [TODO.md](TODO.md) for contribution opportunities

### AI/ML Researchers Start Here:
1. Read "Core Capabilities" section in [README.md](README.md)
2. Study the State Collapse mechanism
3. Review Neuroplasticity implementation in `include/eh_neuro.h`
4. See benchmark results for performance characteristics

### Edge AI Engineers Start Here:
1. Read "Edge AI Deployment" section in [README.md](README.md)
2. Review platform-specific optimization tips
3. Check memory footprint and energy consumption data
4. Study arena allocator implementation for zero-allocation patterns

---

## 📋 Documentation by Topic

### Architecture & Design
- **Memory Arena**: [README.md](README.md) → "Core Components" → "Memory Arena"
- **DAG Structure**: `include/eh_dag.h` + [README.md](README.md)
- **Beam Search**: `include/eh_engine.h` + implementation details
- **State Collapse**: [README.md](README.md) → "Technical Specifications"

### Performance & Benchmarks
- **Benchmark Results**: [README.md](README.md) → "Real-World Execution Results"
- **Scalability**: [README.md](README.md) → "Physical Scalability Matrix"
- **Updates**: [CHANGELOG.md](CHANGELOG.md) → Performance improvements

### API Reference
- **Engine**: `include/eh_engine.h` - Main inference API
- **DAG**: `include/eh_dag.h` - Graph structure
- **Arena**: `include/eh_arena.h` - Memory management
- **Neuro**: `include/eh_neuro.h` - Neuroplasticity
- **Scoring**: `include/eh_scoring.h` - Routing core
- **Tokenizer**: `include/eh_tokenizer.h` - Text processing

### Platform-Specific
- **Edge Devices**: [README.md](README.md) → "Edge AI Deployment"
- **Raspberry Pi**: Platform-specific compilation tips
- **Mobile**: Memory optimization strategies
- **Microcontrollers**: Minimal footprint configuration

### Development
- **Building**: [QUICKSTART.md](QUICKSTART.md) → "Build Commands"
- **Testing**: [QUICKSTART.md](QUICKSTART.md) → "Common Commands"
- **Contributing**: [TODO.md](TODO.md) → Planned features
- **AI Development**: [CREDITS.md](CREDITS.md) → Methodology

---

## 🔍 Find What You Need

### "How do I..."

**...run the benchmark?**
→ [QUICKSTART.md](QUICKSTART.md) - Commands section

**...compile manually?**
→ [QUICKSTART.md](QUICKSTART.md) - Manual Compilation section

**...use the API?**
→ [QUICKSTART.md](QUICKSTART.md) - Quick API Example
→ Header files in `include/`

**...optimize for my platform?**
→ [README.md](README.md) - Edge AI Deployment section

**...understand the architecture?**
→ [README.md](README.md) - Core Components section

**...see performance data?**
→ [README.md](README.md) - Real-World Execution Results
→ [CHANGELOG.md](CHANGELOG.md) - Performance comparison

**...learn about AI collaboration?**
→ [CREDITS.md](CREDITS.md) - Complete methodology

**...contribute?**
→ [TODO.md](TODO.md) - Planned features
→ [CREDITS.md](CREDITS.md) - Development philosophy

---

## 📊 Key Specifications (Quick Reference)

```
Language:        Pure C99
Dependencies:    None (only stdlib + libm)
Memory Footprint: 63 MB peak RSS
Inference Speed:  278,133 passes/sec
Energy:          0.054 mJ/inference
Collapse Ratio:  93.58%
FLOPs Saved:     93.21%
License:         MIT
```

---

## 🎓 Learning Path

### Beginner (Just want to use it)
1. [QUICKSTART.md](QUICKSTART.md) - 5 minutes
2. Run benchmark - 2 minutes
3. Try API example - 10 minutes

**Total: ~20 minutes to productive use**

### Intermediate (Want to understand it)
1. [README.md](README.md) - Full read - 30 minutes
2. Browse header files - 20 minutes
3. Study benchmark code - 20 minutes

**Total: ~70 minutes to solid understanding**

### Advanced (Want to modify/extend it)
1. All documentation - 1 hour
2. Read all headers thoroughly - 1 hour
3. Study implementation files - 2 hours
4. Experiment and benchmark - 1 hour

**Total: ~5 hours to mastery**

---

## 🤝 About This Project

EventHorizon Engine is an **AI-assisted open source project** that demonstrates:
- What's possible with human-AI collaboration
- Production-quality systems programming with AI help
- Transparent disclosure of AI contributions
- Real-world validation and benchmarking

See [CREDITS.md](CREDITS.md) for full details on the collaborative development process.

---

## 📞 Need Help?

1. **Can't find something?** - Use Ctrl+F in [README.md](README.md)
2. **Compilation issues?** - Check [QUICKSTART.md](QUICKSTART.md) troubleshooting
3. **Performance questions?** - See benchmark results in [README.md](README.md)
4. **Want to contribute?** - Check [TODO.md](TODO.md) for ideas

---

**Happy coding! 🚀**

*EventHorizon Engine - Human creativity meets AI execution*
