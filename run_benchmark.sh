#!/bin/bash
# EventHorizon Benchmark Runner Script

echo "=========================================="
echo "  EventHorizon Engine Benchmark Runner"
echo "=========================================="
echo ""

# Check if executable exists
if [ ! -f "./eh_engine_ultimate_bench" ]; then
    echo "❌ Error: eh_engine_ultimate_bench not found!"
    echo "Please compile first with:"
    echo "  gcc -O3 -std=c99 -Wall -Wextra -Iinclude src/core/*.c bench_all.c -o eh_engine_ultimate_bench -lm"
    exit 1
fi

# Make executable if not already
chmod +x ./eh_engine_ultimate_bench

# Run benchmark
echo "🚀 Starting benchmark..."
echo ""
./eh_engine_ultimate_bench

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Benchmark completed successfully!"
else
    echo ""
    echo "❌ Benchmark failed with exit code $?"
fi
