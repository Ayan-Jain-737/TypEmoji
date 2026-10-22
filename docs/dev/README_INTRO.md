# EmojiTyper

Direct emoji typing tool for Windows - types emojis directly into focused text fields without clipboard

## Quick Start

```powershell
git clone https://github.com/ayanjain737/EmojiTyper.git
cd EmojiTyper
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2022_64/lib/cmake"
cmake --build . --config Release
.\bin\EmojiTyper.exe
```

Press `Win + .` to open emoji panel

## Features

- Direct Unicode injection via SendInput + KEYEVENTF_UNICODE
- Non-activating floating panel (no focus theft)
- Full Unicode Emoji 17 support
- Recent/frequent emoji tracking
- Settings persistence (JSON)
- Single instance with system tray host
- Per-user install (no admin required)

## Project Structure

- `src/` - C++23 + Qt6 source code
- `tests/` - Unit tests
- `installer/` - Inno Setup script
- `assets/` - Emoji data and icons

## License

MIT License