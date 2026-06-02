# EventHorizon Engine - Release Creation Script (PowerShell)
# Creates release packages with checksums

$ErrorActionPreference = "Stop"

$VERSION = Get-Content VERSION -Raw
$VERSION = $VERSION.Trim()
$PROJECT_NAME = "eventhorizon"
$RELEASE_DIR = "release-v$VERSION"
$DATE = Get-Date -Format "yyyy-MM-dd"

Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   EventHorizon Engine Release Builder            ║" -ForegroundColor Cyan
Write-Host "║   Version: v$VERSION" -ForegroundColor Cyan
Write-Host "║   Date: $DATE" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Clean previous release
if (Test-Path $RELEASE_DIR) {
    Write-Host "[1/8] Cleaning previous release..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $RELEASE_DIR
}

# Create release directory
Write-Host "[2/8] Creating release directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path "$RELEASE_DIR" -Force | Out-Null
New-Item -ItemType Directory -Path "$RELEASE_DIR\src" -Force | Out-Null
New-Item -ItemType Directory -Path "$RELEASE_DIR\bin" -Force | Out-Null

# Build all targets (WSL)
Write-Host "[3/8] Building release binaries..." -ForegroundColor Yellow
wsl bash -c "make clean && make build-all OPTFLAGS='-O3 -march=native -flto'"

# Copy binaries
Write-Host "[4/8] Copying binaries..." -ForegroundColor Yellow
$binaries = @(
    "eh_engine_ultimate_bench",
    "eh_neuro_test", 
    "eh_test",
    "examples/hello_world",
    "examples/arena_demo",
    "examples/graph_demo",
    "examples/game_ai_npc"
)

foreach ($binary in $binaries) {
    if (Test-Path $binary) {
        Copy-Item $binary "$RELEASE_DIR\bin\" -ErrorAction SilentlyContinue
    }
}

# Create source archive (using WSL tar)
Write-Host "[5/8] Creating source archive..." -ForegroundColor Yellow
wsl bash -c @"
tar czf '$RELEASE_DIR/${PROJECT_NAME}-v${VERSION}-src.tar.gz' \
    --exclude='.git' \
    --exclude='release-*' \
    --exclude='*.o' \
    --exclude='eh_*' \
    --exclude='examples/hello_world' \
    --exclude='examples/arena_demo' \
    --exclude='examples/graph_demo' \
    --exclude='examples/game_ai_npc' \
    --exclude='tests/test_*' \
    --transform 's,^,${PROJECT_NAME}-v${VERSION}/,' \
    .
"@

# Create binary archive
Write-Host "[6/8] Creating binary archive..." -ForegroundColor Yellow
$ARCH = "x64"
$OS = "windows"

Push-Location $RELEASE_DIR
wsl bash -c "tar czf '${PROJECT_NAME}-v${VERSION}-${OS}-${ARCH}.tar.gz' bin/"
Pop-Location

# Generate checksums
Write-Host "[7/8] Generating checksums..." -ForegroundColor Yellow
Push-Location $RELEASE_DIR
Get-ChildItem *.tar.gz | ForEach-Object {
    $hash = (Get-FileHash $_.Name -Algorithm SHA256).Hash.ToLower()
    "$hash  $($_.Name)" | Out-File -Append "${PROJECT_NAME}-v${VERSION}-checksums.txt" -Encoding ASCII
}
Pop-Location

# Create release manifest
Write-Host "[8/8] Creating release manifest..." -ForegroundColor Yellow
$archives = Get-ChildItem "$RELEASE_DIR\*.tar.gz" | ForEach-Object { 
    "$($_.Name) - $([math]::Round($_.Length / 1MB, 2)) MB" 
}
$checksums = Get-Content "$RELEASE_DIR\${PROJECT_NAME}-v${VERSION}-checksums.txt"

$manifest = @"
EventHorizon Engine v$VERSION
Release Date: $DATE
Architecture: ${OS}-${ARCH}

Files:
------
$($archives -join "`n")

Checksums:
----------
$($checksums -join "`n")

Installation:
-------------
1. Extract source archive:
   tar xzf ${PROJECT_NAME}-v${VERSION}-src.tar.gz
   cd ${PROJECT_NAME}-v${VERSION}

2. Build from source (WSL):
   wsl bash -c "make build-all"

3. Or use pre-built binaries:
   tar xzf ${PROJECT_NAME}-v${VERSION}-${OS}-${ARCH}.tar.gz
   .\bin\eh_engine_ultimate_bench

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
"@

$manifest | Out-File "$RELEASE_DIR\MANIFEST.txt" -Encoding UTF8

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   Release v$VERSION Created Successfully!" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "Release directory: $RELEASE_DIR" -ForegroundColor Cyan
Write-Host ""
Write-Host "Files created:" -ForegroundColor Cyan
Get-ChildItem $RELEASE_DIR | Format-Table Name, Length, LastWriteTime
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test the release:" -ForegroundColor White
Write-Host "     cd $RELEASE_DIR"
Write-Host "     tar xzf ${PROJECT_NAME}-v${VERSION}-src.tar.gz"
Write-Host "     cd ${PROJECT_NAME}-v${VERSION}"
Write-Host "     wsl bash -c 'make build-all'"
Write-Host "     wsl bash -c './eh_engine_ultimate_bench'"
Write-Host ""
Write-Host "  2. Create GitHub release:" -ForegroundColor White
Write-Host "     - Tag: v$VERSION"
Write-Host "     - Title: EventHorizon Engine v$VERSION"
Write-Host "     - Upload files from $RELEASE_DIR/"
Write-Host "     - Use RELEASE_NOTES.md as description"
Write-Host ""
Write-Host "  3. Announce on:" -ForegroundColor White
Write-Host "     - GitHub Discussions"
Write-Host "     - Project README"
Write-Host "     - Community channels"
Write-Host ""
