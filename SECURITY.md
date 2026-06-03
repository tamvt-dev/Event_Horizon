# Security Policy

## 🔒 Supported Versions

We actively support the following versions with security updates:

| Version | Supported          | Notes                           |
| ------- | ------------------ | ------------------------------- |
| 1.0.x   | :white_check_mark: | Current stable release          |
| 0.9.x   | :white_check_mark: | Legacy support until Q2 2025    |
| < 0.9   | :x:                | No longer supported             |

## 🛡️ Security Considerations

### Memory Safety

EventHorizon is designed with memory safety in mind:

- **Arena Allocator**: All allocations go through arena, preventing fragmentation and leaks
- **Bounds Checking**: All array accesses validated before use
- **No `malloc/free`**: Per-step allocations eliminated, preventing use-after-free
- **Alignment Enforcement**: AVX2 operations require 32-byte alignment (compile-time checks)

### Input Validation

**DAG File Loading (`eh_hgn_dag_load`):**
- Magic number verification (`0x48474E44`)
- Version compatibility check
- Size overflow detection
- Alignment validation
- CSR integrity verification

**Inference Session (`eh_hgn_session_init`):**
- Null pointer checks on all inputs
- Prompt length validation
- Arena capacity verification
- Config bounds checking

### Known Limitations

1. **Untrusted DAG Files**
   - **Risk**: Malformed binary files could cause crashes
   - **Mitigation**: Always validate DAG files from untrusted sources
   - **Best Practice**: Sign and verify DAG files in production

2. **Stack Overflow**
   - **Risk**: Deep recursion in graph traversal (currently none)
   - **Mitigation**: Iterative algorithms, no recursion used
   - **Status**: Not applicable to current implementation

3. **Integer Overflow**
   - **Risk**: Large vocab_size or edge counts
   - **Mitigation**: Compile-time limits enforced
   - **Limits**: 
     - `EH_HGN_VOCAB_SIZE = 32768` (2^15)
     - `EH_HGN_MAX_EDGES = 1048576` (2^20)

4. **Side-Channel Attacks**
   - **Risk**: Timing attacks on beam search scoring
   - **Mitigation**: Not currently implemented (research use only)
   - **Production Note**: Use constant-time operations if deploying in adversarial environments

## 🚨 Reporting a Vulnerability

We take security seriously. If you discover a security vulnerability:

### For Non-Sensitive Issues

Open a GitHub issue:
1. Go to: https://github.com/YOUR_USERNAME/EventHorizon/issues
2. Click "New issue"
3. Add label: "security"
4. Describe the vulnerability

### For Sensitive Issues

**Email:** YOUR_EMAIL@example.com

**Subject:** `[SECURITY] Brief Description`

**Include:**
- Description of the vulnerability
- Steps to reproduce
- Affected versions
- Potential impact
- Suggested fix (if available)

### What to Expect

- **Response**: I'll respond as soon as possible (usually within a few days)
- **Fix Timeline**: 
  - Critical: As fast as possible
  - High: Within 2 weeks
  - Medium: Within a month
  - Low: Next release
- **Credit**: You'll be credited in SECURITY.md and release notes (unless you prefer anonymity)

## 🔍 Security Checklist for Contributors

Before submitting PRs, verify:

### Memory Safety
- [ ] No raw `malloc/free` (use arena)
- [ ] All array accesses bounds-checked
- [ ] No pointer arithmetic without validation
- [ ] All allocations checked for NULL

### Input Validation
- [ ] All public APIs validate inputs
- [ ] File I/O checks return codes
- [ ] Size calculations checked for overflow
- [ ] User-provided indices validated

### Resource Management
- [ ] All resources cleaned up on error paths
- [ ] No resource leaks in error conditions
- [ ] File handles closed properly
- [ ] Memory released on shutdown

### Testing
- [ ] Boundary condition tests
- [ ] NULL pointer tests
- [ ] Integer overflow tests

## 🛠️ Security Testing

### Static Analysis

Recommended tools:
- **Clang Static Analyzer**: `scan-build make`
- **Cppcheck**: `cppcheck --enable=all src/`
- **Valgrind**: `valgrind --leak-check=full ./tests/test_*`

### Sanitizers

Build with sanitizers for development:

```bash
# Address Sanitizer (memory errors)
gcc -fsanitize=address -g -O1 ...

# Undefined Behavior Sanitizer
gcc -fsanitize=undefined -g -O1 ...
```

## 📋 Security Best Practices

### For Library Users

1. **Validate DAG Files**
   ```c
   // Always check return codes
   if (eh_hgn_dag_load(arena, path, &dag) != EH_HGN_OK) {
       fprintf(stderr, "DAG load failed - untrusted file?\n");
       return -1;
   }
   ```

2. **Limit Arena Size**
   ```c
   // Cap arena to prevent memory exhaustion
   size_t max_arena = 512 * 1024 * 1024;  // 512MB
   EH_Arena *arena = eh_arena_create(max_arena);
   ```

3. **Validate Prompts**
   ```c
   // Check prompt length
   if (prompt_len > EH_BEAM_MAX_LEN) {
       fprintf(stderr, "Prompt too long\n");
       return -1;
   }
   ```

4. **Set Max Steps**
   ```c
   // Prevent infinite loops
   EH_HGN_EngineConfig config = eh_hgn_default_config();
   config.max_steps = 100;  // Reasonable limit
   ```

### For Embedded Deployments

1. **Watchdog Timer**: Set timeout for inference
2. **Stack Limits**: Monitor stack usage (< 8KB per session)
3. **Heap Limits**: Use fixed-size arena (no dynamic growth)
4. **Input Sanitization**: Validate all external inputs

### For Production Deployments

1. **DAG Signing**: Use cryptographic signatures for DAG files
2. **Sandboxing**: Run inference in isolated process
3. **Rate Limiting**: Limit inference requests per client
4. **Monitoring**: Track memory usage, inference time, errors

## 🔐 Cryptographic Considerations

**Current Status**: EventHorizon does not use cryptography internally.

**If Adding Crypto:**
- Use well-established libraries (OpenSSL, libsodium)
- Never implement custom crypto
- Follow OWASP guidelines
- Use constant-time operations

## 📚 Security References

### Standards
- [CWE Top 25](https://cwe.mitre.org/top25/)
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)

### Tools
- [Valgrind](https://valgrind.org/)
- [AddressSanitizer](https://github.com/google/sanitizers)
- [Clang Static Analyzer](https://clang-analyzer.llvm.org/)

##  Security Updates

Security updates are released as:
- **Patch versions** for critical fixes (e.g., 1.0.1 → 1.0.2)
- **GitHub Releases** with security notes
- **CHANGELOG.md** for detailed information

Subscribe to releases to get notifications:
- Watch repository → Custom → Releases

## ⚖️ Vulnerability Disclosure Examples

### Example 1: Buffer Overflow (Hypothetical)

**Title**: Buffer overflow in `eh_hgn_dag_load` edge parsing

**Severity**: High (CVSS 7.5)

**Description**: 
Malformed DAG file with `total_edges` exceeding actual data causes read beyond buffer boundary.

**Affected Versions**: 0.9.0 - 1.0.0

### Example 2: Integer Overflow (Hypothetical)

**Title**: Integer overflow in arena size calculation

**Severity**: Medium (CVSS 5.3)

**Description**:
Large `vocab_size * embed_dim` calculation overflows 32-bit integer.

**Affected Versions**: < 0.9.5

**Fix**: Use 64-bit arithmetic for size calculations (commit def456)

---

**Last Updated**: 2026

For questions about this policy, open a GitHub issue or email YOUR_EMAIL@example.com
