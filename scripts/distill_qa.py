#!/usr/bin/env python3
"""
distill_qa.py â€” Teacher-distilled Q&A corpus generator for EH-G3
Optimized for high-performance extraction, dynamic token trimming, and structural durability.
"""

import os
import sys
import json
import time
import random
import logging
import argparse
import threading
import re
from pathlib import Path
from dataclasses import dataclass
from typing import Optional
from urllib import request
from urllib.request import Request
from urllib.error import HTTPError, URLError

# â”€â”€ Logging â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("distill")

# â”€â”€ Provider configs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
PROVIDERS = {
    "gemini": {
        "base_url": "https://generativelanguage.googleapis.com/v1beta/openai",
        "env_key": "GEMINI_API_KEY",
        "models": [
            "gemini-2.5-flash",
            "gemini-2.5-flash-lite",
            "gemini-3-flash-preview",
            "gemini-3.1-flash-lite-preview",
        ],
        "rpm": 15,
    },
    "groq": {
        "base_url": "https://api.groq.com/openai/v1",
        "env_key": "GROQ_API_KEY",
        "models": [
            "llama-3.3-70b-versatile",
            "llama-3.1-8b-instant",
        ],
        "rpm": 30,
    },
"openrouter": {
        "base_url": "https://openrouter.ai/api/v1",
        "env_key": "OPENROUTER_API_KEY",
        "models": [
            # Free tier models on OpenRouter
            'meta-llama/llama-3.3-70b',
            'meta-llama/llama-3.1-8b',
            'qwen/qwen3-32b',
            'google/gemini-flash-1.5',
            'mistralai/mistral-7b-instruct',
            'microsoft/phi-3-mini-128k-instruct',
        ],
        "rpm": 20,
    },
    "mock": {
        "base_url": None,
        "env_key": None,
        "models": ["mock"],
        "rpm": 9999,
    },
}

