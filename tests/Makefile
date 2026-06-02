# EventHorizon Engine - Tests Makefile

CC = gcc
CFLAGS = -O2 -std=c99 -Wall -Wextra -g
LIBS = -lm

INCDIR = ../include
SRCDIR = ../src/core

CORE_SRCS = $(SRCDIR)/*.c

# Test targets
TESTS = test_arena test_dag test_engine

.PHONY: all test clean help

all: $(TESTS)
	@echo "✓ All tests built"

test_arena: test_arena.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) test_arena.c $(CORE_SRCS) -o test_arena $(LIBS)

test_dag: test_dag.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) test_dag.c $(CORE_SRCS) -o test_dag $(LIBS)

test_engine: test_engine.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) test_engine.c $(CORE_SRCS) -o test_engine $(LIBS)

test: all
	@echo "Running all tests..."
	@echo ""
	@./test_arena && echo "" && ./test_dag && echo "" && ./test_engine
	@echo ""
	@echo "======================================"
	@echo "All test suites completed!"

clean:
	rm -f $(TESTS)
	@echo "✓ Tests cleaned"

help:
	@echo "EventHorizon Engine - Tests"
	@echo ""
	@echo "Targets:"
	@echo "  make        - Build all tests"
	@echo "  make test   - Build and run all tests"
	@echo "  make clean  - Remove built tests"
