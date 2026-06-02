# EventHorizon Engine - Examples Makefile

CC = gcc
CFLAGS = -O2 -std=c99 -Wall -Wextra
LIBS = -lm

INCDIR = ../include
SRCDIR = ../src/core

CORE_SRCS = $(SRCDIR)/*.c

# Example targets
EXAMPLES = hello_world arena_demo graph_demo

.PHONY: all clean help

all: $(EXAMPLES)
	@echo "✓ All examples built successfully"

hello_world: hello_world.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) hello_world.c $(CORE_SRCS) -o hello_world $(LIBS)
	@echo "✓ Built: hello_world"

arena_demo: arena_demo.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) arena_demo.c $(CORE_SRCS) -o arena_demo $(LIBS)
	@echo "✓ Built: arena_demo"

graph_demo: graph_demo.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -I$(INCDIR) graph_demo.c $(CORE_SRCS) -o graph_demo $(LIBS)
	@echo "✓ Built: graph_demo"

clean:
	rm -f $(EXAMPLES)
	@echo "✓ Examples cleaned"

help:
	@echo "EventHorizon Engine - Examples"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build all examples"
	@echo "  make hello_world  - Build hello_world example"
	@echo "  make arena_demo   - Build arena_demo example"
	@echo "  make graph_demo   - Build graph_demo example"
	@echo "  make clean        - Remove built examples"
	@echo ""
	@echo "Run examples:"
	@echo "  ./hello_world"
	@echo "  ./arena_demo"
	@echo "  ./graph_demo"
	@echo "  python3 python_demo.py  (requires bindings)"
