# Contributing to TypEmoji

Thank you for your interest in contributing to TypEmoji! We welcome contributions from developers, packagers, designers, and documentation writers of all experience levels.

This guide outlines our development setup, architecture, coding conventions, testing procedures, and submission workflow to ensure a smooth collaboration process.

---

## Table of Contents

- [Code of Conduct & Community Standards](#code-of-conduct--community-standards)
- [Issue Triage SLA](#issue-triage-sla)
- [Quickstart: Building and Running Locally](#quickstart-building-and-running-locally)
- [Distribution Dependency Table](#distribution-dependency-table)
- [Code Style & Conventions](#code-style--conventions)
- [Architecture Tour](#architecture-tour)
- [Manual Testing Guidelines](#manual-testing-guidelines)
- [Submitting a Pull Request](#submitting-a-pull-request)

---

## Code of Conduct & Community Standards

TypEmoji is an open and inclusive project. We hold all maintainers, contributors, and community members to the standards outlined in our [Code of Conduct](CODE_OF_CONDUCT.md) (Contributor Covenant v2.1).

In brief:
- **Demonstrate empathy and respect** in all interactions and technical discussions.
- **Provide constructive, actionable feedback** during code reviews and issue triage.
- **Focus on the code and technical merits**, avoiding personal or dismissive language.
- **Respect user privacy and system stability** — desktop utilities run with user session privileges; security and non-destructive behavior are paramount.

Instances of unacceptable behavior may be reported confidentially to the maintainers at `contact@typemoji.org`.

---

## Issue Triage SLA

Maintainers are actively committed to supporting the community and keeping development velocity high.

> [!IMPORTANT]
> **24-Hour Triage SLA**: We aim to review, triage, and provide an initial response to every newly opened GitHub Issue, Bug Report, and Feature Discussion within **24 hours**.

When opening an issue, please provide:
1. Your Linux distribution and desktop environment (e.g., *Fedora 40 GNOME 46*, *Arch Linux KDE Plasma 6.1*, *Ubuntu 24.04 Wayland*).
2. Display server session: Wayland or X11 (`echo $XDG_SESSION_TYPE`).
3. TypEmoji version (`EmojiTyper --version` or git commit SHA).
4. Relevant terminal or systemd logs (`journalctl --user -u emojityper.service -b` or `/tmp/emojityper.log`).

---

## Quickstart: Building and Running Locally

TypEmoji uses modern **C++23**, **Qt 6**, **CMake (>= 3.25)**, and the **Ninja** build generator.

### 1. Clone the Repository

```bash
git clone https://github.com/Ayan-Jain-737/TypEmoji.git
cd TypEmoji
```

### 2. Configure the Build

Configure a standard Debug build with Ninja:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

#### Build Configuration Flags:
- `-DCMAKE_BUILD_TYPE=Debug`: Includes debug symbols and asserts (recommended for development).
- `-DCMAKE_BUILD_TYPE=Release`: Full compiler optimizations (`-O3`), suitable for benchmarking and packaging.
- `-DENABLE_KDE=ON` (default): Enables native KDE Plasma 6 global shortcut integration via `KF6GlobalAccel` and `KF6XmlGui`.
- `-DENABLE_KDE=OFF`: Disables KDE framework dependencies completely; ideal for pure GNOME, Sway, Hyprland, or minimal desktop environments.
- `-DENABLE_DEV_HOTRELOAD=ON`: Automatically updates `~/.local/bin/EmojiTyper` after each successful build for rapid iteration.

### 3. Compile

```bash
cmake --build build --parallel "$(nproc)"
```

### 4. Run Locally

You can launch the freshly compiled binary directly from the build directory:

```bash
./build/bin/EmojiTyper
```

TypEmoji runs as a resident background process with a system tray icon. You can interact with the running instance using the command-line interface:

```bash
./build/bin/EmojiTyper --show     # Force-open the emoji picker panel
./build/bin/EmojiTyper --toggle   # Toggle panel visibility (open/close)
./build/bin/EmojiTyper --quit     # Gracefully shut down the daemon
./build/bin/EmojiTyper --reload   # Hot-reload updated binary without losing session state
```

---

## Distribution Dependency Table

Below are the exact packages required to build and run TypEmoji across major Linux distributions.

| Component | Arch Linux / Manjaro | Ubuntu 24.04+ / Debian 12+ | Fedora 40+ / RHEL | openSUSE Tumbleweed / Leap |
| :--- | :--- | :--- | :--- | :--- |
| **C++23 Compiler** | `gcc` (>= 13) or `clang` | `g++` (>= 13) or `clang-16` | `gcc-c++` (>= 13) | `gcc-c++` (>= 13) |
| **Build Tools** | `cmake` `ninja` | `cmake` `ninja-build` | `cmake` `ninja-build` | `cmake` `ninja` |
| **Qt 6 Core & GUI** | `qt6-base` `qt6-tools` | `qt6-base-dev` `qt6-tools-dev` | `qt6-qtbase-devel` `qt6-qttools-devel` | `qt6-base-devel` `qt6-tools-devel` |
| **X11 & XTest** | `libxtst` `libx11` | `libx11-dev` `libxtst-dev` | `libX11-devel` `libXtst-devel` | `libX11-devel` `libXtst-devel` |
| **KDE 6 (Optional)** | `kglobalaccel` `kxmlgui` | `libkf6globalaccel-dev` `libkf6xmlgui-dev` | `kf6-kglobalaccel-devel` `kf6-kxmlgui-devel` | `kf6-kglobalaccel-devel` `kf6-kxmlgui-devel` |
| **Wayland Typing** | `ydotool` or `wtype` | `ydotool` or `wtype` | `ydotool` or `wtype` | `ydotool` or `wtype` |
| **Clipboard Tools** | `wl-clipboard` `xclip` | `wl-clipboard` `xclip` | `wl-clipboard` `xclip` | `wl-clipboard` `xclip` |
| **Color Emoji Fonts** | `noto-fonts-emoji` | `fonts-noto-color-emoji` | `google-noto-color-emoji-fonts` | `noto-coloremoji-fonts` |

### One-Line Install Commands for Dependencies

#### Arch Linux / Manjaro:
```bash
sudo pacman -S --needed cmake ninja gcc qt6-base qt6-tools libxtst libx11 noto-fonts-emoji
# Optional KDE support:
sudo pacman -S --needed kglobalaccel kxmlgui
# Recommended runtime typing tools:
sudo pacman -S --needed ydotool wtype wl-clipboard xclip
```

#### Ubuntu 24.04+ / Debian 12+ / Linux Mint:
```bash
sudo apt update
sudo apt install -y cmake ninja-build g++ qt6-base-dev qt6-tools-dev libx11-dev libxtst-dev fonts-noto-color-emoji
# Optional KDE support:
sudo apt install -y libkf6globalaccel-dev libkf6xmlgui-dev
# Recommended runtime typing tools:
sudo apt install -y ydotool wtype wl-clipboard xclip
```

#### Fedora 40+ / RHEL:
```bash
sudo dnf install -y cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qttools-devel libX11-devel libXtst-devel google-noto-color-emoji-fonts
# Optional KDE support:
sudo dnf install -y kf6-kglobalaccel-devel kf6-kxmlgui-devel
# Recommended runtime typing tools:
sudo dnf install -y ydotool wtype wl-clipboard xclip
```

#### openSUSE Tumbleweed / Leap:
```bash
sudo zypper install -y cmake ninja gcc-c++ qt6-base-devel qt6-tools-devel libX11-devel libXtst-devel noto-coloremoji-fonts
# Optional KDE support:
sudo zypper install -y kf6-kglobalaccel-devel kf6-kxmlgui-devel
# Recommended runtime typing tools:
sudo zypper install -y ydotool wtype wl-clipboard xclip
```

---

## Code Style & Conventions

We follow clean, modern C++23 standards, Qt6 best practices, and the **Ponytail philosophy** (minimal, robust code with zero unnecessary abstractions).

### 1. Modern C++23 Standards
- **Compiler standard**: Code must compile cleanly with `-std=c++23` under both GCC (>= 13) and Clang (>= 16).
- **Zero compiler warnings**: We enforce `-Wall -Wextra -Wpedantic -Werror`. Code must produce zero warnings.
- **String views**: Use `std::string_view` or `QStringView` for non-owning string arguments to eliminate temporary allocations during search and indexing.
- **Structured bindings & constexpr**: Prefer `auto [key, val]` structured bindings and `constexpr` / `consteval` for compile-time lookup tables.

### 2. Memory Management & Ownership
- **Strict RAII**: Wrap all system handles (file descriptors, X11 `Display*`, temporary files, timers) in RAII management objects.
- **Smart Pointers**:
  - Use `std::unique_ptr` for exclusive heap ownership.
  - Use `std::shared_ptr` only when ownership is genuinely shared across asynchronous boundaries.
  - **No raw owning pointers**: Raw pointers (`T*`) are strictly non-owning observers. Never call `delete` manually.
- **Qt Parent-Child Hierarchy**: When creating `QObject` or `QWidget` children, always assign a parent `QObject` to ensure automatic cascade cleanup.

### 3. Qt6 Idioms & UI Practices
- **Modern Signal/Slot Syntax**: Always use compile-time pointer-to-member syntax:
  ```cpp
  // Correct:
  connect(button, &QPushButton::clicked, this, &EmojiPanel::onEmojiSelected);

  // Prohibited:
  connect(button, SIGNAL(clicked()), this, SLOT(onEmojiSelected()));
  ```
- **Theme-Adaptive Styling**: Avoid hardcoding hex color literals in C++ logic. Rely on system `QPalette` colors (`window()`, `windowText()`, `base()`, `highlight()`) or query dynamic desktop color schemes so light and dark modes adapt seamlessly.
- **Model/View Separation**: UI data display must utilize `QAbstractListModel` and `QSortFilterProxyModel` (`EmojiModel`, `EmojiSearchProxy`) to keep data filtering distinct from rendering.

### 4. Error Handling & Logging
- **Fail Closed**: If a hotkey backend or injection method is unavailable, log a diagnostic message and gracefully cascade to the next fallback strategy rather than aborting.
- **No Stdout Noise**: Avoid committing `std::cout` or raw `qDebug()` statements. Use structured categorization or quiet error signals.
- **The Ponytail Rule**:
  - *Does this need to exist?* Reject speculative features (YAGNI).
  - *Rewrite over wrapping*: If a function has a bug or architectural defect, fix it cleanly at the root cause rather than adding wrapper layers.
  - *No promotional slop*: Do not use hyperbolic marketing adjectives in code comments or commit messages (`seamless`, `revolutionary`, `magical`). Explain the *why*, not the obvious *what*.

---

## Architecture Tour

TypEmoji is architected as an efficient, lightweight daemon consisting of four primary subsystems:

```
+-------------------------------------------------------------------------+
|                                TrayHost                                 |
|  - Daemon lifecycle & system tray management                            |
|  - SingleInstance local socket IPC (--show, --toggle, --quit, --reload) |
|  - Global hotkey dispatcher (PortalHotkey, KGlobalAccel, X11Hotkey)     |
+-------------------------------------------------------------------------+
                                     |
               +---------------------+---------------------+
               |                                           |
               v                                           v
+-----------------------------+             +-----------------------------+
|        EmojiDatabase        |             |         EmojiPanel          |
|  - Unicode 16.0 parsing     |             |  - Floating frameless UI    |
|  - ZWJ sequences & symbols  |             |  - Dynamic QPalette theming |
|  - Fuzzy Levenshtein search |             |  - Skin-tone popup grid     |
|  - Recents & favorites      |             |  - Keyboard navigation      |
+-----------------------------+             +-----------------------------+
                                                           |
                                                           v
                                            +-----------------------------+
                                            |        EmojiInjector        |
                                            |  - Focus verification       |
                                            |  - Cascading injection      |
                                            |    (wtype, ydotool, XTest)  |
                                            |  - Clipboard preservation   |
                                            |  - Queued batch execution   |
                                            +-----------------------------+
```

### 1. `TrayHost` (`src/tray/TrayHost.*`)
- **Lifecycle Coordinator**: Manages the application daemon, background event loop, and system tray icon (`QSystemTrayIcon`).
- **Single-Instance IPC**: Employs `QLocalServer` and `QLocalSocket` to handle CLI arguments (`--toggle`, `--show`, `--quit`, `--reload`) sent from subsequent process invocations.
- **Hotkey Routing**: Detects whether the current session is Wayland or X11 and dynamically initializes the appropriate backend:
  - `PortalHotkey`: Communicates with `org.freedesktop.portal.Desktop` GlobalShortcuts DBus interface for Wayland sessions (GNOME Shell, Sway, Hyprland).
  - `KGlobalAccelHotkey`: Connects directly to KDE Plasma 6's native `KGlobalAccel` daemon when running under KDE Wayland.
  - `X11Hotkey`: Utilizes standard X11 `XGrabKey` to bind `Meta + .` on X11 display servers.
- **Development In-Place Reloading**: Uses `QFileSystemWatcher` to monitor binary modification timestamps; automatically re-executes itself on rebuild when developer hot-reloading is enabled.

### 2. `EmojiPanel` (`src/panel/EmojiPanel.*`)
- **Frameless Floating Picker**: A customized 396×506px floating window configured with `Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint`.
- **Drop Shadow & Corner Geometry**: Features a 12px outer drop shadow canvas margin with concentric 14px rounded corners. Automatically clamps to display screen geometry boundaries.
- **Adaptive Theming**: Intercepts `QEvent::PaletteChange` and `QEvent::ThemeChange` to dynamically compute contrast tokens, background tints, and borders for dark and light desktop palettes.
- **Interactive UI Components**:
  - *Vector Search Pill*: Antialiased vector magnifying glass icon, frosted input pill, and instant search clearing button.
  - *Pill Category Bar*: Touch-friendly 38px pill tabs for quick switching between Smileys, People, Animals, Food, Travel, Activities, Objects, Symbols, Flags, and Kaomoji.
  - *Grid Layout*: 44×44px concentric item cells with 22pt emoji typography, hover elevation, and amber indicators for skin tone variants.
  - *Skin Tone Popup*: Interactive variant picker for diverse human emojis.
  - *Elevated Preview Bar*: 52px footer displaying emoji preview, CLDR short name, Unicode hex codepoints (`U+1F600`), and keyboard shortcut tags (`[↵ Insert]` `[⇧↵ Keep]`).
- **Keyboard Navigation**: Complete arrow key navigation (`Left`, `Right`, `Up`, `Down`, `Home`, `End`), `Enter` to insert and dismiss, `Shift + Enter` for continuous multi-emoji typing, and `Escape` to close.

### 3. `EmojiInjector` (`src/injector/EmojiInjector.*`)
- **Direct Character Injection**: Types characters directly into the focused window without permanently overwriting user clipboard data.
- **Cascading Injection Strategy**:
  1. *Wayland (wtype)*: Direct hardware-level character simulation via Wayland virtual keyboard protocol.
  2. *Wayland (ydotool)*: Direct input simulation via `/run/user/$UID/.ydotool_socket`.
  3. *X11 (XTest)*: Native `XTestFakeKeyEvent` simulation with hardware keysym mapping.
  4. *Clipboard Paste Simulation*: Temporary clipboard emulation sending synthetic `Ctrl+V` or `Shift+Insert` key combinations.
- **Clipboard Preservation Engine**:
  - Before performing any paste simulation, saves the existing user clipboard MIME data (`saveClipboard()`).
  - Sets the temporary clipboard text to the requested emoji.
  - Simulates the paste event into the target window.
  - Restores the original user clipboard contents after a debounced interval (`restoreClipboard()`), ensuring zero clipboard pollution.
- **Queued Batching**: Queues rapid successive inputs (e.g., holding `Shift + Enter`) into `m_queuedEmojis` and drains via `processQueue()`, eliminating dropped characters and race conditions.

### 4. `EmojiDatabase` (`src/data/EmojiDatabase.*`)
- **Unicode 16.0 Standard Catalog**: Ingests official Unicode CLDR datasets (`data/emoji-test.txt` and `data/emoji-zwj-sequences.txt`) along with curated math and typography symbols (`data/symbols.json`).
- **Data Model**: Constructs an indexed catalog of `EmojiEntry` records containing CLDR names, categories, subcategories, keywords, skin-tone variant arrays, and pre-composed UTF-8 display strings.
- **Fuzzy Search Algorithm**: Implements an optimized Damerau-Levenshtein distance algorithm with multi-factor relevance scoring (exact match weighting, prefix bonuses, and keyword matching).
- **Recents & Frequency Tracking**: Persists recently and frequently used emojis across user desktop sessions using `QSettings`.

---

## Manual Testing Guidelines

Automated unit tests cannot fully verify desktop compositor interactions, global shortcut grabs, and target window focus. Every PR modifying hotkeys, injection, or UI must be manually verified across the following dimensions:

### 1. Display Server & Compositor Matrix
Test in at least one compositor for your target changes:
- **Wayland (GNOME Shell 44+)**:
  - Verify that `Meta + .` shortcut registration succeeds via XDG Desktop Portal.
  - Test emoji injection into native Wayland applications (GNOME Text Editor, Console, Firefox Wayland).
- **Wayland (KDE Plasma 6)**:
  - Verify shortcut registration via KGlobalAccel.
  - Test injection into Kate, Konsole, and Qt6 native windows.
- **Wayland (Sway / Hyprland)**:
  - Verify IPC toggle behavior (`EmojiTyper --toggle` mapped to compositor keybinding).
  - Verify typing via `wtype` or `ydotool` into Alacritty / Foot terminal.
- **X11 (Any Desktop)**:
  - Verify `XGrabKey` intercepts `Meta + .`.
  - Test injection via XTest into legacy X11 windows and terminals.

### 2. Clipboard Restoration Verification
Preserving user clipboard data is a core design promise of TypEmoji. Verify that clipboard contents are never corrupted:
1. Copy rich text, a URL, or multi-line code into your system clipboard:
   ```bash
   echo "CRITICAL_CLIPBOARD_SECRET_12345" | xclip -selection clipboard  # X11
   # or
   echo "CRITICAL_CLIPBOARD_SECRET_12345" | wl-copy                      # Wayland
   ```
2. Switch to a text editor (e.g., Gedit, Kate, VS Code).
3. Press `Meta + .` to open TypEmoji.
4. Select an emoji (e.g., 🚀) and press `Enter` to insert.
5. In your editor, immediately press `Ctrl + V` or run `wl-paste` / `xclip -o -selection clipboard`.
6. **Pass Criteria**: The pasted text must be `CRITICAL_CLIPBOARD_SECRET_12345`, NOT the emoji. The user's clipboard must be intact.
7. Repeat the test using `Shift + Enter` to insert 5 emojis consecutively. Verify the clipboard remains unpolluted after queue flushing.

### 3. Theme Adaptability (Light & Dark)
TypEmoji must look native on both light and dark desktop themes:
1. Open your desktop system settings and switch to **Dark Theme**.
2. Open TypEmoji: verify search pill background, active category pill indicator, text readability, and drop shadow contrast.
3. Switch your desktop to **Light Theme** while TypEmoji is running.
4. Open TypEmoji again: verify that borders, text colors, selection highlights, and preview bar updated dynamically without restarting the application.

### 4. Non-KDE Portability Build Check
To prevent introducing accidental hard dependencies on KDE Frameworks:
```bash
cmake -B build-nokde -S . -G Ninja -DENABLE_KDE=OFF
cmake --build build-nokde
./build-nokde/bin/EmojiTyper --version
rm -rf build-nokde
```
**Pass Criteria**: Compiles with zero errors and runs cleanly on systems without KDE libraries.

---

## Submitting a Pull Request

Follow these steps to submit your contribution:

### 1. Create a Topic Branch
Work on a descriptive topic branch off the latest `main`:
```bash
git checkout main
git pull origin main
git checkout -b feat/search-levenshtein-optimization
# or
git checkout -b fix/wayland-focus-drop
```

### 2. Adhere to Conventional Commits
We require commit messages following the [Conventional Commits specification](https://www.conventionalcommits.org/):
```
<type>(<scope>): <short description in imperative mood>

[optional body explaining motivation and technical approach]

[optional footer referencing issue numbers: Fixes #42]
```

#### Allowed Types:
- `feat`: A new user-facing feature or capability.
- `fix`: A bug fix.
- `refactor`: Code reorganization with no functional or behavioral change.
- `perf`: Performance optimization.
- `docs`: Documentation additions or revisions.
- `style`: Formatting, whitespace, or naming cleanups.
- `test`: Adding or refining test suites.
- `ci` / `build`: CMake, packaging, or GitHub Actions workflow changes.

#### Examples:
```bash
git commit -m "fix(injector): restore clipboard state after ydotool paste simulation"
git commit -m "feat(panel): add skin-tone popup selector for diverse human emojis"
git commit -m "docs(contributing): add multi-distro dependency table and testing guidelines"
```

### 3. Keep Pull Requests Focused
- **Atomic changes**: Keep PRs focused on a single responsibility. Avoid combining bug fixes with large-scale stylistic refactors.
- **Clean history**: Rebase onto `main` and squash extraneous "wip" or "fix typo" commits before requesting review:
  ```bash
  git fetch origin
  git rebase -i origin/main
  ```

### 4. Review Process
1. Once opened, GitHub Actions CI will build TypEmoji across multiple environments (Release, Debug, non-KDE, GCC, Clang).
2. A maintainer will review your pull request within our **24-hour SLA**.
3. Once approved and all CI checks pass, your PR will be squash-merged into `main`.

Thank you for helping make TypEmoji the best emoji input tool on Linux! 🚀
