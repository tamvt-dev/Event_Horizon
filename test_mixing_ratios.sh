#!/bin/bash
# Test different mixing ratios for EH-G2 Hybrid

MODEL="training/trigram_v2_model.ehdag"
VOCAB="training/trigram_v2_vocab.txt"
PAIRS="training/trigram_v2_vocab.txt.pairs"

echo "==================================================================="
echo "EH-G2 Mixing Ratio Benchmark"
echo "==================================================================="
echo ""

# Test questions
QUESTIONS=(
    "what is your name"
    "how are you"
    "who are you"
    "what is a computer"
    "what is the capital of france"
)

# Mixing ratios to test
RATIOS=(0.0 0.2 0.3 0.4 0.5 0.6 0.8 1.0)

for ratio in "${RATIOS[@]}"; do
    local_pct=$(echo "scale=0; (1 - $ratio) * 100" | bc)
    global_pct=$(echo "scale=0; $ratio * 100" | bc)
    
    echo "==================================================================="
    echo "Mix Ratio: ${local_pct}% local + ${global_pct}% global (${ratio})"
    echo "==================================================================="
    echo ""
    
    for question in "${QUESTIONS[@]}"; do
        echo "Q: \"$question\""
        echo -n "A: "
        ./qa_attention ask "$MODEL" "$VOCAB" "$PAIRS" "$question" "$ratio" 2>/dev/null | grep "^Answer:" | sed 's/Answer:   //'
        echo ""
    done
    
    echo ""
done

echo "==================================================================="
echo "Benchmark Complete"
echo "==================================================================="
