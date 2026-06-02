CC = gcc
CFLAGS = -O3 -std=c99 -Wall -Wextra -march=native
LIBS = -lm

INCDIR = include
SRCDIR = src
CORE_SRCS = $(SRCDIR)/core/*.c

# Targets
TARGET_TEST = eh_test
TARGET_BENCH = eh_engine_ultimate_bench
TARGET_NEURO = eh_neuro_test
TARGET_LEARNING = eh_learning_test

# Default target
all: $(TARGET_BENCH)

# Benchmark suite
bench: $(TARGET_BENCH)

$(TARGET_BENCH): bench_all.c $(CORE_SRCS)
	@echo "🔨 Compiling benchmark suite..."
	$(CC) $(CFLAGS) -I$(INCDIR) $(CORE_SRCS) bench_all.c -o $(TARGET_BENCH) $(LIBS)
	@echo "✅ Build complete: $(TARGET_BENCH)"

# Test suite
test: $(TARGET_TEST)

$(TARGET_TEST): $(SRCDIR)/main_test.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) $(CORE_SRCS) $(SRCDIR)/main_test.c -o $(TARGET_TEST) $(LIBS)

# Neuro test
neuro: $(TARGET_NEURO)

$(TARGET_NEURO): $(SRCDIR)/test_neuro.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) $(CORE_SRCS) $(SRCDIR)/test_neuro.c -o $(TARGET_NEURO) $(LIBS)

# Learning test
learning: $(TARGET_LEARNING)

$(TARGET_LEARNING): $(SRCDIR)/main_learning.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) $(CORE_SRCS) $(SRCDIR)/main_learning.c -o $(TARGET_LEARNING) $(LIBS)

# Run benchmark
run-bench: $(TARGET_BENCH)
	@echo "🚀 Running benchmark..."
	./$(TARGET_BENCH)

# Run test
run-test: $(TARGET_TEST)
	./$(TARGET_TEST)

# Run neuro test
run-neuro: $(TARGET_NEURO)
	./$(TARGET_NEURO)

# Clean all
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(TARGET_TEST) $(TARGET_BENCH) $(TARGET_NEURO) $(TARGET_LEARNING)
	rm -f benchmark_results_*.txt
	@echo "✅ Clean complete"

# Build all targets
build-all: $(TARGET_TEST) $(TARGET_BENCH) $(TARGET_NEURO) $(TARGET_LEARNING)
	@echo "✅ All targets built successfully"

# Help
help:
	@echo "EventHorizon Engine - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build benchmark suite (default)"
	@echo "  make bench        - Build benchmark suite"
	@echo "  make test         - Build test suite"
	@echo "  make neuro        - Build neuro test"
	@echo "  make learning     - Build learning test"
	@echo "  make build-all    - Build all targets"
	@echo ""
	@echo "  make run-bench    - Build and run benchmark"
	@echo "  make run-test     - Build and run test"
	@echo "  make run-neuro    - Build and run neuro test"
	@echo ""
	@echo "  make clean        - Remove all build artifacts"
	@echo "  make help         - Show this help message"

.PHONY: all bench test neuro learning build-all run-bench run-test run-neuro clean help