#!/usr/bin/env python3
import subprocess
import sys
import re

# Benchmark dataset: 50 questions with expected keyword/fact matching and domain classification
test_cases = [
    # Identity
    {"q": "what is your name", "expected": ["assistant", "ai"], "domain": "identity"},
    {"q": "who are you", "expected": ["assistant", "ai", "model"], "domain": "identity"},
    {"q": "are you human", "expected": ["no", "ai", "assistant", "not human"], "domain": "identity"},
    {"q": "what is your purpose", "expected": ["help", "assist", "ai"], "domain": "identity"},
    
    # Greetings
    {"q": "how are you", "expected": ["fine", "well", "good", "how"], "domain": "greetings"},
    {"q": "good morning", "expected": ["morning", "hello", "good"], "domain": "greetings"},
    {"q": "hello", "expected": ["hello", "hi", "hey"], "domain": "greetings"},
    {"q": "hi", "expected": ["hello", "hi", "hey"], "domain": "greetings"},
    
    # Capabilities
    {"q": "what can you do", "expected": ["help", "questions", "write", "tasks"], "domain": "capabilities"},
    {"q": "can you help me", "expected": ["yes", "sure", "can", "help"], "domain": "capabilities"},
    {"q": "can you do math", "expected": ["yes", "can", "math", "calculate"], "domain": "capabilities"},
    
    # Math
    {"q": "what is two plus two", "expected": ["four", "4"], "domain": "math"},
    {"q": "what is ten minus three", "expected": ["seven", "7"], "domain": "math"},
    {"q": "what is five times five", "expected": ["twenty five", "25"], "domain": "math"},
    {"q": "what is pi", "expected": ["three", "3.14", "constant"], "domain": "math"},
    
    # Science
    {"q": "what is the sun", "expected": ["star", "nearest"], "domain": "science"},
    {"q": "what is water", "expected": ["liquid", "hydrogen", "oxygen"], "domain": "science"},
    {"q": "what is electricity", "expected": ["movement", "electrons", "charge", "energy"], "domain": "science"},
    {"q": "why is the sky blue", "expected": ["scattering", "light", "blue", "atmosphere"], "domain": "science"},
    {"q": "what is photosynthesis", "expected": ["plants", "sunlight", "food"], "domain": "science"},
    {"q": "what is gravity", "expected": ["force", "pull", "attracts"], "domain": "science"},
    {"q": "what is an atom", "expected": ["smallest", "unit", "element"], "domain": "science"},
    {"q": "what is dna", "expected": ["genetic", "instructions", "life"], "domain": "science"},
    
    # Technology
    {"q": "what is python", "expected": ["programming", "language"], "domain": "technology"},
    {"q": "what is the internet", "expected": ["connects", "network", "computers"], "domain": "technology"},
    {"q": "what is artificial intelligence", "expected": ["machines", "simulate", "human", "learning"], "domain": "technology"},
    {"q": "what is a database", "expected": ["stores", "organizes", "data"], "domain": "technology"},
    {"q": "what is html", "expected": ["language", "web", "pages"], "domain": "technology"},
    {"q": "what is css", "expected": ["style", "design", "pages"], "domain": "technology"},
    {"q": "what is javascript", "expected": ["programming", "interactivity", "web"], "domain": "technology"},
    {"q": "what is a cpu", "expected": ["central", "processing", "unit"], "domain": "technology"},
    {"q": "what is ram", "expected": ["random", "access", "memory"], "domain": "technology"},
    
    # Geography
    {"q": "what is the capital of france", "expected": ["paris"], "domain": "geography"},
    {"q": "what is the largest country", "expected": ["russia"], "domain": "geography"},
    {"q": "what is the highest mountain", "expected": ["everest"], "domain": "geography"},
    {"q": "what is the Amazon", "expected": ["river", "south america"], "domain": "geography"},
    {"q": "where is the sahara", "expected": ["desert", "africa"], "domain": "geography"},
    {"q": "what is the capital of japan", "expected": ["tokyo"], "domain": "geography"},
    {"q": "what is the capital of usa", "expected": ["washington", "dc"], "domain": "geography"},
    {"q": "what is the largest ocean", "expected": ["pacific"], "domain": "geography"},
    
    # Animals
    {"q": "what is a dog", "expected": ["mammal", "pet", "canine", "loyal"], "domain": "animals"},
    {"q": "what is a whale", "expected": ["mammal", "ocean", "largest", "marine"], "domain": "animals"},
    {"q": "what is a cat", "expected": ["mammal", "pet", "purrs", "feline"], "domain": "animals"},
    {"q": "what is a lion", "expected": ["cat", "king", "jungle", "mammal"], "domain": "animals"},
    {"q": "what is a shark", "expected": ["fish", "ocean", "predator", "teeth"], "domain": "animals"},
    {"q": "what is a penguin", "expected": ["bird", "cannot fly", "swims"], "domain": "animals"},
    
    # Philosophy & Fun
    {"q": "what is love", "expected": ["emotion", "feeling", "affection"], "domain": "philosophy"},
    {"q": "what is happiness", "expected": ["emotion", "satisfaction", "joy", "state"], "domain": "philosophy"},
    {"q": "what is the meaning of life", "expected": ["purpose", "meaning", "find", "live"], "domain": "philosophy"},
    {"q": "tell me a joke", "expected": ["why", "did", "joke", "funny"], "domain": "fun"}
]

