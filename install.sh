#!/usr/bin/env bash
# ==============================================================================
# TypEmoji Official One-Line Installer
# Safe to run directly via:
#   curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash
#
# Direct Emoji Typing Tool for Linux Desktops (X11 & Wayland)
# https://github.com/Ayan-Jain-737/TypEmoji
# ==============================================================================
set -euo pipefail

VERSION="2.3.2"
GITHUB_REPO="Ayan-Jain-737/TypEmoji"
RELEASE_BASE_URL="https://github.com/${GITHUB_REPO}/releases/download/v${VERSION}"
REPO_CLONE_URL="https://github.com/${GITHUB_REPO}.git"

# Detect script directory if running from a local clone
SCRIPT_DIR=""
PROJECT_ROOT=""
if [[ -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]:-}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    if [[ -f "$SCRIPT_DIR/scripts/install.sh" && -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
        PROJECT_ROOT="$SCRIPT_DIR"
    fi
fi

# Terminal colors and formatting
if [[ -t 1 || -t 2 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    BOLD=''
    NC=''
fi

step() { echo -e "\n${BLUE}${BOLD}▶ $1${NC}"; }
ok()   { echo -e "  ${GREEN}✔ $1${NC}"; }
warn() { echo -e "  ${YELLOW}⚠ $1${NC}"; }
err()  { echo -e "  ${RED}✖ $1${NC}"; }
info() { echo -e "  ${CYAN}ℹ $1${NC}"; }

# Temporary directory management
TMP_DIR="$(mktemp -d /tmp/typemoji_install_XXXXXX)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# Print ASCII Banner
print_banner() {
    echo -e "${BOLD}${BLUE}"
    echo "  ████████╗██╗   ██╗██████╗ ███████╗███╗   ███╗ ██████╗      ██╗██╗"
    echo "  ╚══██╔══╝╚██╗ ██╔╝██╔══██╗██╔════╝████╗ ████║██╔═══██╗     ██║██║"
    echo "     ██║    ╚████╔╝ ██████╔╝█████╗  ██╔████╔██║██║   ██║     ██║██║"
    echo "     ██║     ╚██╔╝  ██╔═══╝ ██╔══╝  ██║╚██╔╝██║██║   ██║██   ██║██║"
    echo "     ██║      ██║   ██║     ███████╗██║ ╚═╝ ██║╚██████╔╝╚█████╔╝██║"
    echo "     ╚═╝      ╚═╝   ╚═╝     ╚══════╝╚═╝     ╚═╝ ╚═════╝  ╚════╝ ╚═╝"
    echo -e "${NC}"
    echo -e "  ${BOLD}TypEmoji v${VERSION}${NC} — Direct Emoji Typing Tool for Linux (X11 & Wayland)"
    echo "  ─────────────────────────────────────────────────────────────"
}

# Print help message
show_help() {
    print_banner
    echo ""
    echo "Usage: install.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -b, --build       Compile and install from source instead of fetching precompiled packages"
    echo "  -h, --help        Show this help message and exit"
    echo "  -v, --version     Show version"
    echo ""
    echo "Examples:"
    echo "  curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash"
    echo "  curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash -s -- --build"
    echo "  ./install.sh --build"
    echo ""
    exit 0
}

# Parse Command Line Arguments
BUILD_FROM_SOURCE=false
for arg in "$@"; do
    case "$arg" in
        -b|--build)
            BUILD_FROM_SOURCE=true
            ;;
        -h|--help)
            show_help
            ;;
        -v|--version)
            echo "TypEmoji installer v${VERSION}"
            exit 0
            ;;
        *)
            warn "Unknown argument: $arg (ignoring)"
            ;;
    esac
done

print_banner

# ── 1. ENVIRONMENT & PRIVILEGE DETECTION ──────────────────────────────────────
step "Detecting system environment and privileges"

ARCH="$(uname -m)"
info "Architecture: ${BOLD}${ARCH}${NC}"

if [[ "$ARCH" != "x86_64" && "$ARCH" != "amd64" ]]; then
    warn "Official precompiled binary releases target x86_64/amd64."
    warn "Detected non-x86_64 architecture ($ARCH). Automatically enabling source build mode (--build)."
    BUILD_FROM_SOURCE=true
fi

DISTRO="unknown"
DISTRO_LIKE=""
DISTRO_PRETTY="Linux"

