# Contributing to EventHorizon Engine

Thank you for your interest in contributing to EventHorizon Engine! This document provides guidelines and instructions for contributing.

---

## 🎯 Ways to Contribute

### Code Contributions
- **Bug fixes** - Fix issues or improve stability
- **Performance optimizations** - Speed up execution or reduce memory
- **Platform ports** - Add support for new platforms (ESP32, STM32, etc.)
- **Feature implementations** - Work on ROADMAP items

### Documentation
- **Improve README** - Clarify explanations
- **Add examples** - Create usage examples
- **Write tutorials** - Step-by-step guides
- **Translate docs** - Help non-English speakers

### Testing & Validation
- **Run benchmarks** - On different hardware
- **Report bugs** - With reproducible examples
- **Test edge cases** - Find potential issues
- **Validate correctness** - Verify algorithms

---

## 🚀 Getting Started

### 1. Fork & Clone
```bash
# Fork the repository on GitHub
# Then clone your fork
git clone https://github.com/YOUR_USERNAME/eventhorizon.git
cd eventhorizon
```

### 2. Build & Test
```bash
# Build all targets
make build-all

# Run benchmark
make run-bench

# Run tests
make run-test
```

### 3. Create a Branch
```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/your-bug-fix
```

---

## 📋 Development Guidelines

### Code Quality Standards

#### C99 Compliance
- Use pure C99 standard
- No C++ features
- No compiler-specific extensions (except in platform-specific code)

#### Style Guide
```c
// ✅ Good: Clear naming, documented
/**
 * Allocate memory from arena with alignment.
 * Returns NULL if arena is full.
 */
void *eh_arena_alloc(EH_Arena *arena, size_t size);

// ❌ Bad: Unclear, no documentation
void *alloc(void *a, int s);
```

#### Performance First
- Profile before optimizing
- Measure with benchmarks
- Document performance changes

#### Memory Safety
- No malloc/free in hot paths
- Use arena allocator
- Check buffer bounds
- Validate pointers

### Code Organization
```
include/        - Public API headers
src/core/       - Core implementations
src/            - Test and example code
bench_all.c     - Comprehensive benchmarks
```

---

## 🧪 Testing Requirements

### Before Submitting
1. **Build successfully**: `make clean && make build-all`
2. **Run benchmarks**: `make run-bench`
3. **Verify performance**: No regressions
4. **Test on target platform**: If platform-specific

### Benchmark Requirements
- Provide before/after performance data
- Test on relevant hardware
- Include memory usage stats
- Document any trade-offs

Example:
```
Before: 278K inferences/sec, 63 MB RAM
After:  320K inferences/sec, 60 MB RAM
Change: +15% speed, -5% memory
```

---

## 📝 Pull Request Process

### 1. Commit Messages
Follow conventional commits:
```
feat: add ARM NEON optimization for matrix multiply
fix: correct buffer overflow in arena allocator
docs: update README with ESP32 instructions
perf: optimize beam search by 20%
```

### 2. PR Description Template
```markdown
## Description
Brief description of changes

## Motivation
Why is this change needed?

## Changes
- List of changes made

## Performance Impact
Before: X inferences/sec
After: Y inferences/sec
Change: +Z%

## Testing
- [ ] Built successfully on Linux
- [ ] Built successfully on macOS  
- [ ] Ran benchmark suite
- [ ] Tested on target platform (if applicable)

## Checklist
- [ ] Code follows C99 standard
- [ ] Documentation updated
- [ ] Benchmarks provided
- [ ] No performance regressions
```

### 3. Review Process
- Maintainers will review your PR
- Address feedback promptly
- Be open to suggestions
- Iterate until approved

---

## 🤖 AI Assistance Disclosure

### Using AI Tools
You **MAY** use AI tools (Claude, GPT, Gemini, etc.) to help with:
- Code generation
- Bug fixes
- Documentation
- Optimization ideas

### Disclosure Requirements
If you use AI assistance, please:
1. **Acknowledge it** in your PR description
2. **Review carefully** - You're responsible for the code
3. **Test thoroughly** - AI-generated code needs validation
4. **Document reasoning** - Explain design decisions

Example:
```markdown
## AI Assistance
- Used Claude to generate initial ARM NEON implementation
- Manually reviewed and optimized the code
- Benchmarked on Raspberry Pi 4
- Achieved 2x speedup vs scalar version
```

---

## 🎯 Priority Areas (from ROADMAP)

### High Priority - Phase 1 (Q3 2026)
- [ ] ARM NEON intrinsics
- [ ] x86 SSE/AVX2 vectorization
- [ ] INT8/INT4 quantization
- [ ] Multi-threading support

### Medium Priority - Phase 2 (Q4 2026)
- [ ] ONNX import/export
- [ ] TensorFlow Lite compatibility
- [ ] Python bindings
- [ ] Profiling tools

### Good First Issues
- [ ] Add more usage examples
- [ ] Write performance tuning guide
- [ ] Create platform-specific builds
- [ ] Improve error messages
- [ ] Add input validation

---

## 📞 Communication

### Questions & Discussion
- **GitHub Issues** - Bug reports, feature requests
- **GitHub Discussions** - General questions, ideas
- **Pull Requests** - Code review, technical discussion

### Response Time
- We aim to respond to issues within 48 hours
- PRs typically reviewed within 1 week
- Complex PRs may take longer

---

## 📄 License

By contributing, you agree that your contributions will be licensed under the Apache-2.0 License.

You retain copyright to your contributions, but grant the project an irrevocable license to use, modify, and distribute your code.

---

## 🙏 Recognition

Contributors are recognized in:
- **CREDITS.md** - Detailed acknowledgments
- **GitHub contributors** - Automatic listing
- **Release notes** - Major contributions highlighted

---

## ⚠️ Code of Conduct

### Our Standards
- **Be respectful** - Treat everyone with respect
- **Be constructive** - Provide helpful feedback
- **Be collaborative** - Work together towards common goals
- **Be inclusive** - Welcome people of all backgrounds

### Unacceptable Behavior
- Harassment or discrimination
- Trolling or insulting comments
- Personal attacks
- Publishing private information

### Enforcement
Violations may result in:
1. Warning
2. Temporary ban
3. Permanent ban

Report issues to project maintainers.

---

## 🎓 Learning Resources

### For New Contributors
- Read [QUICKSTART.md](QUICKSTART.md) first
- Study [README.md](README.md) for architecture
- Review [ROADMAP.md](ROADMAP.md) for priorities
- Check existing issues for ideas

### C99 Resources
- [C99 Standard](https://www.open-std.org/jtc1/sc22/wg14/)
- [Systems Programming](https://github.com/angrave/SystemProgramming/wiki)
- [Performance Optimization](https://www.agner.org/optimize/)

### Edge AI Resources
- [TensorFlow Lite](https://www.tensorflow.org/lite)
- [ONNX Runtime](https://onnxruntime.ai/)
- [ARM NEON Intrinsics](https://developer.arm.com/architectures/instruction-sets/intrinsics/)

---

Thank you for contributing to EventHorizon Engine! 🚀

*Together we're building the future of edge AI inference.*
