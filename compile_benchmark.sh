#!/bin/bash
# EventHorizon Benchmark Compilation Script

echo "=========================================="
echo "  Compiling EventHorizon Benchmark"
echo "=========================================="
echo ""

# Compilation flags
CC="gcc"
CFLAGS="-O3 -std=c99 -Wall -Wextra -march=native"
INCLUDES="-Iinclude"
SOURCES="src/core/*.c bench_all.c"
OUTPUT="eh_engine_ultimate_bench"
LIBS="-lm"

echo "🔨 Compiler: $CC"
echo "🚩 Flags: $CFLAGS"
echo "📦 Output: $OUTPUT"
echo ""

# Compile
echo "⚙️  Compiling..."
$CC $CFLAGS $INCLUDES $SOURCES -o $OUTPUT $LIBS

# Check result
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilation successful!"
    
    # Make executable
    chmod +x $OUTPUT
    
    # Show file info
    SIZE=$(du -h "$OUTPUT" | cut -f1)
    echo "📊 Binary size: $SIZE"
    echo ""
    echo "🚀 Run with: ./$OUTPUT"
else
    echo ""
    echo "❌ Compilation failed!"
    exit 1
fi
