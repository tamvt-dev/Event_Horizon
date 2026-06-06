# EventHorizon Engine - Tests Makefile

CC = gcc
CFLAGS = -O2 -std=c99 -Wall -Wextra -g
LIBS = -lm

INCDIR = ../include
SRCDIR = ../src
CORE_SRCS = $(SRCDIR)/core/*.c
HGN_SRCS = $(SRCDIR)/hgn/*.c

# Test targets
TESTS = test_arena test_dag test_engine test_eh_hgn_dag test_eh_hgn_beam test_eh_hgn_collapse test_eh_hgn_engine test_eh_hgn_io test_eh_hgn_builder

.PHONY: all test clean help

all: $(TESTS)
	@echo "✓ All tests built"

test_arena: test_arena.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_arena.c $(CORE_SRCS) $(HGN_SRCS) -o test_arena $(LIBS)

test_dag: test_dag.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_dag.c $(CORE_SRCS) $(HGN_SRCS) -o test_dag $(LIBS)

test_engine: test_engine.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_engine.c $(CORE_SRCS) $(HGN_SRCS) -o test_engine $(LIBS)

test_eh_hgn_dag: test_eh_hgn_dag.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_dag.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_dag $(LIBS)

test_eh_hgn_beam: test_eh_hgn_beam.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_beam.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_beam $(LIBS)

test_eh_adaptive: legacy/test_eh_adaptive.c $(CORE_SRCS) $(HGN_SRCS) $(SRCDIR)/hgn/legacy/eh_adaptive.c
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn -I$(INCDIR)/hgn/legacy legacy/test_eh_adaptive.c $(CORE_SRCS) $(HGN_SRCS) $(SRCDIR)/hgn/legacy/eh_adaptive.c -o test_eh_adaptive $(LIBS)

test_eh_hgn_collapse: test_eh_hgn_collapse.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_collapse.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_collapse $(LIBS)

test_eh_hgn_engine: test_eh_hgn_engine.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_engine.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_engine $(LIBS)

test_eh_hgn_io: test_eh_hgn_io.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_io.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_io $(LIBS)

test_eh_hgn_builder: test_eh_hgn_builder.c $(CORE_SRCS) $(HGN_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR)/core -I$(INCDIR)/hgn test_eh_hgn_builder.c $(CORE_SRCS) $(HGN_SRCS) -o test_eh_hgn_builder $(LIBS)

test: all
	@echo "Running all tests..."
	@echo ""
	@./test_arena && echo "" && ./test_dag && echo "" && ./test_engine && echo "" && ./test_eh_hgn_dag && echo "" && ./test_eh_hgn_beam && echo "" && ./test_eh_hgn_collapse && echo "" && ./test_eh_hgn_engine && echo "" && ./test_eh_hgn_io && echo "" && ./test_eh_hgn_builder
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
