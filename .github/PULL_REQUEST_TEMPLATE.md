## Summary of Changes

<!-- Provide a clear and concise description of the changes introduced in this PR. Explain what problem it solves and how. -->

## Related Issue

<!-- Link the relevant issue(s) resolved or impacted by this PR -->
Fixes #

## Type of Change

- [ ] 🐛 Bug fix (non-breaking change fixing an issue)
- [ ] ✨ New feature (non-breaking change adding functionality)
- [ ] 💥 Breaking change (fix or feature causing existing functionality to break)
- [ ] ♻️ Code refactoring / cleanups
- [ ] 📝 Documentation updates
- [ ] 📦 Build system / Packaging / CI improvements

## Testing Done

Please document the testing performed to verify these changes:

### Display Server & Compositor
- [ ] **Wayland** (Compositor: GNOME Mutter / KDE KWin / Sway / Hyprland / other: `______`)
- [ ] **X11** (Window Manager / Desktop: `______`)

### Theming & Appearance
- [ ] **Dark Theme** (verified panel styling, contrast, and font readability)
- [ ] **Light Theme** (verified panel styling, contrast, and font readability)

### Verification Commands Run
- [ ] `cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel` (Clean build with zero warnings/errors)
- [ ] `cmake -B build-test-nokde -S . -G Ninja -DENABLE_KDE=OFF && cmake --build build-test-nokde --parallel` (Non-KDE build verification)
- [ ] Single-instance IPC behavior verified (`EmojiTyper --show`, `--toggle`, `--quit`)

## Pull Request Checklist

- [ ] My code adheres to the project's coding standards and modern C++23 best practices.
- [ ] I have not committed any credentials, tokens, secrets, or temporary debug files.
- [ ] I have updated corresponding documentation (README, man pages, docs/, or inline docs) where appropriate.
- [ ] All changes compile cleanly without introducing new compiler warnings.
- [ ] Packaging specifications and manifests remain valid and functional.
