#!/bin/bash
# EventHorizon Benchmark Runner with Output Logging

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="benchmark_results_${TIMESTAMP}.txt"

echo "=========================================="
echo "  EventHorizon Engine Benchmark Runner"
echo "=========================================="
echo ""
echo "📝 Results will be saved to: $OUTPUT_FILE"
echo ""

# Run benchmark and save output
./eh_engine_ultimate_bench 2>&1 | tee "$OUTPUT_FILE"

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Benchmark completed successfully!"
    echo "📄 Results saved to: $OUTPUT_FILE"
    
    # Show file size
    SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
    echo "📊 Output file size: $SIZE"
else
    echo ""
    echo "❌ Benchmark failed!"
fi