# Semantic relevance keywords per domain
domain_keywords = {
    "identity": ["assistant", "ai", "model", "me", "name", "called", "bot"],
    "greetings": ["hello", "hi", "hey", "fine", "good", "morning", "how"],
    "capabilities": ["help", "tasks", "write", "do", "math", "calculate", "can"],
    "math": ["plus", "minus", "times", "divided", "equal", "number", "sum", "math", "four", "seven", "twenty", "pi", "constant", "three"],
    "science": ["star", "sun", "earth", "planet", "orbit", "water", "liquid", "gas", "oxygen", "element", "atom", "molecule", "energy", "electricity", "electrons", "charge", "force", "gravity", "evolution", "cell", "dna", "genetic", "photosynthesis", "atmosphere", "sky", "blue", "light", "heat"],
    "technology": ["python", "programming", "language", "internet", "connects", "network", "computers", "server", "database", "data", "html", "css", "style", "design", "javascript", "code", "app", "operating", "system", "hardware", "software", "cpu", "ram", "memory"],
    "geography": ["paris", "france", "capital", "city", "tokyo", "japan", "beijing", "china", "london", "england", "rome", "italy", "washington", "everest", "mountain", "nepal", "himalayas", "amazon", "river", "sahara", "desert", "pacific", "atlantic", "ocean", "largest", "continent", "russia"],
    "animals": ["dog", "cat", "whale", "mammal", "pet", "fish", "shark", "ocean", "lion", "tiger", "stripes", "trunk", "elephant", "dolphin", "bird", "fly", "feathers", "wings", "reptile", "snake", "frog", "amphibian", "insect", "bee", "honey", "butterfly", "penguin"],
    "philosophy": ["love", "happiness", "feeling", "emotion", "state", "mind", "joy", "purpose", "meaning", "life", "exist"],
    "fun": ["joke", "laugh", "funny", "chicken", "road", "cross", "why"]
}

def clean_and_split(text):
    text = text.lower().strip()
    return re.findall(r'\b\w+\b', text)

def check_coherence(tokens):
    if len(tokens) == 0:
        return False
    # Check for excessive repetition (e.g., same word 3 times consecutively)
    consecutive_rep = 0
    last_token = None
    for token in tokens:
        if token == last_token:
            consecutive_rep += 1
            if consecutive_rep >= 2:  # Found 3 in a row
                return False
        else:
            consecutive_rep = 0
        last_token = token
        
    # Check for short cyclic loops (e.g., A B A B)
    if len(tokens) >= 4:
        for i in range(len(tokens) - 3):
            if tokens[i] == tokens[i+2] and tokens[i+1] == tokens[i+3]:
                return False
                
    return True

def run_query(question):
    try:
        cmd = ["./qa_trigram", "ask", "training/trigram_model.ehdag", 
               "training/trigram_vocab.txt", "training/trigram_vocab.txt.pairs", question]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        # Parse answer
        for line in result.stdout.split("\n"):
            if "Answer:" in line:
                return line.split("Answer:")[1].strip()
    except Exception as e:
        pass
    return ""

def main():
    print(f"===========================================================")
    print(f"             EVENTHORIZON QUALITY METRICS EVALUATOR")
    print(f"             Total benchmark test cases: {len(test_cases)}")
    print(f"===========================================================\n")
    
    coherent_count = 0
    relevant_count = 0
    accurate_count = 0
    
    results_by_domain = {}
    
    for idx, case in enumerate(test_cases):
        q = case["q"]
        expected_facts = case["expected"]
        domain = case["domain"]
        
        if domain not in results_by_domain:
            results_by_domain[domain] = {"total": 0, "coherent": 0, "relevant": 0, "accurate": 0}
            
        results_by_domain[domain]["total"] += 1
        
        answer = run_query(q)
        answer_tokens = clean_and_split(answer)
        
        # 1. Structural Coherence
        is_coherent = check_coherence(answer_tokens)
        if is_coherent:
            coherent_count += 1
            results_by_domain[domain]["coherent"] += 1
            
        # 2. Semantic Relevance (contains keywords relevant to the domain)
        rel_keywords = domain_keywords.get(domain, [])
        is_relevant = any(tok in rel_keywords for tok in answer_tokens)
        if is_relevant:
            relevant_count += 1
            results_by_domain[domain]["relevant"] += 1
            
        # 3. Factual Accuracy (contains at least one of the expected facts/keywords)
        is_accurate = any(fact in answer_tokens for fact in expected_facts)
        if is_accurate:
            accurate_count += 1
            results_by_domain[domain]["accurate"] += 1
            
        status_str = f"Coherent: {'OK' if is_coherent else 'FAIL'} | Relevant: {'OK' if is_relevant else 'FAIL'} | Accurate: {'OK' if is_accurate else 'FAIL'}"
        print(f"[{idx+1:2d}] Q: {q}")
        print(f"     A: {answer if answer else '[No Answer]'}")
        print(f"     {status_str}\n")
        
    total = len(test_cases)
    coherence_pct = (coherent_count / total) * 100
    relevance_pct = (relevant_count / total) * 100
    accuracy_pct = (accurate_count / total) * 100
    
    print("===========================================================")
    print("                 FINAL SUMMARY REPORT")
    print("===========================================================")
    print(f"Structural Coherence : {coherent_count}/{total} ({coherence_pct:.1f}%)")
    print(f"Semantic Relevance   : {relevant_count}/{total} ({relevance_pct:.1f}%)")
    print(f"Factual Accuracy     : {accurate_count}/{total} ({accuracy_pct:.1f}%)")
    print("===========================================================\n")
    
    print("Domain Breakdown:")
    print(f"{'Domain':15s} | {'Total':5s} | {'Coherent':8s} | {'Relevant':8s} | {'Accurate':8s}")
    print("-" * 55)
    for domain, stats in results_by_domain.items():
        print(f"{domain:15s} | {stats['total']:5d} | {stats['coherent']:8d} | {stats['relevant']:8d} | {stats['accurate']:8d}")
    print("")

if __name__ == "__main__":
    main()