if [[ -f /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    DISTRO="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
    DISTRO_PRETTY="${PRETTY_NAME:-$DISTRO}"
elif [[ -f /usr/lib/os-release ]]; then
    # shellcheck disable=SC1091
    . /usr/lib/os-release
    DISTRO="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
    DISTRO_PRETTY="${PRETTY_NAME:-$DISTRO}"
fi

info "Distribution: ${BOLD}${DISTRO_PRETTY}${NC} (ID: ${DISTRO}, ID_LIKE: ${DISTRO_LIKE:-none})"

# Determine distribution family
FAMILY="generic"
if [[ "$DISTRO" =~ (ubuntu|debian|linuxmint|pop|elementary|zorin|kali|devuan|raspbian) ]] || \
   [[ "$DISTRO_LIKE" =~ (ubuntu|debian) ]]; then
    FAMILY="debian"
elif [[ "$DISTRO" =~ (fedora|rhel|centos|rocky|almalinux) ]] || \
     [[ "$DISTRO_LIKE" =~ (fedora|rhel) ]]; then
    FAMILY="fedora"
elif [[ "$DISTRO" =~ (opensuse|suse) ]] || \
     [[ "$DISTRO_LIKE" =~ (suse|opensuse) ]]; then
    FAMILY="suse"
elif [[ "$DISTRO" =~ (arch|manjaro|endeavouros|garuda|artix|chimera) ]] || \
     [[ "$DISTRO_LIKE" =~ (arch) ]]; then
    FAMILY="arch"
fi

info "Package manager family: ${BOLD}${FAMILY}${NC}"

# Detect privileges: Root vs Sudo vs Standard User
IS_ROOT=false
CAN_SUDO=false
SUDO_CMD=""

if [[ $EUID -eq 0 ]]; then
    IS_ROOT=true
    SUDO_CMD=""
    info "Running as ${BOLD}root${NC} — system-wide installation enabled."
else
    if command -v sudo &>/dev/null; then
        if sudo -n true 2>/dev/null; then
            CAN_SUDO=true
            SUDO_CMD="sudo"
            info "Passwordless ${BOLD}sudo${NC} privileges detected."
        elif [[ -c /dev/tty ]]; then
            echo ""
            info "Requesting sudo privileges for native package manager installation:"
            if sudo -v </dev/tty 2>/dev/null; then
                CAN_SUDO=true
                SUDO_CMD="sudo"
                ok "Sudo privileges authenticated."
            else
                warn "Sudo authentication was not completed. Proceeding with user-level installation (~/.local)."
            fi
        else
            warn "No active TTY available for sudo prompt. Proceeding with user-level installation (~/.local)."
        fi
    else
        info "sudo command not found. Proceeding with user-level installation (~/.local)."
    fi
fi

# Download utility wrapper
download_file() {
    local url="$1"
    local dest="$2"
    if command -v curl &>/dev/null; then
        curl -fsSL --connect-timeout 20 --retry 2 "$url" -o "$dest" 2>/dev/null
    elif command -v wget &>/dev/null; then
        wget -q --timeout=20 --tries=2 -O "$dest" "$url" 2>/dev/null
    else
        return 1
    fi
}

# ── 2. INSTALLATION DISPATCH ──────────────────────────────────────────────────
INSTALLED=false

# Helper for installing via portable tarball into ~/.local or /usr/local
install_portable_tarball() {
    local tarball_path="$1"
    local target_prefix
    if [[ "$IS_ROOT" == true ]]; then
        target_prefix="/usr/local"
    else
        target_prefix="${HOME}/.local"
    fi

    step "Extracting portable release to $target_prefix"
    mkdir -p "$target_prefix/bin"
    mkdir -p "$target_prefix/share/emojityper/data"
    mkdir -p "$target_prefix/share/applications"
    mkdir -p "$target_prefix/share/icons/hicolor/scalable/apps"
    mkdir -p "$target_prefix/share/metainfo"

    tar -xzf "$tarball_path" -C "$target_prefix" --strip-components=1
    chmod +x "$target_prefix/bin/EmojiTyper"
    ok "Installed binary to $target_prefix/bin/EmojiTyper"

    # Autostart configuration
    local autostart_dir="${HOME}/.config/autostart"
    if [[ "$IS_ROOT" == true ]]; then
        autostart_dir="/etc/xdg/autostart"
    fi
    mkdir -p "$autostart_dir"
    if [[ -f "$target_prefix/share/applications/org.emojiboard.EmojiTyper.desktop" ]]; then
        cp -f "$target_prefix/share/applications/org.emojiboard.EmojiTyper.desktop" "$autostart_dir/"
        ok "Configured desktop autostart in $autostart_dir"
    fi

    # Update desktop and icon databases
    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "$target_prefix/share/applications" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "$target_prefix/share/icons/hicolor" 2>/dev/null || true
    fi

    # Clean up duplicate systemd user service if present (desktop autostart handles startup cleanly)
    if [[ "$IS_ROOT" == false ]] && command -v systemctl &>/dev/null; then
        systemctl --user disable --now emojityper.service 2>/dev/null || true
        rm -f "${HOME}/.config/systemd/user/emojityper.service" 2>/dev/null || true
        systemctl --user daemon-reload 2>/dev/null || true
    fi

    INSTALLED=true
}

# Helper to build from source using scripts/install.sh
compile_from_source() {
    step "Compiling TypEmoji from source"
    if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/scripts/install.sh" ]]; then
        info "Running local build script in $PROJECT_ROOT"
        bash "$PROJECT_ROOT/scripts/install.sh"
        INSTALLED=true
    else
        info "Cloning latest TypEmoji repository..."
        local clone_dir="$TMP_DIR/typemoji_repo"
        if ! command -v git &>/dev/null; then
            err "git is required to build from source. Please install git or run on an x86_64 machine with precompiled releases."
            exit 1
        fi
        git clone --depth 1 "$REPO_CLONE_URL" "$clone_dir"
        chmod +x "$clone_dir/scripts/install.sh"
        bash "$clone_dir/scripts/install.sh"
        INSTALLED=true
    fi
}

# Check if building from source was explicitly requested
if [[ "$BUILD_FROM_SOURCE" == true ]]; then
    compile_from_source
fi

# Attempt precompiled package installation if not yet installed
if [[ "$INSTALLED" == false ]]; then
    step "Fetching release v${VERSION} precompiled packages"

    # Strategy 1: Native package manager installation if root or sudo is available
    if [[ "$IS_ROOT" == true || "$CAN_SUDO" == true ]]; then
        case "$FAMILY" in
            debian)
                DEB_FILE="TypEmoji-${VERSION}-Ubuntu-Debian-x86_64.deb"
                DEB_URL="${RELEASE_BASE_URL}/${DEB_FILE}"
                DEB_DEST="$TMP_DIR/$DEB_FILE"

                # Check if file exists locally in repo dist/ (useful for testing/offline)
                if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$DEB_FILE" ]]; then
                    cp "$PROJECT_ROOT/dist/$DEB_FILE" "$DEB_DEST"
                elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Ubuntu-Debian-x86_64.deb" ]]; then
                    cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Ubuntu-Debian-x86_64.deb" "$DEB_DEST"
                else
                    info "Downloading Debian/Ubuntu package: $DEB_URL"
                    download_file "$DEB_URL" "$DEB_DEST" || true
                fi

                if [[ -f "$DEB_DEST" && -s "$DEB_DEST" ]]; then
                    info "Installing .deb package via apt..."
                    if $SUDO_CMD apt-get update -qq 2>/dev/null && $SUDO_CMD apt-get install -y "$DEB_DEST"; then
                        ok "Successfully installed Debian/Ubuntu package via apt"
                        INSTALLED=true
                    elif $SUDO_CMD dpkg -i "$DEB_DEST" 2>/dev/null || $SUDO_CMD apt-get install -f -y; then
                        ok "Successfully installed Debian/Ubuntu package via dpkg/apt-get"
                        INSTALLED=true
                    else
                        warn "Native .deb package installation encountered an issue; trying fallback..."
                    fi
                fi
                ;;

            fedora)
                RPM_FILE="TypEmoji-${VERSION}-Fedora-openSUSE-x86_64.rpm"
                RPM_URL="${RELEASE_BASE_URL}/${RPM_FILE}"
                RPM_DEST="$TMP_DIR/$RPM_FILE"

                if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$RPM_FILE" ]]; then
                    cp "$PROJECT_ROOT/dist/$RPM_FILE" "$RPM_DEST"
                elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Fedora-openSUSE-x86_64.rpm" ]]; then
                    cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Fedora-openSUSE-x86_64.rpm" "$RPM_DEST"
                else
                    info "Downloading Fedora RPM package: $RPM_URL"
                    download_file "$RPM_URL" "$RPM_DEST" || true
                fi

                if [[ -f "$RPM_DEST" && -s "$RPM_DEST" ]]; then
                    info "Installing RPM package via dnf..."
                    if $SUDO_CMD dnf install -y "$RPM_DEST"; then
                        ok "Successfully installed Fedora package via dnf"
                        INSTALLED=true
                    elif $SUDO_CMD rpm -Uvh --force "$RPM_DEST"; then
                        ok "Successfully installed Fedora package via rpm"
                        INSTALLED=true
                    else
                        warn "Native RPM package installation encountered an issue; trying fallback..."
                    fi
                fi
                ;;

            suse)
                RPM_FILE="TypEmoji-${VERSION}-Fedora-openSUSE-x86_64.rpm"
                RPM_URL="${RELEASE_BASE_URL}/${RPM_FILE}"
                RPM_DEST="$TMP_DIR/$RPM_FILE"

                if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$RPM_FILE" ]]; then
                    cp "$PROJECT_ROOT/dist/$RPM_FILE" "$RPM_DEST"
                elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Fedora-openSUSE-x86_64.rpm" ]]; then
                    cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Fedora-openSUSE-x86_64.rpm" "$RPM_DEST"
                else
                    info "Downloading openSUSE RPM package: $RPM_URL"
                    download_file "$RPM_URL" "$RPM_DEST" || true
                fi

                if [[ -f "$RPM_DEST" && -s "$RPM_DEST" ]]; then
                    info "Installing RPM package via zypper..."
                    if $SUDO_CMD zypper --non-interactive install --allow-unsigned-rpm "$RPM_DEST"; then
                        ok "Successfully installed openSUSE package via zypper"
                        INSTALLED=true
                    else
                        warn "Native zypper package installation encountered an issue; trying fallback..."
                    fi
                fi
                ;;

            arch)
                PKG_FILE="TypEmoji-${VERSION}-Arch-Linux-x86_64.pkg.tar.zst"
                PKG_URL="${RELEASE_BASE_URL}/${PKG_FILE}"
                PKG_DEST="$TMP_DIR/$PKG_FILE"

                if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$PKG_FILE" ]]; then
                    cp "$PROJECT_ROOT/dist/$PKG_FILE" "$PKG_DEST"
                elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Arch-Linux-x86_64.pkg.tar.zst" ]]; then
                    cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Arch-Linux-x86_64.pkg.tar.zst" "$PKG_DEST"
                else
                    info "Downloading Arch Linux package: $PKG_URL"
                    download_file "$PKG_URL" "$PKG_DEST" || true
                fi

                if [[ -f "$PKG_DEST" && -s "$PKG_DEST" ]]; then
                    info "Installing package via pacman..."
                    if $SUDO_CMD pacman -U --noconfirm "$PKG_DEST"; then
                        ok "Successfully installed Arch Linux package via pacman"
                        INSTALLED=true
                    else
                        warn "Native pacman package installation encountered an issue; trying fallback..."
                    fi
                fi
                ;;
        esac
    fi

    # Strategy 2: Universal Self-Extracting Installer (.run)
    if [[ "$INSTALLED" == false ]]; then
        RUN_FILE="TypEmoji-${VERSION}-Universal-Installer.run"
        RUN_URL="${RELEASE_BASE_URL}/${RUN_FILE}"
        RUN_DEST="$TMP_DIR/$RUN_FILE"

        if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$RUN_FILE" ]]; then
            cp "$PROJECT_ROOT/dist/$RUN_FILE" "$RUN_DEST"
        elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Universal-Installer.run" ]]; then
            cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Universal-Installer.run" "$RUN_DEST"
        else
            info "Downloading Universal Installer: $RUN_URL"
            download_file "$RUN_URL" "$RUN_DEST" || true
        fi

        if [[ -f "$RUN_DEST" && -s "$RUN_DEST" ]]; then
            chmod +x "$RUN_DEST"
            info "Executing Universal Installer..."
            if "$RUN_DEST"; then
                ok "Successfully installed via Universal Installer"
                INSTALLED=true
            else
                warn "Universal Installer execution failed; trying portable tarball fallback..."
            fi
        fi
    fi

    # Strategy 3: Portable Tarball (.tar.gz) user installation
    if [[ "$INSTALLED" == false ]]; then
        TAR_FILE="TypEmoji-${VERSION}-Portable-Linux-x86_64.tar.gz"
        TAR_URL="${RELEASE_BASE_URL}/${TAR_FILE}"
        TAR_DEST="$TMP_DIR/$TAR_FILE"

        if [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/$TAR_FILE" ]]; then
            cp "$PROJECT_ROOT/dist/$TAR_FILE" "$TAR_DEST"
        elif [[ -n "$PROJECT_ROOT" && -f "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Portable-Linux-x86_64.tar.gz" ]]; then
            cp "$PROJECT_ROOT/dist/TypEmoji-1.0.0-Portable-Linux-x86_64.tar.gz" "$TAR_DEST"
        else
            info "Downloading Portable Tarball: $TAR_URL"
            download_file "$TAR_URL" "$TAR_DEST" || true
        fi

        if [[ -f "$TAR_DEST" && -s "$TAR_DEST" ]]; then
            install_portable_tarball "$TAR_DEST"
        fi
    fi

    # Strategy 4: Fall back to compiling from source if all precompiled packages failed
    if [[ "$INSTALLED" == false ]]; then
        warn "Could not fetch precompiled binaries (release may not yet be published or network restricted)."
        info "Falling back to compiling and building from source..."
        compile_from_source
    fi
fi

if [[ "$INSTALLED" == false ]]; then
    err "Installation failed. Please review the output above or report an issue at:"
    err "https://github.com/${GITHUB_REPO}/issues"
    exit 1
fi

# ── 3. POST-INSTALL CHECKS & AUTO-LAUNCH ──────────────────────────────────────
step "Finalizing setup"

BIN_PATH=""
if command -v EmojiTyper &>/dev/null; then
    BIN_PATH="$(command -v EmojiTyper)"
elif [[ -x "/usr/local/bin/EmojiTyper" ]]; then
    BIN_PATH="/usr/local/bin/EmojiTyper"
elif [[ -x "/usr/bin/EmojiTyper" ]]; then
    BIN_PATH="/usr/bin/EmojiTyper"
elif [[ -x "${HOME}/.local/bin/EmojiTyper" ]]; then
    BIN_PATH="${HOME}/.local/bin/EmojiTyper"
fi

if [[ -n "$BIN_PATH" ]]; then
    ok "EmojiTyper executable ready at: ${BOLD}${BIN_PATH}${NC}"
else
    warn "EmojiTyper executable not found in standard paths."
fi

# PATH guidance for user-level installs
if [[ "$IS_ROOT" == false && ":$PATH:" != *":${HOME}/.local/bin:"* ]]; then
    warn "~/.local/bin is currently not in your PATH."
    echo "    To run EmojiTyper directly from terminal, add this to ~/.bashrc or ~/.zshrc:"
    echo "      export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

# Launch background daemon for standard user sessions
if [[ "$IS_ROOT" == false && -n "$BIN_PATH" ]]; then
    if pgrep -x "EmojiTyper" >/dev/null 2>&1; then
        ok "EmojiTyper is currently running."
    else
        step "Starting TypEmoji background service"
        nohup "$BIN_PATH" > /tmp/emojityper.log 2>&1 &
        sleep 1
        if pgrep -x "EmojiTyper" >/dev/null 2>&1; then
            ok "TypEmoji background service started successfully."
            if command -v notify-send &>/dev/null; then
                notify-send "TypEmoji Ready" "Press Super + . (Win + .) to open the emoji picker." -i emojityper 2>/dev/null || true
            fi
        else
            warn "TypEmoji daemon could not be started automatically. You can launch it manually with: $BIN_PATH"
        fi
    fi
fi

# ── 4. COMPLETION BANNER & USAGE INSTRUCTIONS ─────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  ✔ TypEmoji v${VERSION} installed successfully!${NC}"
echo ""
echo "  ┌─────────────────────────────────────────────────────────┐"
echo "  │  How to use TypEmoji                                    │"
echo "  │                                                         │"
echo "  │  • Press  Super + .  (Win + .)  to open emoji panel     │"
echo "  │  • Or click the TypEmoji icon in your system tray       │"
echo "  │  • Start typing immediately to search emojis            │"
echo "  │  • Click or press Enter to type an emoji                │"
echo "  │  • Press Shift + Enter for continuous typing            │"
echo "  │  • Press Escape to dismiss the panel                    │"
echo "  │                                                         │"
echo "  │  TypEmoji will start automatically whenever you log in. │"
echo "  └─────────────────────────────────────────────────────────┘"
echo ""
echo -e "  Documentation & Source: ${CYAN}https://github.com/${GITHUB_REPO}${NC}"
echo -e "${GREEN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
echo ""
exit 0