DOMAINS = {
    "technology": [
        "computer","database","algorithm","network","server","api","python","javascript",
        "html","css","compiler","operating system","memory","cpu","gpu","cache","firewall",
        "encryption","cloud computing","machine learning","neural network","deep learning",
        "blockchain","container","microservices","git","linux","bash","sql","json","xml",
        "http","tcp","dns","ip address","router","browser","database query","recursion",
        "sorting algorithm","binary search","data structure","linked list","stack","queue",
        "graph","tree","hash table","array","pointer","memory leak","debugging","testing",
        "agile","devops","ci cd","docker","kubernetes","rest api","graphql","websocket",
        "virtual machine","garbage collection","thread","process","semaphore","deadlock",
    ],
    "science": [
        "gravity","photosynthesis","evolution","dna","cell","atom","molecule","electricity",
        "magnetism","energy","temperature","pressure","velocity","acceleration","mass",
        "density","wave","light","sound","oxygen","hydrogen","carbon","water","fire",
        "radiation","virus","bacteria","photon","black hole","star","planet","solar system",
        "moon","sun","element","compound","mixture","acid","base","ph","oxidation","neutron",
        "proton","electron","nuclear energy","entropy","thermodynamics","quantum mechanics",
        "relativity","big bang","dna replication","protein","enzyme","photon","laser",
        "semiconductor","superconductor","plasma","fission","fusion","isotope","catalyst",
    ],
    "geography": [
        "france","japan","germany","italy","spain","china","india","canada","australia",
        "brazil","russia","mexico","egypt","argentina","south korea","indonesia","turkey",
        "poland","netherlands","sweden","norway","denmark","greece","portugal","ukraine",
        "paris","tokyo","berlin","rome","madrid","beijing","delhi","ottawa","canberra",
        "brasilia","moscow","london","washington","cairo","buenos aires","seoul","jakarta",
        "amazon river","nile river","mount everest","sahara desert","pacific ocean",
        "atlantic ocean","arctic","antarctica","himalayas","andes","alps","eiffel tower",
        "great wall","statue of liberty","taj mahal","great barrier reef","amazon rainforest",
    ],
    "history": [
        "world war one","world war two","french revolution","american revolution","roman empire",
        "ancient egypt","renaissance","industrial revolution","cold war","fall of berlin wall",
        "moon landing","civil rights movement","holocaust","colonialism","ottoman empire",
        "mongol empire","black death","magna carta","declaration of independence",
        "albert einstein","isaac newton","charles darwin","marie curie","alan turing",
        "napoleon","cleopatra","julius caesar","alexander the great","abraham lincoln",
        "mahatma gandhi","nelson mandela","winston churchill","shakespeare","leonardo da vinci",
        "columbus","galileo","nikola tesla","thomas edison","wright brothers",
    ],
    "mathematics": [
        "pi","infinity","prime number","fraction","percentage","equation","algebra",
        "geometry","calculus","statistics","probability","function","matrix","vector",
        "pythagorean theorem","square root","integer","decimal","logarithm","factorial",
        "fibonacci sequence","prime factorization","set theory","graph theory",
        "differential equation","integral","derivative","limit","polynomial","complex number",
        "trigonometry","sine","cosine","tangent","exponent","inequality","permutation",
        "combination","binomial theorem","quadratic formula","arithmetic","geometric series",
    ],
    "nature": [
        "mammal","reptile","bird","fish","insect","dog","cat","elephant","lion","tiger",
        "whale","shark","eagle","butterfly","bee","ant","tree","flower","forest","ocean",
        "mountain","river","desert","volcano","earthquake","hurricane","tornado","tsunami",
        "coral reef","rainforest","savanna","tundra","wetland","glacier","atmosphere",
        "weather","climate","rain","snow","wind","cloud","thunder","lightning","rainbow",
        "photosynthesis","ecosystem","food chain","predator","prey","symbiosis","migration",
        "hibernation","camouflage","venom","pollination","germination","metamorphosis",
    ],
    "everyday": [
        "money","bank","education","school","hospital","medicine","law","government",
        "economy","trade","language","music","art","sport","food","sleep","health","time",
        "map","book","library","museum","religion","philosophy","psychology","sociology",
        "democracy","capitalism","communism","inflation","tax","insurance","mortgage",
        "nutrition","calorie","protein","vitamin","exercise","meditation","yoga","stress",
        "memory","learning","creativity","intelligence","emotion","happiness","friendship",
        "family","marriage","culture","tradition","festival","holiday","travel","tourism",
    ],
    "people_roles": [
        "doctor","engineer","teacher","scientist","artist","musician","writer","politician",
        "soldier","police","firefighter","chef","lawyer","judge","architect","pilot",
        "astronaut","philosopher","mathematician","historian","journalist","programmer",
        "designer","accountant","economist","psychologist","biologist","chemist","physicist",
    ],
}

TEMPLATES = [
    "what is {concept}",
    "who is {concept}",
    "where is {concept}",
    "how does {concept} work",
    "what does {concept} mean",
    "what is a {concept}",
    "define {concept}",
]

SYSTEM_PROMPT = """You are a factual Q&A generator for a knowledge graph.
For each question, respond with EXACTLY this format:
answer text here|confidence

Where:
- answer text: factual answer in 5-10 words, lowercase, no punctuation
- confidence: your confidence score 0.0-10.0 (10=certain, 5=somewhat sure)

Never include conversational filler or markdown code blocks."""

@dataclass
class QAPair:
    question: str
    answer: str
    confidence: float
    domain: str
    model: str

@dataclass
class WorkerConfig:
    provider: str
    model: str
    api_key: str
    base_url: str
    rpm: int
    domains: list
    min_confidence: float = 5.0

def call_api(base_url: str, api_key: str, model: str, prompt: str, timeout: int = 30) -> Optional[str]:
    url = f"{base_url}/chat/completions"
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": prompt}
    ]
    payload = json.dumps({
        "model": model,
        "messages": messages,
        "max_tokens": 160,
        "temperature": 0.3,
    }).encode("utf-8")

    headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}",
        # ThÃªm dÃ²ng User-Agent dÆ°á»›i Ä‘Ã¢y Ä‘á»ƒ vÆ°á»£t qua Cloudflare WAF 
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    }

    req = Request(url, data=payload, headers=headers, method="POST")
    try:
        with request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            return data["choices"][0]["message"]["content"].strip()
    except HTTPError as e:
        if e.code == 429:
            raise
        log.warning(f"[API ERROR] {model} returned HTTP {e.code}: {e.reason}")
        return None
    except Exception as e:
        log.warning(f"[CONN ERROR] Connection failed for {model}: {e}")
        return None

