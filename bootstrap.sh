#!/usr/bin/env bash
# bootstrap.sh — one-time (but safe to re-run) project setup.
# Run this after a fresh clone, or after wiping build/ or vcpkg_installed/.
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCPKG_DIR="$PROJECT_ROOT/vcpkg"

echo "== 1. System packages required for GLFW to detect Wayland at build time =="
NEEDED_PKGS=(wayland wayland-protocols libxkbcommon extra-cmake-modules)
MISSING_PKGS=()
for pkg in "${NEEDED_PKGS[@]}"; do
    pacman -Qi "$pkg" &>/dev/null || MISSING_PKGS+=("$pkg")
done
FRESH_PKG_INSTALL=false
if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    echo "Installing missing packages: ${MISSING_PKGS[*]}"
    sudo pacman -S --needed "${MISSING_PKGS[@]}"
    FRESH_PKG_INSTALL=true
else
    echo "All required packages already installed."
fi

echo "== 2. Local vcpkg checkout =="
if [ ! -d "$VCPKG_DIR" ]; then
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
else
    echo "vcpkg already present at $VCPKG_DIR (skipping clone)."
fi

if [ ! -x "$VCPKG_DIR/vcpkg" ]; then
    "$VCPKG_DIR/bootstrap-vcpkg.sh"
else
    echo "vcpkg already bootstrapped."
fi

if [ "$FRESH_PKG_INSTALL" = true ]; then
    # vcpkg's binary cache is keyed by port/feature/compiler hash, which has no
    # idea whether a system dev package (like Wayland's) just appeared or
    # disappeared. Without this, a stale cached glfw3 build gets silently
    # restored instead of rebuilt, and the new packages never actually get used.
    echo "New system packages were installed — purging cached glfw3 build so it rebuilds against them."
    rm -rf "$HOME/.cache/vcpkg/archives"
    rm -rf "$VCPKG_DIR/buildtrees/glfw3" "$VCPKG_DIR/packages/glfw3_x64-linux"
    rm -rf "$PROJECT_ROOT/vcpkg_installed"
fi

echo "== 3. Configure + build (vcpkg manifest mode installs deps automatically) =="
cmake -B "$PROJECT_ROOT/build" -S "$PROJECT_ROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
cmake --build "$PROJECT_ROOT/build"

echo "== 4. Sanity-check that GLFW actually picked up Wayland =="
GLFW_LOG=$(find "$VCPKG_DIR/buildtrees/glfw3" -name "*.log" 2>/dev/null | head -n1 || true)
if [ -n "$GLFW_LOG" ] && grep -qi "wayland" "$GLFW_LOG"; then
    echo "GLFW Wayland support: looks OK (found references in build log)."
else
    echo "WARNING: couldn't confirm Wayland support in GLFW's build log."
    echo "Check manually: grep -i wayland $VCPKG_DIR/buildtrees/glfw3/*.log"
fi

echo
echo "Done. Next time, you only need:"
echo "  cmake --build $PROJECT_ROOT/build"