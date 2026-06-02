# EventHorizon Engine v1.0.0 - Release Checklist

Use this checklist to ensure a smooth release process.

---

## 📋 Pre-Release Checklist

### Code Quality
- [x] All unit tests passing (12/12)
- [x] No compiler warnings
- [x] Memory leaks checked (Valgrind clean)
- [x] Code reviewed
- [x] Examples tested and working
- [x] Benchmark results verified

### Documentation
- [x] README.md updated
- [x] RELEASE_NOTES.md created
- [x] CHANGELOG.md updated
- [x] VERSION file updated (1.0.0)
- [x] API documentation complete
- [x] Examples documented
- [x] CONTRIBUTING.md present

### Build System
- [x] Makefile working on Linux
- [x] WSL build tested
- [x] Docker build working
- [x] CI/CD passing
- [x] ESP32 build config ready
- [x] Release scripts tested

### Legal & Licensing
- [x] LICENSE file present (Apache-2.0)
- [x] NOTICE file complete
- [x] Copyright headers in source files
- [x] Third-party attributions documented
- [x] No proprietary code included

---

## 🚀 Release Process

### 1. Version Bump
```bash
# Update VERSION file
echo "1.0.0" > VERSION

# Update version references in:
- README.md
- RELEASE_NOTES.md
- Makefile (if version-tagged)
- platformio.ini (ESP32)
```

### 2. Run Tests
```bash
# Unit tests
cd tests
make test

# Examples
cd examples
make
./hello_world
./arena_demo
./graph_demo
./game_ai_npc

# Benchmark
make bench
./eh_engine_ultimate_bench
```

### 3. Build Release Packages

#### Linux/WSL
```bash
chmod +x create_release.sh
./create_release.sh
```

#### Windows
```powershell
.\create_release.ps1
```

### 4. Verify Release
```bash
cd release-v1.0.0

# Check files exist
ls -lh

# Verify checksums
sha256sum -c eventhorizon-v1.0.0-checksums.txt

# Test source archive
tar xzf eventhorizon-v1.0.0-src.tar.gz
cd eventhorizon-v1.0.0
make build-all
./eh_engine_ultimate_bench

# Test binary archive
cd ../..
tar xzf eventhorizon-v1.0.0-linux-x64.tar.gz
./bin/eh_engine_ultimate_bench
```

### 5. Create Git Tag
```bash
git add .
git commit -m "Release v1.0.0"
git tag -a v1.0.0 -m "EventHorizon Engine v1.0.0 - Initial Release"
git push origin main
git push origin v1.0.0
```

### 6. Create GitHub Release
1. Go to GitHub → Releases → "Draft a new release"
2. Choose tag: `v1.0.0`
3. Release title: `EventHorizon Engine v1.0.0 - Initial Release`
4. Description: Copy from `GITHUB_RELEASE_TEMPLATE.md`
5. Upload files from `release-v1.0.0/`:
   - `eventhorizon-v1.0.0-src.tar.gz`
   - `eventhorizon-v1.0.0-linux-x64.tar.gz`
   - `eventhorizon-v1.0.0-macos-arm64.tar.gz` (if available)
   - `eventhorizon-v1.0.0-checksums.txt`
   - `MANIFEST.txt`
6. Check "Set as the latest release"
7. Publish release

### 7. Update Documentation
```bash
# Update README badges
[![Release](https://img.shields.io/github/v/release/[username]/eventhorizon)]()

# Update links to point to v1.0.0
- Download links
- Installation instructions
- Quick start guide
```

---

## 📢 Post-Release Checklist

### Announcements
- [ ] GitHub Discussions - Announcement post
- [ ] Project README - Add release banner
- [ ] Social media (Twitter/X, LinkedIn, Reddit)
- [ ] Relevant forums (Hacker News, edge AI communities)
- [ ] Email contributors/early testers

### Monitoring
- [ ] Watch GitHub Issues for bug reports
- [ ] Monitor Discussions for questions
- [ ] Track download statistics
- [ ] Collect user feedback

### Follow-up
- [ ] Respond to issues within 24-48 hours
- [ ] Update documentation based on feedback
- [ ] Plan patch release (v1.0.1) if needed
- [ ] Start Phase 1 development (SIMD, quantization)

---

## 🐛 Hotfix Process (if needed)

If critical bugs are found post-release:

### 1. Create Hotfix Branch
```bash
git checkout -b hotfix-v1.0.1 v1.0.0
```

### 2. Fix Bug
```bash
# Make fixes
git add .
git commit -m "Fix: [description]"
```

### 3. Test Thoroughly
```bash
make clean
make build-all
make test
```

### 4. Release Patch
```bash
# Update VERSION
echo "1.0.1" > VERSION

# Create release
./create_release.sh

# Tag and push
git tag -a v1.0.1 -m "EventHorizon Engine v1.0.1 - Hotfix"
git push origin hotfix-v1.0.1
git push origin v1.0.1

# Merge to main
git checkout main
git merge hotfix-v1.0.1
git push origin main
```

### 5. GitHub Release
- Create new release for v1.0.1
- Mark as patch release
- Document fixes in release notes

---

## 📊 Release Metrics to Track

### Download Stats
- Source archive downloads
- Binary archive downloads
- Docker image pulls

### Engagement
- GitHub stars
- Forks
- Issues opened
- Pull requests
- Discussions started

### Adoption
- Projects using EventHorizon
- Blog posts / articles
- Academic citations
- Production deployments

---

## 🎯 Success Criteria

**v1.0.0 Release is successful if:**
- [ ] 100+ GitHub stars in first month
- [ ] 10+ issues opened (shows engagement)
- [ ] 5+ external contributors
- [ ] 3+ production use cases reported
- [ ] Zero critical bugs in first week
- [ ] Documentation clarity rated >8/10

---

## 📝 Notes

### Lessons Learned
Document what went well and what could be improved for v1.1.0:
- ...
- ...

### Feedback Summary
Key feedback from early users:
- ...
- ...

### Next Release Planning
v1.1.0 target features:
- SIMD optimizations
- Multi-threading
- Performance improvements

---

## 🔗 Quick Links

- **Release Notes**: [RELEASE_NOTES.md](RELEASE_NOTES.md)
- **GitHub Release Template**: [GITHUB_RELEASE_TEMPLATE.md](GITHUB_RELEASE_TEMPLATE.md)
- **Roadmap**: [ROADMAP.md](ROADMAP.md)
- **Contributing**: [CONTRIBUTING.md](CONTRIBUTING.md)

---

**Last Updated:** June 2, 2026  
**Version:** 1.0.0  
**Status:** Ready for Release ✅