def parse_response(text: str, question: str) -> tuple[str, float]:
    if not text:
        return "", 0.0

    # Loáº¡i bá» token nhÃ¡p suy luáº­n áº©n cá»§a dÃ²ng mÃ´ hÃ¬nh Reasoning (Qwen-QwQ)
    text = re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL)
    text = text.strip().lower()
    text = re.sub(r"^(a|answer|output):\s*", "", text)

    if "|" in text:
        parts = text.rsplit("|", 1)
        answer = parts[0].strip()
        try:
            confidence = max(0.0, min(10.0, float(parts[1].strip())))
        except ValueError:
            confidence = 7.0
    else:
        answer = text
        words = text.split()
        confidence = 8.0 if 3 <= len(words) <= 10 else 6.0

    answer = re.sub(r"[^\x20-\x7e]", " ", answer)
    answer = re.sub(r"\s+", " ", answer).strip()

    words = answer.split()
    if len(words) > 15:
        answer = " ".join(words[:15])
        confidence = max(confidence - 1.0, 0.0)

    return answer, confidence

class DistillWorker:
    def __init__(self, config: WorkerConfig):
        self.cfg = config
        self._last_call = 0.0
        self._min_interval = 60.0 / max(config.rpm, 1)
        self._lock = threading.Lock()

    def _rate_limit(self):
        with self._lock:
            elapsed = time.time() - self._last_call
            if elapsed < self._min_interval:
                time.sleep(self._min_interval - elapsed)
            self._last_call = time.time()

    def generate(self, question: str, domain: str, retries: int = 3) -> Optional[QAPair]:
        if self.cfg.provider == "mock":
            words = question.split()
            concept = words[-1] if words else "thing"
            return QAPair(question=question, answer=f"{concept} is verified", confidence=9.0, domain=domain, model="mock")

        prompt = (
            f"Answer this question in 5-10 lowercase words, then add |confidence (0-10).\n"
            f"Format: answer words here|8.5\n"
            f"Q: {question}"
        )
        delay = 4.0

        for attempt in range(retries):
            self._rate_limit()
            try:
                raw = call_api(self.cfg.base_url, self.cfg.api_key, self.cfg.model, prompt)
                if raw is None:
                    continue

                answer, confidence = parse_response(raw, question)
                if not answer or len(answer.split()) < 2:
                    continue

                if confidence < self.cfg.min_confidence:
                    return None

                return QAPair(question=question, answer=answer, confidence=confidence, domain=domain, model=self.cfg.model)
            except HTTPError as e:
                if e.code == 429:
                    log.warning(f"{self.cfg.model}: rate limit, backing off {delay:.0f}s")
                    time.sleep(delay)
                    delay *= 2
        return None

class Checkpoint:
    def __init__(self, path: str):
        self.path = Path(path)
        self.tmp_path = self.path.with_suffix(".tmp")
        self.completed: set = set()
        self.total_generated = 0
        self.total_discarded = 0

    def load(self):
        if self.path.exists():
            try:
                data = json.loads(self.path.read_text())
                self.completed = set(data.get("completed", []))
                self.total_generated = data.get("total_generated", 0)
                self.total_discarded = data.get("total_discarded", 0)
                log.info(f"Resumed state: {self.total_generated} pairs loaded.")
            except Exception as e:
                log.warning(f"Checkpoint read error: {e} â€” starting fresh")

    def save(self):
        data = {
            "completed": list(self.completed),
            "total_generated": self.total_generated,
            "total_discarded": self.total_discarded,
        }
        self.tmp_path.write_text(json.dumps(data, indent=2))
        self.tmp_path.replace(self.path)

