#!/usr/bin/env bash
# =============================================================================
# slermes-install.sh — Installer for the Slermes C11 agent.
#
# Installs the slermes binary (and optional desktop GUI / web server),
# creates the ~/.slermes home directory structure, and installs shell
# completions. Mirrors the Python hermes installer's responsibilities.
#
# Usage:
#   ./slermes-install.sh [--prefix /usr/local] [--gui] [--web] [--user]
#
# Flags:
#   --prefix DIR   Install under DIR (default: /usr/local for root, ~/.local otherwise)
#   --gui          Also install slermes-desktop-gui (SDL2)
#   --web          Also install web_server
#   --user         Force user install (~/.local/bin, no sudo needed)
# =============================================================================
set -euo pipefail

# ── resolve repo root ───────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

# ── flags ───────────────────────────────────────────────────────────────────
PREFIX=""
INSTALL_GUI=0
INSTALL_WEB=0
FORCE_USER=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)   PREFIX="$2"; shift 2 ;;
        --gui)      INSTALL_GUI=1; shift ;;
        --web)      INSTALL_WEB=1; shift ;;
        --user)     FORCE_USER=1; shift ;;
        --help|-h)
            grep '^#' "$0" | sed 's/^# \{0,1\}//' | sed -n '1,20p'
            exit 0 ;;
        *) echo "unknown flag: $1 (try --help)"; exit 1 ;;
    esac
done

# ── install prefix resolution ───────────────────────────────────────────────
if [[ -z "$PREFIX" ]]; then
    if [[ "$FORCE_USER" == 1 || "$(id -u)" != "0" ]]; then
        PREFIX="$HOME/.local"
    else
        PREFIX="/usr/local"
    fi
fi
BIN_DIR="$PREFIX/bin"

echo "==> Slermes installer"
echo "    prefix: $PREFIX"
echo "    bin:    $BIN_DIR"

# ── build (if binaries missing) ─────────────────────────────────────────────
if [[ ! -x "$REPO_ROOT/slermes" ]]; then
    echo "==> building slermes..."
    make -C "$REPO_ROOT" -j"$(nproc)" slermes
fi
if [[ "$INSTALL_GUI" == 1 && ! -x "$REPO_ROOT/slermes-desktop-gui" ]]; then
    echo "==> building desktop GUI..."
    make -C "$REPO_ROOT" -j"$(nproc)" desktop-gui
fi
if [[ "$INSTALL_WEB" == 1 && ! -x "$REPO_ROOT/web_server" ]]; then
    echo "==> building web server..."
    make -C "$REPO_ROOT" -j"$(nproc)" web
fi

# ── install binaries ────────────────────────────────────────────────────────
mkdir -p "$BIN_DIR"
install -m 0755 "$REPO_ROOT/slermes" "$BIN_DIR/slermes"
echo "    installed: $BIN_DIR/slermes"
if [[ "$INSTALL_GUI" == 1 ]]; then
    install -m 0755 "$REPO_ROOT/slermes-desktop-gui" "$BIN_DIR/slermes-desktop-gui"
    echo "    installed: $BIN_DIR/slermes-desktop-gui"
fi
if [[ "$INSTALL_WEB" == 1 ]]; then
    install -m 0755 "$REPO_ROOT/web_server" "$BIN_DIR/slermes-web-server"
    echo "    installed: $BIN_DIR/slermes-web-server"
fi

# ── home directory structure ────────────────────────────────────────────────
HOME_DIR="${SLERMES_HOME:-$HOME/.slermes}"
mkdir -p "$HOME_DIR"/{sessions,skills,cron,profiles,cache,plugins,logs,pets}
echo "    home:   $HOME_DIR (created directory structure)"

# ── shell completions ───────────────────────────────────────────────────────
if command -v slermes >/dev/null 2>&1; then
    echo "==> shell completions: run 'slermes completions bash' / 'zsh' to generate"
    echo "    (or: source <(slermes completions bash))"
fi

# ── PATH hint ───────────────────────────────────────────────────────────────
case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
        echo
        echo "==> NOTE: $BIN_DIR is not on your PATH."
        echo "    Add it:  export PATH=\"$BIN_DIR:\$PATH\""
        ;;
esac

echo
echo "==> Done. Run 'slermes init' to create your config, then 'slermes --help'."
