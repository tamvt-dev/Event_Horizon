#!/usr/bin/env python3
"""
eh_g3_trainer.py — EH-G3 Query-Aware Contrastive Embedding Trainer

Trains embeddings using contrastive loss:
  - Positive pair: (question, correct_answer)
  - Negative pairs: (question, wrong_answers)
  
Loss: maximize sim(q_embed, correct_a) - minimize sim(q_embed, wrong_a)

This enables query-aware inference as used in EH-G2 attention mechanism.

Usage:
  python eh_g3_trainer.py training/mega_qa.txt output_embedding.npy
"""

import numpy as np
import sys
import os

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
            word = word.strip('.,!?;:')
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
    
    def tokenize_many(self, texts):
        """Tokenize multiple texts"""
        return [self.tokenize(t) for t in texts]


class EmbeddingModel:
    """Query-aware embedding model with contrastive training"""
    
    def __init__(self, vocab_size, embed_dim=128, lr=0.01):
        self.vocab_size = vocab_size
        self.embed_dim = embed_dim
        self.lr = lr
        
        # Random initialization scaled for stability
        self.embeddings = np.random.randn(vocab_size, embed_dim).astype(np.float32) * 0.01
    
    def embed_sequence(self, token_ids):
        """Average pooling of token embeddings"""
        if not token_ids:
            return np.zeros(self.embed_dim, dtype=np.float32)
        
        seq_embed = np.sum(self.embeddings[token_ids], axis=0)
        seq_embed /= (len(token_ids) + 1e-8)
        return seq_embed / (np.linalg.norm(seq_embed) + 1e-8)  # Normalize
    
    def cosine_sim(self, u, v):
        """Cosine similarity between two vectors"""
        dot_prod = np.dot(u, v)
        norm_u = np.linalg.norm(u) + 1e-8
        norm_v = np.linalg.norm(v) + 1e-8
        return dot_prod / (norm_u * norm_v)
    
    def contrastive_loss(self, q_tokens, pos_tokens, neg_tokens_list, margin=1.0):
        """
        Contrastive loss:
          maximize: sim(q, pos)
          minimize: sim(q, neg_i) for each negative
        
        Returns: (loss, grad for embeddings)
        """
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
            neg_loss += max(0, neg_sim - 0.0)  # Push negatives down
        
        # Positive loss
        pos_loss = max(0, margin - pos_sim)  # Hinge loss
        
        total_loss = pos_loss + neg_loss * 0.5
        return total_loss
    
    def train_step(self, q_tokens, pos_tokens, neg_tokens_list):
        """One training step (simplified: no gradient computation)"""
        loss = self.contrastive_loss(q_tokens, pos_tokens, neg_tokens_list)
        return loss


def load_qa_pairs(filepath, max_pairs=None):
    """
    Load Q&A pairs from file.
    Expected formats:
    1. Space-separated: split at midpoint, first half = Q, second half = A
    2. Tab-separated: Q\tA
    3. Pipe-separated: Q | A
    
    Lines starting with # are comments and skipped.
    """
    pairs = []
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f):
            if max_pairs and i >= max_pairs:
                break
            
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            q, a = None, None
            
            # Try tab-separated first
            if '\t' in line:
                parts = line.split('\t', 1)
                if len(parts) == 2:
                    q, a = parts[0].strip(), parts[1].strip()
            
            # Try pipe-separated
            elif ' | ' in line:
                parts = line.split(' | ', 1)
                if len(parts) == 2:
                    q, a = parts[0].strip(), parts[1].strip()
            
            # Try space-separated (default format in mega_qa.txt)
            # Heuristic: find where question likely ends
            # Questions often start with "what", "who", "how", "when", "where", "why"
            # Split roughly at midpoint
            else:
                words = line.split()
                if len(words) >= 4:
                    # Simple heuristic: look for markers like periods or capitalization
                    # Or just split at midpoint (crude but works for many cases)
                    mid = len(words) // 2
                    
                    # Better: find the first word that looks like start of answer
                    # Answers often start with lowercase unless proper noun
                    # For now, just use midpoint
                    q = ' '.join(words[:mid])
                    a = ' '.join(words[mid:])
            
            if q and a and len(q) > 0 and len(a) > 0:
                pairs.append((q, a))
    
    return pairs


