#!/bin/bash
# EventHorizon Engine - Release Creation Script
# Creates release packages with checksums

set -e  # Exit on error

VERSION=$(cat VERSION)
PROJECT_NAME="eventhorizon"
RELEASE_DIR="release-v${VERSION}"
DATE=$(date +%Y-%m-%d)

echo "╔══════════════════════════════════════════════════╗"
echo "║   EventHorizon Engine Release Builder            ║"
echo "║   Version: v${VERSION}                           "
echo "║   Date: ${DATE}                                  "
echo "╚══════════════════════════════════════════════════╝"
echo ""

# Clean previous release
if [ -d "$RELEASE_DIR" ]; then
    echo "[1/8] Cleaning previous release..."
    rm -rf "$RELEASE_DIR"
fi

# Create release directory
echo "[2/8] Creating release directory..."
mkdir -p "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/src"
mkdir -p "$RELEASE_DIR/bin"

# Build all targets
echo "[3/8] Building release binaries..."
make clean
make build-all OPTFLAGS="-O3 -march=native -flto"

# Copy binaries
echo "[4/8] Copying binaries..."
cp eh_engine_ultimate_bench "$RELEASE_DIR/bin/" 2>/dev/null || true
cp eh_neuro_test "$RELEASE_DIR/bin/" 2>/dev/null || true
cp eh_test "$RELEASE_DIR/bin/" 2>/dev/null || true
cp examples/hello_world "$RELEASE_DIR/bin/" 2>/dev/null || true
cp examples/arena_demo "$RELEASE_DIR/bin/" 2>/dev/null || true
cp examples/graph_demo "$RELEASE_DIR/bin/" 2>/dev/null || true
cp examples/game_ai_npc "$RELEASE_DIR/bin/" 2>/dev/null || true

# Create source archive
echo "[5/8] Creating source archive..."
tar czf "$RELEASE_DIR/${PROJECT_NAME}-v${VERSION}-src.tar.gz" \
    --exclude=".git" \
    --exclude="release-*" \
    --exclude="*.o" \
    --exclude="eh_*" \
    --exclude="examples/hello_world" \
    --exclude="examples/arena_demo" \
    --exclude="examples/graph_demo" \
    --exclude="examples/game_ai_npc" \
    --exclude="tests/test_*" \
    --transform "s,^,${PROJECT_NAME}-v${VERSION}/," \
    .

# Create binary archive
echo "[6/8] Creating binary archive..."
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

cd "$RELEASE_DIR"
tar czf "${PROJECT_NAME}-v${VERSION}-${OS}-${ARCH}.tar.gz" bin/

# Generate checksums
echo "[7/8] Generating checksums..."
sha256sum *.tar.gz > "${PROJECT_NAME}-v${VERSION}-checksums.txt"

cd ..

# Create release manifest
echo "[8/8] Creating release manifest..."
cat > "$RELEASE_DIR/MANIFEST.txt" << EOF
EventHorizon Engine v${VERSION}
Release Date: ${DATE}
Architecture: ${OS}-${ARCH}

Files:
------
$(ls -lh "$RELEASE_DIR"/*.tar.gz | awk '{print $9, "-", $5}')

Checksums:
----------
$(cat "$RELEASE_DIR/${PROJECT_NAME}-v${VERSION}-checksums.txt")

Installation:
-------------
1. Extract source archive:
   tar xzf ${PROJECT_NAME}-v${VERSION}-src.tar.gz
   cd ${PROJECT_NAME}-v${VERSION}

2. Build from source:
   make build-all

3. Or use pre-built binaries:
   tar xzf ${PROJECT_NAME}-v${VERSION}-${OS}-${ARCH}.tar.gz
   ./bin/eh_engine_ultimate_bench

Documentation:
--------------
See README.md for comprehensive documentation
See QUICKSTART.md for 30-second tutorial
See PERFORMANCE_TUNING.md for optimization guide

License:
--------
Apache-2.0 (see LICENSE file)

Contact:
--------
GitHub: https://github.com/[username]/eventhorizon
Issues: https://github.com/[username]/eventhorizon/issues
EOF

echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║   Release v${VERSION} Created Successfully!      "
echo "╚══════════════════════════════════════════════════╝"
echo ""
echo "Release directory: $RELEASE_DIR"
echo ""
echo "Files created:"
ls -lh "$RELEASE_DIR"
echo ""
echo "Next steps:"
echo "  1. Test the release:"
echo "     cd $RELEASE_DIR"
echo "     tar xzf ${PROJECT_NAME}-v${VERSION}-src.tar.gz"
echo "     cd ${PROJECT_NAME}-v${VERSION}"
echo "     make build-all"
echo "     ./eh_engine_ultimate_bench"
echo ""
echo "  2. Create GitHub release:"
echo "     - Tag: v${VERSION}"
echo "     - Title: EventHorizon Engine v${VERSION}"
echo "     - Upload files from $RELEASE_DIR/"
echo "     - Use RELEASE_NOTES.md as description"
echo ""
echo "  3. Announce on:"
echo "     - GitHub Discussions"
echo "     - Project README"
echo "     - Community channels"
echo ""
