#!/bin/bash
# Build kaentake on macOS/Linux via a Windows VM (UTM).
#
# Prerequisites (one-time setup):
#   1. Install UTM: brew install --cask utm
#   2. Create a Windows 11 ARM64 VM named "Windows"
#   3. Install in the VM:
#      - VS Build Tools: vs_buildtools.exe --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
#      - Python (ARM64 embeddable): extract to C:\pyarm, add to PATH
#      - CMake: cmake-*-windows-arm64.msi with ADD_CMAKE_TO_PATH=System
#      - Git: Git-*-64-bit.exe /VERYSILENT
#   4. Clone the repo: git clone --recursive https://github.com/iw2d/kaentake.git C:\kaentake
#
# Usage:
#   ./build.sh [debug|release]
#
set -e

CONFIG="${1:-release}"
VM_NAME="Windows"

run() { utmctl exec "$VM_NAME" --cmd "$@" 2>&1; }
pull() { utmctl file pull "$VM_NAME" "$1" 2>&1; }
push() { utmctl file push "$VM_NAME" "$1"; }

# Verify VM is running
STATUS=$(utmctl status "$VM_NAME" 2>&1)
if ! echo "$STATUS" | grep -q "started"; then
    echo "Starting VM..."
    utmctl start "$VM_NAME"
    sleep 10
fi

# Push build script
cat <<'BAT' | push "C:\\kaentake\\build_remote.bat"
@echo off
set "PATH=C:\pyarm;C:\Program Files\CMake\bin;C:\Program Files\Git\bin;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" arm64_x86
set "PATH=C:\pyarm;%PATH%"
cd C:\kaentake
git pull --recurse-submodules
cmake --preset %1-win32
cmake --build --preset %1-win32
echo === DONE === > C:\build_status.txt
BAT

# Sync local changes to VM
DIFF=$(git diff HEAD --stat)
if [ -n "$DIFF" ]; then
    echo "Syncing local changes..."
    git diff HEAD > /tmp/kaentake.patch
    cat /tmp/kaentake.patch | push "C:\\kaentake\\local.patch"
    run cmd /c "cd C:\kaentake && git checkout -- . && git apply local.patch"
fi

# Build
echo "Building ($CONFIG)..."
run cmd /c "C:\kaentake\build_remote.bat $CONFIG > C:\build_log.txt 2>&1"

# Wait for completion
for i in $(seq 1 60); do
    sleep 5
    RESULT=$(pull "C:\\build_status.txt" 2>/dev/null)
    if echo "$RESULT" | grep -q "DONE"; then break; fi
    [ $((i % 6)) -eq 0 ] && echo "  building... ($((i*5))s)"
done

# Check for errors
LOG=$(pull "C:\\build_log.txt")
if echo "$LOG" | grep -q "Build FAILED"; then
    echo "Build failed:"
    echo "$LOG" | grep -E "error |FAILED" | head -10
    exit 1
fi

# Pull artifacts
CONFIG_DIR=$(echo "$CONFIG" | sed 's/./\U&/')  # release -> Release
mkdir -p build/Release
pull "C:\\kaentake\\build\\${CONFIG_DIR}\\Kaentake.dll" > build/Release/Kaentake.dll
pull "C:\\kaentake\\build\\${CONFIG_DIR}\\Kaentake.exe" > build/Release/Kaentake.exe
pull "C:\\kaentake\\build\\${CONFIG_DIR}\\Custom.wz" > build/Release/Custom.wz

# Cleanup
run cmd /c "del C:\build_status.txt C:\build_log.txt 2>nul"

echo ""
echo "Build complete:"
ls -la build/Release/*.dll build/Release/*.exe build/Release/*.wz
