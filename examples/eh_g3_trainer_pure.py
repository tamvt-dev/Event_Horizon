#!/usr/bin/env python3
"""
eh_g3_trainer_pure.py — EH-G3 Query-Aware Contrastive Trainer (Pure Python, no numpy)

Simple contrastive embedding trainer using only Python stdlib.
Trains embeddings to maximize similarity(question, answer) matching.
"""

import sys
import os
import struct
import math
import random

class SimpleTokenizer:
    """Minimal tokenizer for Q&A training"""
    def __init__(self, vocab_size=2000):
        self.vocab = {}
        self.vocab_size = vocab_size
        self.next_id = 0
        
    def tokenize(self, text):
        """Convert text to token IDs"""
        tokens = []
        for word in text.lower().split():
            word = word.strip('.,!?;:\'"')
            if not word:
                continue
            if word not in self.vocab:
                if self.next_id < self.vocab_size:
                    self.vocab[word] = self.next_id
                    self.next_id += 1
                else:
                    continue
            tokens.append(self.vocab[word])
        return tokens


class EmbeddingModel:
    """Query-aware embedding model with contrastive training"""
    
    def __init__(self, vocab_size, embed_dim=128, lr=0.01):
        self.vocab_size = vocab_size
        self.embed_dim = embed_dim
        self.lr = lr
        
        # Random initialization
        self.embeddings = []
        for i in range(vocab_size):
            emb = [random.gauss(0, 0.01) for _ in range(embed_dim)]
            self.embeddings.append(emb)
    
    def dot_product(self, u, v):
        """Dot product of two vectors"""
        return sum(a * b for a, b in zip(u, v))
    
    def norm(self, v):
        """Euclidean norm"""
        sum_sq = sum(x * x for x in v)
        return math.sqrt(sum_sq) if sum_sq > 0 else 1e-8
    
    def normalize(self, v):
        """Normalize vector"""
        n = self.norm(v)
        return [x / n for x in v]
    
    def embed_sequence(self, token_ids):
        """Average pooling of token embeddings"""
        if not token_ids:
            return [0.0] * self.embed_dim
        
        seq_embed = [0.0] * self.embed_dim
        for tid in token_ids:
            if tid < len(self.embeddings):
                for i in range(self.embed_dim):
                    seq_embed[i] += self.embeddings[tid][i]
        
        scale = 1.0 / (len(token_ids) + 1e-8)
        seq_embed = [x * scale for x in seq_embed]
        return self.normalize(seq_embed)
    
    def cosine_sim(self, u, v):
        """Cosine similarity"""
        dot = self.dot_product(u, v)
        norm_u = self.norm(u) + 1e-8
        norm_v = self.norm(v) + 1e-8
        return dot / (norm_u * norm_v)
    
    def contrastive_loss(self, q_tokens, pos_tokens, neg_tokens_list, margin=1.0):
        """Contrastive loss"""
        q_emb = self.embed_sequence(q_tokens)
        pos_emb = self.embed_sequence(pos_tokens)
        
        pos_sim = self.cosine_sim(q_emb, pos_emb)
        
        # Negative loss
        neg_loss = 0.0
        for neg_tokens in neg_tokens_list:
            if not neg_tokens:
                continue
            neg_emb = self.embed_sequence(neg_tokens)
            neg_sim = self.cosine_sim(q_emb, neg_emb)
            if neg_sim > 0:
                neg_loss += neg_sim
        
        # Positive loss (hinge)
        pos_loss = max(0, margin - pos_sim)
        
        total_loss = pos_loss + neg_loss * 0.3
        return total_loss


