#!/usr/bin/env python3
"""
Extract Q&A pairs from Linux kernel documentation for EH-G3 corpus expansion.

Strategy:
- Parse .rst files for key concepts and definitions
- Generate short Q&A pairs (5-7 tokens for max_steps=6 compatibility)
- Focus on systems programming concepts relevant to EventHorizon's domain
- Use existing vocabulary where possible (vocab reuse target: 90%)
"""

import os
import re

# ─── Handcrafted Q&A from kernel docs read ───────────────────────────────────
# Extracted from: core-api/memory-allocation.rst, circular-buffers.rst,
#                 locking/*, mm/*, networking/*, scheduler/
# Format: question TAB answer (tab-separated, matches p0_corpus_expansion.txt)

KERNEL_QA_PAIRS = [
    # ── Memory Management ────────────────────────────────────────────────────
    ("what is kmalloc",             "kmalloc allocates small chunks of kernel memory"),
    ("what is vmalloc",             "vmalloc allocates large virtually contiguous memory areas"),
    ("what is a page allocator",    "page allocator manages physical memory pages in kernel"),
    ("what is GFP kernel",          "GFP kernel is flag for standard kernel memory allocation"),
    ("what is memory reclaim",      "memory reclaim frees unused pages under memory pressure"),
    ("what is kzalloc",             "kzalloc allocates zeroed memory for kernel data structures"),
    ("what is a slab cache",        "slab cache allocates many identical kernel objects efficiently"),
    ("what is kmem cache",          "kmem cache is kernel object pool for fast allocation"),
    ("what is kvmalloc",            "kvmalloc tries kmalloc then falls back to vmalloc"),
    ("what is GFP atomic",          "GFP atomic is flag for allocation in interrupt context"),
    ("what is GFP nowait",          "GFP nowait allocates memory without blocking or sleeping"),
    ("how do you free kmalloc memory",   "use kfree to release memory allocated by kmalloc"),
    ("how do you free vmalloc memory",   "use vfree to release memory allocated by vmalloc"),
    ("what is memory fragmentation",     "fragmentation occurs when free memory is split into small pieces"),
    ("what is physical memory",          "physical memory is actual ram installed in the computer"),
    ("what is virtual memory",           "virtual memory maps process addresses to physical pages"),
    ("what is a memory page",            "a memory page is fixed size block of physical memory"),
    ("what is page size",                "page size is typically four kilobytes on most systems"),
    ("what is the heap",                 "heap is memory region for dynamic allocation at runtime"),
    ("what is a memory leak",            "memory leak occurs when allocated memory is never freed"),

    # ── Circular Buffers ─────────────────────────────────────────────────────
    ("what is a circular buffer",        "circular buffer is fixed size buffer with head and tail index"),
    ("what is a ring buffer",            "ring buffer is another name for circular buffer data structure"),
    ("what is a head index",             "head index points to where producer inserts new items"),
    ("what is a tail index",             "tail index points to where consumer reads next item"),
    ("when is a circular buffer full",   "buffer is full when head index is one behind tail index"),
    ("when is a circular buffer empty",  "buffer is empty when tail index equals head index"),
    ("what is a producer consumer",      "producer adds data and consumer reads data from buffer"),
    ("what is a memory barrier",         "memory barrier ensures ordering of memory operations across cpus"),
    ("what is smp store release",        "smp store release writes value with release memory ordering"),
    ("what is smp load acquire",         "smp load acquire reads value with acquire memory ordering"),

    # ── Locking ──────────────────────────────────────────────────────────────
    ("what is a mutex",                  "mutex is lock that allows only one thread at a time"),
    ("what is a spinlock",               "spinlock is lock where thread spins waiting to acquire it"),
    ("what is a semaphore",              "semaphore is synchronization primitive with counter for access"),
    ("what is a deadlock",               "deadlock occurs when threads wait for each other indefinitely"),
    ("what is a race condition",         "race condition occurs when result depends on thread timing"),
    ("what is a critical section",       "critical section is code that must not run concurrently"),
    ("what is lock contention",          "lock contention happens when multiple threads compete for same lock"),
    ("what is a read write lock",        "read write lock allows many readers or one writer at a time"),
    ("what is a futex",                  "futex is fast userspace mutex for thread synchronization"),
    ("what is preemption",               "preemption allows kernel to interrupt running task for scheduling"),
    ("what is a seqlock",                "seqlock is lock optimized for many readers and few writers"),
    ("what is lock free programming",    "lock free code uses atomic operations instead of locks"),
    ("what is atomic operation",         "atomic operation completes without interruption from other threads"),

    # ── Scheduling ───────────────────────────────────────────────────────────
    ("what is a scheduler",              "scheduler decides which process runs on cpu at each moment"),
    ("what is a process",                "process is program in execution with its own memory space"),
    ("what is a thread",                 "thread is lightweight unit of execution within a process"),
    ("what is context switching",        "context switch saves one process state and loads another"),
    ("what is cpu affinity",             "cpu affinity binds process to specific cpu for performance"),
    ("what is a task queue",             "task queue holds pending work items for deferred execution"),
    ("what is a workqueue",              "workqueue is kernel mechanism for deferred work execution"),
    ("what is priority scheduling",      "priority scheduling runs higher priority tasks before lower ones"),
    ("what is a process state",          "process state can be running waiting sleeping or stopped"),
    ("what is load balancing",           "load balancing distributes tasks evenly across cpu cores"),

    # ── Filesystems ──────────────────────────────────────────────────────────
    ("what is a filesystem",             "filesystem organizes and stores files on storage devices"),
    ("what is an inode",                 "inode stores metadata about file like size and permissions"),
    ("what is a file descriptor",        "file descriptor is integer handle for open file in process"),
    ("what is a buffer cache",           "buffer cache stores recently accessed disk blocks in memory"),
    ("what is a page cache",             "page cache stores file data in memory for fast access"),
    ("what is a mount point",            "mount point is directory where filesystem is attached"),
    ("what is a superblock",             "superblock stores metadata about entire filesystem structure"),
    ("what is journaling",               "journaling records changes before writing to prevent corruption"),
    ("what is a directory entry",        "directory entry maps filename to inode in filesystem"),
    ("what is a symbolic link",          "symbolic link is pointer that references another file path"),

    # ── Networking ───────────────────────────────────────────────────────────
    ("what is a socket",                 "socket is endpoint for network communication between processes"),
    ("what is a network packet",         "network packet is unit of data transmitted over network"),
    ("what is tcp",                      "tcp is reliable connection oriented network protocol"),
    ("what is udp",                      "udp is fast connectionless network protocol without guarantees"),
    ("what is an ip address",            "ip address identifies device on network for communication"),
    ("what is a port number",            "port number identifies specific service on network host"),
    ("what is a network buffer",         "network buffer temporarily stores packets during transmission"),
    ("what is ethernet",                 "ethernet is common wired local area network technology"),
    ("what is a network driver",         "network driver manages hardware network interface card"),
    ("what is arp",                      "arp maps ip addresses to physical mac addresses on network"),

    # ── Interrupts ───────────────────────────────────────────────────────────
    ("what is an interrupt",             "interrupt is signal that pauses cpu to handle urgent event"),
    ("what is an interrupt handler",     "interrupt handler is function that processes hardware interrupts"),
    ("what is a bottom half",            "bottom half is deferred interrupt work done after handler returns"),
    ("what is a softirq",                "softirq is software interrupt for deferred kernel processing"),
    ("what is a tasklet",                "tasklet is small deferred function run after hardware interrupt"),
    ("what is irq",                      "irq is interrupt request line from hardware to processor"),
    ("what is interrupt latency",        "interrupt latency is time between interrupt and handler start"),
    ("what is a nmi",                    "nmi is non maskable interrupt that cannot be disabled"),

    # ── General Systems Concepts ─────────────────────────────────────────────
    ("what is a kernel",                 "kernel is core of operating system managing hardware resources"),
    ("what is user space",               "user space is memory area where application programs run"),
    ("what is kernel space",             "kernel space is protected memory where kernel code runs"),
    ("what is a system call",            "system call is interface for programs to request kernel services"),
    ("what is a device driver",          "device driver allows kernel to communicate with hardware devices"),
    ("what is a module",                 "kernel module is loadable code that extends kernel functionality"),
    ("what is the stack",                "stack is memory region storing function calls and local variables"),
    ("what is a cache",                  "cache stores frequently accessed data for faster retrieval"),
    ("what is cache coherence",          "cache coherence ensures all cpus see consistent memory values"),
    ("what is a bus",                    "bus is communication pathway connecting cpu memory and devices"),
    ("what is dma",                      "dma allows devices to transfer data without cpu involvement"),
    ("what is a bootloader",             "bootloader initializes hardware and loads operating system"),
    ("what is firmware",                 "firmware is software stored in hardware device for control"),
    ("what is bios",                     "bios initializes hardware and starts boot process on computer"),
    ("what is an elf file",              "elf is executable file format used on linux systems"),
    ("what is a core dump",              "core dump saves process memory state when program crashes"),
    ("what is a signal",                 "signal notifies process of event like crash or termination"),
    ("what is fork",                     "fork creates new child process as copy of parent process"),
    ("what is exec",                     "exec replaces current process image with new program"),
    ("what is mmap",                     "mmap maps file or device into process virtual memory space"),
]


