#!/bin/bash
# Test aggressive hub penalty values
# Goal: Find penalty strength that affects output text

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  EH-G3: Aggressive Hub Penalty Testing                  ║"
echo "║  Testing: 5.0, 10.0, 20.0, 50.0                          ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Test questions to monitor
QUESTIONS=(
    "what is a computer"
    "what is the capital of france"
    "how are you"
)

# Compile first
echo "[1/5] Compiling..."
gcc -O3 -std=c99 -Wall -Wextra -march=native \
    -Iinclude -Iinclude/core -Iinclude/hgn \
    src/core/*.c src/hgn/*.c examples/qa_attention_g3.c \
    -o examples/qa_attention_g3_test -lm 2>&1 | grep -E "error" || echo "✓ Compiled"

if [ ! -f examples/qa_attention_g3_test ]; then
    echo "✗ Compilation failed!"
    exit 1
fi

echo ""
echo "[2/5] Testing hub_penalty = 5.0 (2.5× default)"
echo "─────────────────────────────────────────────"

# For now, just run the existing binary which has hub_penalty=2.0
# We'll need to modify source to test different values

echo ""
echo "NOTE: Current implementation uses fixed hub_penalty=2.0"
echo "To test different values, we need to:"
echo "  1. Add command-line parameter support, OR"
echo "  2. Modify source code for each test"
echo ""
echo "Let's modify source to test 20.0 (strong penalty)..."
echo ""

# Modify source to use hub_penalty=20.0
sed -i 's/hub_penalty = 2\.0f/hub_penalty = 20.0f/' examples/qa_attention_g3.c

echo "[3/5] Recompiling with hub_penalty=20.0..."
gcc -O3 -std=c99 -Wall -Wextra -march=native \
    -Iinclude -Iinclude/core -Iinclude/hgn \
    src/core/*.c src/hgn/*.c examples/qa_attention_g3.c \
    -o examples/qa_attention_g3_test -lm 2>&1 | grep -E "error" || echo "✓ Compiled with hub_penalty=20.0"

echo ""
echo "[4/5] Running test with hub_penalty=20.0..."
echo "─────────────────────────────────────────────"
./examples/qa_attention_g3_test training/trigram_v2_model.ehdag training/eh_g3_embeddings.bin 2>&1 | \
    grep -A 2 "Q: what is a computer\|Q: what is the capital of france\|Q: how are you" | head -20

echo ""
echo "[5/5] Restoring original hub_penalty=2.0..."
sed -i 's/hub_penalty = 20\.0f/hub_penalty = 2.0f/' examples/qa_attention_g3.c

echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║  Test Complete - Check if output changed                ║"
echo "╚══════════════════════════════════════════════════════════╝"