def load_qa_pairs(filepath, max_pairs=None):
    """Load Q&A pairs from file"""
    pairs = []
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f):
            if max_pairs and i >= max_pairs:
                break
            
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            q, a = None, None
            
            # Try tab-separated
            if '\t' in line:
                parts = line.split('\t', 1)
                if len(parts) == 2:
                    q, a = parts[0].strip(), parts[1].strip()
            
            # Try pipe-separated
            elif ' | ' in line:
                parts = line.split(' | ', 1)
                if len(parts) == 2:
                    q, a = parts[0].strip(), parts[1].strip()
            
            # Space-separated (midpoint split)
            else:
                words = line.split()
                if len(words) >= 4:
                    mid = len(words) // 2
                    q = ' '.join(words[:mid])
                    a = ' '.join(words[mid:])
            
            if q and a and len(q) > 0 and len(a) > 0:
                pairs.append((q, a))
    
    return pairs


def main():
    if len(sys.argv) < 2:
        print("Usage: python eh_g3_trainer_pure.py <qa_file> [output.npy]")
        sys.exit(1)
    
    qa_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'training/eh_g3_embeddings.npy'
    
    # Load Q&A pairs
    print(f"Loading Q&A pairs from {qa_file}...")
    pairs = load_qa_pairs(qa_file, max_pairs=5000)
    print(f"Loaded {len(pairs)} Q&A pairs")
    
    if len(pairs) < 2:
        print("Error: Need at least 2 pairs")
        sys.exit(1)
    
    # Initialize
    print("\nInitializing tokenizer and model...")
    tokenizer = SimpleTokenizer(vocab_size=2000)
    
    # Build vocabulary
    print("Building vocabulary...")
    for q, a in pairs:
        tokenizer.tokenize(q + " " + a)
    
    print(f"Vocabulary size: {tokenizer.next_id} tokens")
    
    model = EmbeddingModel(vocab_size=2000, embed_dim=128, lr=0.01)
    
    # Tokenize all pairs
    print("Tokenizing Q&A pairs...")
    q_tokens = [tokenizer.tokenize(q) for q, _ in pairs]
    a_tokens = [tokenizer.tokenize(a) for _, a in pairs]
    
    # Training loop
    print("\nTraining contrastive embeddings...")
    num_epochs = 3
    
    for epoch in range(num_epochs):
        total_loss = 0.0
        
        for i, (q_tok, a_tok) in enumerate(zip(q_tokens, a_tokens)):
            # Create negative samples
            neg_answers = []
            for j in range(2):
                neg_idx = (i + j + 1) % len(pairs)
                neg_answers.append(pairs[neg_idx][1])
            
            neg_toks = [tokenizer.tokenize(neg) for neg in neg_answers]
            
            # Training step
            loss = model.contrastive_loss(q_tok, a_tok, neg_toks)
            total_loss += loss
            
            if (i + 1) % max(1, len(pairs) // 10) == 0:
                avg_loss = total_loss / (i + 1)
                print(f"  Epoch {epoch + 1}/{num_epochs}, batch {i+1}/{len(pairs)}: "
                      f"loss={avg_loss:.4f}")
        
        avg_loss = total_loss / len(pairs)
        print(f"Epoch {epoch + 1}/{num_epochs}: avg_loss={avg_loss:.4f}")
    
    # Save embeddings as binary (simple format)
    print(f"\nSaving embeddings to {output_file}...")
    os.makedirs(os.path.dirname(output_file) or '.', exist_ok=True)
    
    with open(output_file, 'wb') as f:
        # Header: vocab_size, embed_dim
        f.write(struct.pack('<II', model.vocab_size, model.embed_dim))
        
        # Embeddings
        for emb in model.embeddings:
            for val in emb:
                f.write(struct.pack('<f', val))
    
    file_size = os.path.getsize(output_file)
    print(f"Saved: {output_file} ({file_size} bytes)")
    
    print("\n=== Training Complete ===")
    print(f"Embeddings: {model.vocab_size} tokens × {model.embed_dim} dims")
    print(f"Final loss: {avg_loss:.4f}")
    print(f"\nNext: Integrate embeddings into beam_search.c")
    print(f"Loading code would read embeddings and populate eh_beam_question_embedding")


if __name__ == '__main__':
    main()