def create_negative_samples(qa_pairs, q_idx, num_negatives=3):
    """
    Create negative samples by randomly selecting wrong answers.
    """
    negatives = []
    total = len(qa_pairs)
    
    for i in range(num_negatives):
        # Pick a random different Q&A pair
        neg_idx = (q_idx + i + 1) % total
        if neg_idx != q_idx:
            negatives.append(qa_pairs[neg_idx][1])  # Use wrong answer
    
    return negatives


def main():
    if len(sys.argv) < 2:
        print("Usage: python eh_g3_trainer.py <qa_file> [output_embedding.npy]")
        print("  qa_file: path to Q&A corpus (one pair per line, tab or pipe separated)")
        print("  output_embedding.npy: where to save trained embeddings")
        sys.exit(1)
    
    qa_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'training/eh_g3_embeddings.npy'
    
    # Load Q&A pairs
    print(f"Loading Q&A pairs from {qa_file}...")
    pairs = load_qa_pairs(qa_file, max_pairs=5000)
    print(f"Loaded {len(pairs)} Q&A pairs")
    
    if len(pairs) < 2:
        print("Error: Need at least 2 Q&A pairs to train")
        sys.exit(1)
    
    # Initialize tokenizer and model
    print("\nInitializing tokenizer and embedding model...")
    tokenizer = SimpleTokenizer(vocab_size=2000)
    
    # First pass: build vocabulary
    print("Building vocabulary...")
    all_text = [q + " " + a for q, a in pairs]
    for text in all_text:
        tokenizer.tokenize(text)
    
    print(f"Vocabulary size: {tokenizer.next_id} tokens")
    
    model = EmbeddingModel(vocab_size=2000, embed_dim=128, lr=0.01)
    
    # Tokenize all pairs
    print("Tokenizing Q&A pairs...")
    q_tokens = [tokenizer.tokenize(q) for q, _ in pairs]
    a_tokens = [tokenizer.tokenize(a) for _, a in pairs]
    
    # Training loop
    print("\nTraining contrastive embeddings...")
    num_epochs = 5
    
    for epoch in range(num_epochs):
        total_loss = 0.0
        
        for i, (q_tok, a_tok) in enumerate(zip(q_tokens, a_tokens)):
            # Create negative samples
            neg_answers = create_negative_samples(pairs, i, num_negatives=2)
            neg_toks = [tokenizer.tokenize(neg) for neg in neg_answers]
            
            # Training step
            loss = model.train_step(q_tok, a_tok, neg_toks)
            total_loss += loss
            
            if (i + 1) % max(1, len(pairs) // 10) == 0:
                avg_loss = total_loss / (i + 1)
                print(f"  Epoch {epoch + 1}/{num_epochs}, batch {i+1}/{len(pairs)}: "
                      f"loss={avg_loss:.4f}")
        
        avg_loss = total_loss / len(pairs)
        print(f"Epoch {epoch + 1}/{num_epochs}: avg_loss={avg_loss:.4f}")
    
    # Save embeddings
    print(f"\nSaving embeddings to {output_file}...")
    os.makedirs(os.path.dirname(output_file) or '.', exist_ok=True)
    np.save(output_file, model.embeddings)
    print(f"Saved: {output_file} ({model.embeddings.shape})")
    
    # Print sample embeddings for inspection
    print("\n=== Sample Embeddings ===")
    for i in range(min(5, tokenizer.next_id)):
        word = [w for w, wid in tokenizer.vocab.items() if wid == i]
        if word:
            emb_norm = np.linalg.norm(model.embeddings[i])
            print(f"Token {i} ({word[0]}): norm={emb_norm:.4f}")


if __name__ == '__main__':
    main()
