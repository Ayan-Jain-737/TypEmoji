# Contributing to EmojiTyper

Thank you for your interest in contributing to EmojiTyper! We welcome contributions from everyone.

## Development Setup

### Dependencies
- **C++23** capable compiler (GCC 12+ or Clang 15+)
- **CMake 3.25+** and **Ninja**
- **Qt 6.5+** (Core, Gui, Widgets, Network, Concurrent, DBus)
- **KDE Frameworks 6** (KF6GlobalAccel, KF6XmlGui)
- **X11 / XTest** (`libx11-dev`, `libxtst-dev`)
- **Runtime helpers**: `wl-clipboard`, `wtype` (Wayland), `xclip` (X11)

### Building
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build --parallel
```

### Running Tests
```bash
ctest --test-dir build --output-on-failure
```

## Pull Request Guidelines
1. Ensure all unit tests pass before submitting.
2. Follow modern C++23 standards (use RAII, smart pointers, `std::string_view` where appropriate).
3. Keep code clean and self-documenting; avoid committing debug logs to stdout/stderr.
4. If modifying UI components, verify both Dark and Light desktop themes.
