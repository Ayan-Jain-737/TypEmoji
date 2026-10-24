#!/usr/bin/env bash
# ==============================================================================
# EmojiTyper Multi-Distro Package Builder
# Generates .deb, .run installer, .tar.gz, and Arch packages into dist/
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DIST_DIR="$PROJECT_ROOT/dist"

echo "▶ 1. Building EmojiTyper (Release)..."
mkdir -p "$BUILD_DIR" "$DIST_DIR"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "▶ 2. Generating Debian/Ubuntu .deb and Portable .tar.gz packages..."
cd "$BUILD_DIR"
cpack -G "DEB;TGZ"

echo "▶ 3. Generating Universal Self-Resolving .run Installer..."
cd "$PROJECT_ROOT"
chmod +x "$PROJECT_ROOT/scripts/create-installer.sh"
"$PROJECT_ROOT/scripts/create-installer.sh"

echo "▶ 4. Packaging into dist/ folder..."
cp "$BUILD_DIR"/EmojiTyper-*.deb "$DIST_DIR/EmojiTyper-1.0.0-Ubuntu-Debian-x86_64.deb"
cp "$BUILD_DIR"/EmojiTyper-*.run "$DIST_DIR/EmojiTyper-1.0.0-Universal-Installer.run"
cp "$BUILD_DIR"/EmojiTyper-*.tar.gz "$DIST_DIR/EmojiTyper-1.0.0-Portable-Linux-x86_64.tar.gz"

# Arch package build if makepkg is available
if command -v makepkg &>/dev/null; then
    echo "▶ 5. Building Arch Linux .pkg.tar.zst package..."
    TMP_ARCH_DIR="$(mktemp -d /tmp/emojityper_arch_XXXXXX)"
    cp -r "$PROJECT_ROOT"/* "$TMP_ARCH_DIR/" || true
    cd "$TMP_ARCH_DIR/packaging"
    sed -i 's/source=(.*)/source=()/g' PKGBUILD
    sed -i 's/sha256sums=(.*)/sha256sums=()/g' PKGBUILD
    sed -i "s|\$pkgname-\$pkgver|$TMP_ARCH_DIR|g" PKGBUILD
    cp emojityper-1.0.0-1-*.pkg.tar.zst "$DIST_DIR/EmojiTyper-1.0.0-Arch-Linux-x86_64.pkg.tar.zst" 2>/dev/null || true
    rm -rf "$TMP_ARCH_DIR"
fi

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Packages ready in $DIST_DIR:"
ls -lh "$DIST_DIR"
echo "══════════════════════════════════════════════════════════════"
