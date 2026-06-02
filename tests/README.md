# EventHorizon Engine - Unit Tests

Comprehensive unit tests for core functionality.

## 🧪 Test Suites

| Test Suite | File | Coverage |
|------------|------|----------|
| Arena Tests | `test_arena.c` | Memory arena functionality |
| DAG Tests | `test_dag.c` | Graph operations |
| Engine Tests | `test_engine.c` | Inference engine |

## 🚀 Running Tests

### Run All Tests
```bash
make test
```

### Build and Run Individual Tests
```bash
make test_arena && ./test_arena
make test_dag && ./test_dag
make test_engine && ./test_engine
```

### Clean
```bash
make clean
```

## 📊 Test Coverage

### Arena Tests (7 tests)
- ✓ Creation/destruction
- ✓ Basic allocation
- ✓ 16-byte alignment
- ✓ Out of memory handling
- ✓ Arena reset (O(1))
- ✓ Node allocation
- ✓ Zero-size allocation edge case

### DAG Tests (3 tests)
- ✓ Node creation
- ✓ Node connection
- ✓ State collapse evaluation

### Engine Tests (2 tests)
- ✓ Engine setup
- ✓ Basic inference

**Total: 12 unit tests**

## 🎯 Adding New Tests

1. Create `test_feature.c`:
```c
#include <stdio.h>
#include "eh_feature.h"

#define TEST(name) printf("  Testing: %s ... ", name);
#define PASS() printf("PASS\n");
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_something(void) {
    TEST("feature functionality");
    
    // Your test code
    if (condition) FAIL("Error message");
    
    PASS();
    return 0;
}

int main(void) {
    int failed = 0;
    failed += test_something();
    return (failed == 0) ? 0 : 1;
}
```

2. Add to Makefile:
```makefile
test_feature: test_feature.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) test_feature.c $(CORE_SRCS) -o test_feature $(LIBS)
```

3. Update TESTS list:
```makefile
TESTS = test_arena test_dag test_engine test_feature
```

## 🐛 Debugging Failed Tests

### Use GDB
```bash
gdb ./test_arena
(gdb) run
(gdb) backtrace
```

### Add Debug Prints
```c
printf("DEBUG: value = %d\n", value);
```

### Compile with Debug Symbols
Already included: `-g` flag in CFLAGS

## 📈 Future Tests

Planned test suites:
- [ ] Scoring core tests
- [ ] Neuroplasticity tests
- [ ] Dynamic collapse tests
- [ ] Tokenizer tests
- [ ] Integration tests
- [ ] Performance regression tests

## 🤝 Contributing Tests

When contributing:
1. Write tests for new features
2. Ensure all existing tests pass
3. Aim for >80% code coverage
4. Test edge cases and error conditions
5. Document what each test verifies

## 📝 Test Output Format

```
EventHorizon Engine - Arena Unit Tests
======================================

  Testing: arena create/destroy ... PASS
  Testing: basic allocation ... PASS
  Testing: 16-byte alignment ... PASS
  ...

======================================
All tests passed! ✓
```

## ⚡ CI/CD Integration

Tests are automatically run by GitHub Actions on:
- Every push to main/develop
- Every pull request
- See `.github/workflows/ci.yml`

## 📚 See Also

- [examples/](../examples/) - Usage examples
- [CONTRIBUTING.md](../CONTRIBUTING.md) - Contribution guidelines
- [ROADMAP.md](../ROADMAP.md) - Future test plans