def build_workers(args) -> list[DistillWorker]:
    workers = []
    provider_order = [args.teacher] if args.teacher and args.teacher != "auto" else ["groq", "gemini", "openrouter"]
    available = []

    for provider in provider_order:
        pconf = PROVIDERS[provider]
        key = os.environ.get(pconf["env_key"], "")
        if not key:
            continue
        for model in pconf["models"]:
            available.append((provider, model, key, pconf))

    if not available:
        log.error("No active API keys found in Environment Variables.")
        sys.exit(1)

    domain_names = list(DOMAINS.keys())
    for i in range(min(args.workers, len(available))):
        provider, model, key, pconf = available[i % len(available)]
        assigned_domains = domain_names[i::args.workers] or domain_names
        
        if args.requests_per_minute > 0:
            rpm_limit = args.requests_per_minute
        else:
            rpm_limit = pconf["rpm"]

        cfg = WorkerConfig(
            provider=provider, model=model, api_key=key,
            base_url=pconf["base_url"], rpm=rpm_limit,
            domains=assigned_domains, min_confidence=args.min_confidence
        )
        workers.append(DistillWorker(cfg))
        log.info(f"  Worker {i}: {provider}/{model} â†’ RPM: {rpm_limit} | domains: {assigned_domains}")

    return workers

def worker_task(worker: DistillWorker, results: list, checkpoint: Checkpoint, 
                lock: threading.Lock, output_path: str, target: int):
    discard_streak = 0
    while True:
        try:
            with lock:
                if checkpoint.total_generated >= target:
                    break

            domain = random.choice(worker.cfg.domains)
            concepts = DOMAINS[domain]
            concept = random.choice(concepts)
            template_idx = random.randint(0, len(TEMPLATES) - 1)
            question = TEMPLATES[template_idx].format(concept=concept)

            pair = worker.generate(question, domain)

            with lock:
                if pair is not None:
                    with open(output_path, "a", encoding="utf-8", newline="\n") as f:
                        f.write(f"# conf={pair.confidence:.2f} model={pair.model}\n{pair.question}\n{pair.answer}\n")
                    results.append(pair)
                    checkpoint.total_generated += 1
                    discard_streak = 0
                    
                    # Log tiáº¿n Ä‘á»™ chi tiáº¿t má»—i khi cÃ o thÃªm Ä‘Æ°á»£c 5 cÃ¢u thÃ nh cÃ´ng
                    if checkpoint.total_generated % 5 == 0:
                        log.info(f"[PROGRESS] {checkpoint.total_generated}/{target} pairs generated | {checkpoint.total_discarded} discarded")
                    
                    if checkpoint.total_generated % 50 == 0:
                        checkpoint.save()
                else:
                    checkpoint.total_discarded += 1
                    discard_streak += 1
                    
                    # BÃ¡o Ä‘á»™ng náº¿u má»™t Worker bá»‹ tá»« chá»‘i liÃªn tiáº¿p quÃ¡ nhiá»u láº§n (vÃ­ dá»¥: lá»—i Key)
                    if discard_streak % 20 == 0:
                        log.warning(f"[{worker.cfg.model}] Warning: {discard_streak} consecutive discards. Check your API limits.")
                        
        except Exception as e:
            log.error(f"Unexpected error in thread {threading.current_thread().name}: {e}")
            time.sleep(1)

def main():
    parser = argparse.ArgumentParser(description="Distill Q&A corpus from teacher models")
    parser.add_argument("--teacher", choices=["gemini","groq","openrouter","mock","auto"], default="auto")
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--target", type=int, default=10000)
    parser.add_argument("--min-confidence", type=float, default=3.0)
    parser.add_argument("--output", default="training/qa_distilled.txt")
    parser.add_argument("--checkpoint", default="training/distill_checkpoint.json")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--requests-per-minute", type=int, default=0)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if args.verbose:
        log.setLevel(logging.DEBUG)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    checkpoint = Checkpoint(args.checkpoint)
    if args.resume:
        checkpoint.load()
    elif output_path.exists():
        output_path.unlink()

    log.info("=== EH-G3 Dataset Distillation ===")
    workers = build_workers(args)
    
    results = []
    lock = threading.Lock()
    threads = []
    t_start = time.time()

    for i, worker in enumerate(workers):
        t = threading.Thread(
            target=worker_task,
            args=(worker, results, checkpoint, lock, str(output_path), args.target),
            name=f"worker-{i}",
            daemon=True
        )
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    checkpoint.save()
    elapsed = time.time() - t_start
    log.info(f"=== Complete === Generated: {checkpoint.total_generated} pairs in {elapsed:.1f}s")

if __name__ == "__main__":
    main()