#!/bin/bash
cd /mnt/c/Users/Administrator/Desktop/my_project/eventhorizon

# Generate 200 queries (mix of correct and typo)
python3 -c "
queries = [
    'what is graivity', 'what is einsten', 'what is evoluion',
    'pytohn is what', 'what iz gravity', 'what is algoritm',
    'what is python', 'what is gravity', 'who is einstein',
    'define algorithm', 'what is evolution', 'what is dna',
] * 17
for q in queries[:200]:
    print(q)
" > /tmp/bench_queries.txt

echo "Running 200 queries (mix correct + typos)..."
time (./examples/eh_retrieve \
    training/trigram_v20_model.ehdag \
    training/eh_g3_embeddings.bin \
    training/trigram_v20_vocab.txt \
    training/answer_pool.txt \
    < /tmp/bench_queries.txt > /dev/null 2>/dev/null)

echo ""
echo "Per-query latency:"
python3 -c "
import subprocess, time

queries = ['what is graivity', 'what is einsten', 'what is python'] * 10
start = time.time()
r = subprocess.run(
    ['./examples/eh_retrieve',
     'training/trigram_v20_model.ehdag',
     'training/eh_g3_embeddings.bin',
     'training/trigram_v20_vocab.txt',
     'training/answer_pool.txt'],
    input='\n'.join(queries)+'\n',
    capture_output=True, text=True
)
elapsed = time.time() - start
print(f'  {len(queries)} queries in {elapsed*1000:.1f}ms')
print(f'  Per query: {elapsed*1000/len(queries):.2f}ms')
"
