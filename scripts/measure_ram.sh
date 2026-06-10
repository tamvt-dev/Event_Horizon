#!/bin/bash
cd /mnt/c/Users/Administrator/Desktop/my_project/eventhorizon

echo "=== RAM usage measurement ==="

# Method 1: /usr/bin/time -v
echo ""
echo "--- eh_retrieve (retrieval mode) ---"
/usr/bin/time -v sh -c 'echo "what is python" | ./examples/eh_retrieve \
    training/trigram_v20_model.ehdag \
    training/eh_g3_embeddings.bin \
    training/trigram_v20_vocab.txt \
    training/answer_pool.txt > /dev/null' 2>&1 | grep -E "Maximum resident|wall clock|Elapsed"

echo ""
echo "--- eh_query (beam mode) ---"
/usr/bin/time -v sh -c 'echo "what is python" | ./examples/eh_query \
    training/trigram_v20_model.ehdag \
    training/eh_g3_embeddings.bin \
    training/trigram_v20_vocab.txt \
    training/trigram_v20_vocab.txt.pairs > /dev/null' 2>&1 | grep -E "Maximum resident|wall clock|Elapsed"

echo ""
echo "--- File sizes ---"
ls -lh training/trigram_v20_model.ehdag
ls -lh training/answer_pool.txt
ls -lh training/eh_g3_embeddings.bin
ls -lh training/trigram_v20_vocab.txt

echo ""
echo "--- Memory breakdown (estimate) ---"
python3 - << 'EOF'
import os

dag   = os.path.getsize("training/trigram_v20_model.ehdag")
pool  = os.path.getsize("training/answer_pool.txt")
embed = os.path.getsize("training/eh_g3_embeddings.bin")
vocab = os.path.getsize("training/trigram_v20_vocab.txt")

# Pool in memory: 2642 pairs × (256 + 256 + 128*4) bytes = ~2642 × 1024 bytes
pool_ram = 2642 * (256 + 256 + 128*4)

print(f"DAG file:           {dag/1024:.0f} KB")
print(f"Embeddings file:    {embed/1024:.0f} KB")
print(f"Vocab file:         {vocab:.0f} B")
print(f"Answer pool file:   {pool/1024:.0f} KB")
print(f"")
print(f"Runtime memory (estimate):")
print(f"  Arena (DAG):      20 MB  (fixed allocation)")
print(f"  Pool embeddings:  {pool_ram/1024:.0f} KB  (2642 × 128 floats)")
print(f"  Vocab table:      {10000*64/1024:.0f} KB  (10000 words × 64 bytes)")
print(f"  Stack + misc:     ~500 KB")
print(f"  ─────────────────────────")
print(f"  Total estimate:   ~22 MB")
EOF