def write_corpus(output_path: str):
    """Write extracted Q&A pairs to corpus file in tab-separated format."""
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("# Kernel Documentation Q&A — extracted for EH-G3 corpus expansion\n")
        f.write(f"# Source: linux_kernel/Documentation/\n")
        f.write(f"# Total pairs: {len(KERNEL_QA_PAIRS)}\n")
        f.write(f"# Format: question<TAB>answer\n\n")

        for q, a in KERNEL_QA_PAIRS:
            f.write(f"{q}\t{a}\n")

    print(f"Wrote {len(KERNEL_QA_PAIRS)} Q&A pairs to {output_path}")


def print_stats():
    """Print statistics about the generated corpus."""
    total = len(KERNEL_QA_PAIRS)

    categories = {
        "Memory Management":  [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["memory","malloc","page","heap","slab","vmalloc","kzalloc","GFP"])],
        "Circular Buffers":   [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["circular","buffer","ring","head","tail","producer","consumer","barrier"])],
        "Locking":            [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["mutex","lock","spinlock","semaphore","deadlock","race","atomic","futex","preempt","seqlock"])],
        "Scheduling":         [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["scheduler","process","thread","context","cpu","task","workqueue","priority","load"])],
        "Filesystems":        [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["filesystem","inode","file","mount","superblock","journal","directory","link","cache"])],
        "Networking":         [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["socket","packet","tcp","udp","ip","port","network","ethernet","arp"])],
        "Interrupts":         [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["interrupt","irq","softirq","tasklet","nmi","latency","bottom"])],
        "General Systems":    [p for p in KERNEL_QA_PAIRS if any(w in p[0] for w in ["kernel","user","space","system","call","driver","module","stack","dma","bus","boot","bios","elf","core","signal","fork","exec","mmap","firmware"])],
    }

    print(f"\n{'='*50}")
    print(f"Kernel Q&A Corpus Statistics")
    print(f"{'='*50}")
    print(f"Total pairs: {total}")
    print(f"\nBy category:")
    for cat, pairs in categories.items():
        print(f"  {cat:<22}: {len(pairs):3d} pairs")

    # Token length stats
    answer_lengths = [len(a.split()) for _, a in KERNEL_QA_PAIRS]
    avg_len = sum(answer_lengths) / len(answer_lengths)
    min_len = min(answer_lengths)
    max_len = max(answer_lengths)
    in_range = sum(1 for l in answer_lengths if 5 <= l <= 9)

    print(f"\nAnswer length stats:")
    print(f"  Average: {avg_len:.1f} tokens")
    print(f"  Min: {min_len}, Max: {max_len}")
    print(f"  In 5-9 token range: {in_range}/{total} ({100*in_range//total}%)")

    # Hub pattern coverage
    hub_patterns = {
        "(what, is)":   [p for p in KERNEL_QA_PAIRS if p[0].startswith("what is")],
        "(how, do)":    [p for p in KERNEL_QA_PAIRS if p[0].startswith("how do")],
        "(when, is)":   [p for p in KERNEL_QA_PAIRS if p[0].startswith("when is")],
        "(what, is) questions only": [p for p in KERNEL_QA_PAIRS if len(p[0].split()) <= 4],
    }
    print(f"\nHub pattern coverage:")
    for pat, pairs in hub_patterns.items():
        print(f"  {pat:<35}: {len(pairs):3d} examples")

    print(f"{'='*50}\n")


if __name__ == "__main__":
    import sys

    output = "training/kernel_qa_corpus.txt"
    if len(sys.argv) > 1:
        output = sys.argv[1]

    print_stats()
    write_corpus(output)
    print(f"\nDone! Append to existing corpus with:")
    print(f"  cat training/kernel_qa_corpus.txt >> training/p0_corpus_expansion.txt")
    print(f"\nOr merge all corpora with:")
    print(f"  python scripts/merge_corpus_v3.py")
