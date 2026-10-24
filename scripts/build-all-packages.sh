#!/usr/bin/env bash
# ==============================================================================
# TypEmoji Multi-Distro Package Builder
# Generates .deb, .run installer, .tar.gz, and Arch packages into dist/
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DIST_DIR="$PROJECT_ROOT/dist"

echo "▶ 1. Cleaning and preparing directories..."
mkdir -p "$BUILD_DIR" "$DIST_DIR"
rm -f "$BUILD_DIR"/*.deb "$BUILD_DIR"/*.tar.gz "$BUILD_DIR"/*.run
rm -f "$DIST_DIR"/*

echo "▶ 2. Building TypEmoji (Release)..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "▶ 3. Generating Debian/Ubuntu .deb and Portable .tar.gz packages..."
cd "$BUILD_DIR"
cpack -G "DEB;TGZ"

echo "▶ 4. Generating Universal Self-Resolving .run Installer..."
cd "$PROJECT_ROOT"
chmod +x "$PROJECT_ROOT/scripts/create-installer.sh"
"$PROJECT_ROOT/scripts/create-installer.sh"

echo "▶ 5. Packaging into dist/ folder..."
find "$BUILD_DIR" -maxdepth 1 -name "*.deb" -exec cp {} "$DIST_DIR/TypEmoji-1.0.0-Ubuntu-Debian-x86_64.deb" \;
find "$BUILD_DIR" -maxdepth 1 -name "*.run" -exec cp {} "$DIST_DIR/TypEmoji-1.0.0-Universal-Installer.run" \;
find "$BUILD_DIR" -maxdepth 1 -name "*.tar.gz" -exec cp {} "$DIST_DIR/TypEmoji-1.0.0-Portable-Linux-x86_64.tar.gz" \;

# Arch package build if makepkg is available
if command -v makepkg &>/dev/null; then
    echo "▶ 6. Building Arch Linux .pkg.tar.zst package..."
    TMP_ARCH_DIR="$(mktemp -d /tmp/typemoji_arch_XXXXXX)"
    cp -r "$PROJECT_ROOT"/* "$TMP_ARCH_DIR/" || true
    cd "$TMP_ARCH_DIR/packaging"
    sed -i 's/source=(.*)/source=()/g' PKGBUILD
    sed -i 's/sha256sums=(.*)/sha256sums=()/g' PKGBUILD
    sed -i "s|\$pkgname-\$pkgver|$TMP_ARCH_DIR|g" PKGBUILD
    makepkg -f --nodeps --noconfirm 2>/dev/null || true
    find . -maxdepth 1 -name "*.pkg.tar.zst" ! -name "*-debug-*" -exec cp {} "$DIST_DIR/TypEmoji-1.0.0-Arch-Linux-x86_64.pkg.tar.zst" \;
    rm -rf "$TMP_ARCH_DIR"
fi

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  TypEmoji packages ready in $DIST_DIR:"
ls -lh "$DIST_DIR"
echo "══════════════════════════════════════════════════════════════"
