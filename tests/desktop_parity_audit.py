#!/usr/bin/env python3
"""
desktop_parity_audit.py — Detailed desktop/TUI/CLI parity audit

Audits the C implementation against the TypeScript/Electron desktop app
across all 63 IPC handlers and 30+ Electron APIs.

Usage:
    python3 desktop_parity_audit.py [--json] [--detail <area>]
"""

import os, re, sys, json, argparse
from dataclasses import dataclass, field, asdict

SLERMES_DIR = "/home/wubu/hermes-agent-dev/slermes"
DESKTOP_DIR = "/home/wubu/hermes-agent-dev/apps/desktop"

@dataclass
class ParityItem:
    area: str           # "window", "terminal", "session", "profile", "update", etc.
    feature: str        # Human-readable feature name
    ts_source: str      # Where it's implemented in TS
    c_source: str       # Where it should be in C (or "MISSING")
    status: str         # "DONE", "PARTIAL", "MISSING", "STUB"
    priority: str       # "P0" (critical), "P1" (important), "P2" (nice), "P3" (future)
    detail: str         # What's missing
    cua_relevant: bool  # Whether CUA could help with this

# ═══════════════════════════════════════════════════════════════════════════
# COMPLETE FEATURE MAP
# ═══════════════════════════════════════════════════════════════════════════

def build_feature_map() -> list[ParityItem]:
    """Complete feature parity map: Electron/TS desktop → C11 desktop."""
    
    features = []
    
    # ── WINDOW MANAGEMENT ─────────────────────────────────────────────────
    features.extend([
        ParityItem("window", "Window creation (Wayland)", "electron/session-windows.cjs", "src/window_wayland.c", "DONE", "P0",
                   "xdg-shell window with EGL rendering", False),
        ParityItem("window", "Window creation (Win32)", "electron/session-windows.cjs", "src/window_win32.c", "DONE", "P0",
                   "Win32 native window with GDI+", False),
        ParityItem("window", "Window creation (macOS)", "electron/session-windows.cjs", "src/window_macos.m", "PARTIAL", "P0",
                   "Cocoa NSWindow — needs NSView rendering", False),
        ParityItem("window", "Multi-window management", "electron/session-windows.cjs", "src/desktop_app.c", "STUB", "P0",
                   "Session window registry, open/focus/close", False),
        ParityItem("window", "Window minimize/maximize/restore", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Platform-specific window state management", False),
        ParityItem("window", "Window transparency/blur", "electron/main.cjs", "MISSING", "MISSING", "P2",
                   "Acrylic/Mica/BlurBehind effects", False),
        ParityItem("window", "Window always-on-top", "electron/main.cjs", "MISSING", "MISSING", "P2",
                   "Set window z-order", False),
        ParityItem("window", "Window titlebar customization", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Custom titlebar with traffic lights", False),
        ParityItem("window", "Window menu bar", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Application menu (File, Edit, View, etc.)", False),
        ParityItem("window", "System tray icon", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Tray icon with context menu", False),
        ParityItem("window", "Global keyboard shortcuts", "electron/main.cjs", "MISSING", "MISSING", "P2",
                   "Register hotkeys (e.g. Ctrl+Shift+H)", False),
        ParityItem("window", "Deep linking (hermes://)", "electron/main.cjs", "MISSING", "MISSING", "P2",
                   "URL protocol handler", False),
        ParityItem("window", "Single instance lock", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Prevent multiple app instances", False),
    ])
    
    # ── TERMINAL ──────────────────────────────────────────────────────────
    features.extend([
        ParityItem("terminal", "PTY allocation", "node-pty", "MISSING", "MISSING", "P0",
                   "Pseudo-terminal for shell processes", False),
        ParityItem("terminal", "Terminal rendering (xterm)", "@xterm/xterm", "MISSING", "MISSING", "P0",
                   "VT100/xterm terminal emulation + WebGL rendering", False),
        ParityItem("terminal", "Terminal resize", "electron/main.cjs:terminal:resize", "MISSING", "MISSING", "P0",
                   "Send resize events to PTY", False),
        ParityItem("terminal", "Terminal input", "electron/main.cjs:terminal:write", "MISSING", "MISSING", "P0",
                   "Send keystrokes to PTY", False),
        ParityItem("terminal", "Terminal output", "electron/main.cjs:terminal:start", "MISSING", "MISSING", "P0",
                   "Read PTY output and render", False),
        ParityItem("terminal", "Terminal disposal", "electron/main.cjs:terminal:dispose", "MISSING", "MISSING", "P1",
                   "Clean up PTY and renderer", False),
        ParityItem("terminal", "Terminal search", "@xterm/addon-search", "MISSING", "MISSING", "P2",
                   "Search within terminal buffer", False),
        ParityItem("terminal", "Terminal web links", "@xterm/addon-web-links", "MISSING", "MISSING", "P2",
                   "Clickable URLs in terminal", False),
        ParityItem("terminal", "Terminal Unicode", "@xterm/addon-unicode11", "MISSING", "MISSING", "P2",
                   "Full Unicode 11 support", False),
    ])
    
    # ── SESSION MANAGEMENT ────────────────────────────────────────────────
    features.extend([
        ParityItem("session", "Session create", "apps/desktop/src/app/session", "src/desktop_app.c", "STUB", "P0",
                   "Create new chat session", False),
        ParityItem("session", "Session switch", "apps/desktop/src/app/session", "src/desktop_app.c", "STUB", "P0",
                   "Switch between sessions", False),
        ParityItem("session", "Session delete", "apps/desktop/src/app/session", "MISSING", "MISSING", "P0",
                   "Delete session with confirmation", False),
        ParityItem("session", "Session rename", "apps/desktop/src/app/session", "MISSING", "MISSING", "P1",
                   "Rename session title", False),
        ParityItem("session", "Session archive", "apps/desktop/src/app/session", "MISSING", "MISSING", "P1",
                   "Archive/unarchive sessions", False),
        ParityItem("session", "Session search", "apps/desktop/src/app/session", "MISSING", "MISSING", "P1",
                   "Fuzzy search sessions", False),
        ParityItem("session", "Session pin", "apps/desktop/src/app/session", "MISSING", "MISSING", "P2",
                   "Pin sessions to top", False),
        ParityItem("session", "Session export", "apps/desktop/src/app/session", "MISSING", "MISSING", "P2",
                   "Export session as JSON/Markdown", False),
        ParityItem("session", "Session window", "electron/session-windows.cjs", "MISSING", "MISSING", "P0",
                   "Open session in new window", False),
        ParityItem("session", "Session drag & drop", "apps/desktop/src/app/session", "MISSING", "MISSING", "P2",
                   "Drag sessions to reorder", False),
    ])
    
    # ── CHAT INTERFACE ────────────────────────────────────────────────────
    features.extend([
        ParityItem("chat", "Message rendering", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P0",
                   "Render markdown, code blocks, tool calls", False),
        ParityItem("chat", "Code syntax highlighting", "react-shiki", "MISSING", "MISSING", "P1",
                   "Shiki-based code highlighting", False),
        ParityItem("chat", "Math rendering", "remark-math + katex", "MISSING", "MISSING", "P2",
                   "LaTeX math rendering", False),
        ParityItem("chat", "Message composer", "apps/desktop/src/app/chat/composer", "MISSING", "MISSING", "P0",
                   "Text input with autocomplete", False),
        ParityItem("chat", "File attachments", "apps/desktop/src/app/chat/composer", "MISSING", "MISSING", "P1",
                   "Drag & drop files into chat", False),
        ParityItem("chat", "Image paste", "apps/desktop/src/app/chat/composer", "MISSING", "MISSING", "P1",
                   "Paste images from clipboard", False),
        ParityItem("chat", "Slash commands", "apps/desktop/src/app/chat/composer", "MISSING", "MISSING", "P1",
                   "/help, /clear, /model, etc.", False),
        ParityItem("chat", "Voice input", "apps/desktop/src/app/chat/composer", "MISSING", "MISSING", "P2",
                   "Voice activity detection + transcription", False),
        ParityItem("chat", "Voice output", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P2",
                   "TTS playback of responses", False),
        ParityItem("chat", "Streaming responses", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P0",
                   "Real-time streaming from gateway", False),
        ParityItem("chat", "Tool call rendering", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P1",
                   "Render tool calls and results", False),
        ParityItem("chat", "Artifact rendering", "apps/desktop/src/app/artifacts", "MISSING", "MISSING", "P2",
                   "HTML/JS/CSS artifact preview", False),
        ParityItem("chat", "Reasoning display", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P2",
                   "Show model reasoning chain", False),
        ParityItem("chat", "Context menu", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P2",
                   "Right-click menu on messages", False),
        ParityItem("chat", "Scroll to bottom", "apps/desktop/src/app/chat", "MISSING", "MISSING", "P1",
                   "Auto-scroll + scroll-to-bottom button", False),
    ])
    
    # ── PROFILES ──────────────────────────────────────────────────────────
    features.extend([
        ParityItem("profile", "Profile list", "apps/desktop/src/app/profiles", "src/web_app.c", "STUB", "P1",
                   "List all profiles", False),
        ParityItem("profile", "Profile create", "apps/desktop/src/app/profiles", "src/web_app.c", "STUB", "P1",
                   "Create new profile", False),
        ParityItem("profile", "Profile switch", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P1",
                   "Switch active profile", False),
        ParityItem("profile", "Profile delete", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P1",
                   "Delete profile with confirmation", False),
        ParityItem("profile", "Profile rename", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P2",
                   "Rename profile", False),
        ParityItem("profile", "Profile soul", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P1",
                   "Edit profile soul (system prompt)", False),
        ParityItem("profile", "Profile model", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P1",
                   "Set model per profile", False),
        ParityItem("profile", "Profile scope", "apps/desktop/src/app/profiles", "MISSING", "MISSING", "P2",
                   "Profile scope isolation", False),
    ])
    
    # ── MODELS ────────────────────────────────────────────────────────────
    features.extend([
        ParityItem("model", "Model list", "apps/desktop/src/app/models", "src/web_app.c", "DONE", "P0",
                   "List available models", False),
        ParityItem("model", "Model picker", "apps/desktop/src/app/models", "MISSING", "MISSING", "P0",
                   "Dialog to select model", False),
        ParityItem("model", "Model switch", "apps/desktop/src/app/models", "MISSING", "MISSING", "P0",
                   "Switch active model", False),
        ParityItem("model", "Model info card", "apps/desktop/src/app/models", "MISSING", "MISSING", "P1",
                   "Show model capabilities, limits", False),
        ParityItem("model", "Auxiliary models", "apps/desktop/src/app/models", "MISSING", "MISSING", "P2",
                   "Models for specific tasks", False),
        ParityItem("model", "Model analytics", "apps/desktop/src/app/models", "MISSING", "MISSING", "P2",
                   "Usage stats per model", False),
        ParityItem("model", "Model visibility", "apps/desktop/src/app/models", "MISSING", "MISSING", "P2",
                   "Show/hide models in picker", False),
    ])
    
    # ── SETTINGS ──────────────────────────────────────────────────────────
    features.extend([
        ParityItem("settings", "Settings page", "apps/desktop/src/app/settings", "src/web_app.c", "STUB", "P1",
                   "Settings UI", False),
        ParityItem("settings", "Theme switcher", "apps/desktop/src/app/settings", "MISSING", "MISSING", "P1",
                   "Dark/light/system theme", False),
        ParityItem("settings", "Font settings", "apps/desktop/src/app/settings", "MISSING", "MISSING", "P2",
                   "Font family, size", False),
        ParityItem("settings", "Default project dir", "apps/desktop/src/app/settings", "MISSING", "MISSING", "P2",
                   "Set default working directory", False),
        ParityItem("settings", "Connection config", "apps/desktop/src/app/settings", "MISSING", "MISSING", "P1",
                   "Gateway URL, auth settings", False),
        ParityItem("settings", "Environment vars", "apps/desktop/src/app/settings", "MISSING", "MISSING", "P2",
                   "Edit .env variables", False),
    ])
    
    # ── NATIVE OS INTEGRATIONS ────────────────────────────────────────────
    features.extend([
        ParityItem("native", "Clipboard read", "electron/main.cjs:writeClipboard", "MISSING", "MISSING", "P0",
                   "Read text/image from clipboard", False),
        ParityItem("native", "Clipboard write", "electron/main.cjs:writeClipboard", "MISSING", "MISSING", "P0",
                   "Write text/image to clipboard", False),
        ParityItem("native", "File dialog (open)", "electron/main.cjs:selectPaths", "MISSING", "MISSING", "P1",
                   "Native file picker dialog", False),
        ParityItem("native", "File dialog (save)", "electron/main.cjs:selectPaths", "MISSING", "MISSING", "P1",
                   "Native save dialog", False),
        ParityItem("native", "File read", "electron/main.cjs:readFileText", "MISSING", "MISSING", "P0",
                   "Read file contents", False),
        ParityItem("native", "File browse", "electron/main.cjs:fs:readDir", "MISSING", "MISSING", "P1",
                   "Directory listing", False),
        ParityItem("native", "Open external URL", "electron/main.cjs:openExternal", "MISSING", "MISSING", "P1",
                   "Open URL in system browser", False),
        ParityItem("native", "Show in folder", "electron/main.cjs:shell", "MISSING", "MISSING", "P2",
                   "Reveal file in file manager", False),
        ParityItem("native", "Notifications", "electron/main.cjs:notify", "MISSING", "MISSING", "P1",
                   "Native OS notifications", False),
        ParityItem("native", "Dark mode detection", "electron/main.cjs:native-theme", "MISSING", "MISSING", "P2",
                   "Detect system dark mode", False),
        ParityItem("native", "Spell checker", "electron/main.cjs:session", "MISSING", "MISSING", "P3",
                   "Native spell checking", False),
        ParityItem("native", "Microphone access", "electron/main.cjs:requestMicrophoneAccess", "MISSING", "MISSING", "P2",
                   "Request mic permission", False),
        ParityItem("native", "Safe storage", "electron/main.cjs:safeStorage", "MISSING", "MISSING", "P1",
                   "Encrypted credential storage", False),
        ParityItem("native", "Power monitor", "electron/main.cjs:powerMonitor", "MISSING", "MISSING", "P3",
                   "Detect sleep/wake/lock", False),
        ParityItem("native", "Haptics", "web-haptics", "MISSING", "MISSING", "P3",
                   "Haptic feedback (macOS)", False),
    ])
    
    # ── UPDATES ───────────────────────────────────────────────────────────
    features.extend([
        ParityItem("update", "Check for updates", "electron/main.cjs:updates:check", "MISSING", "MISSING", "P1",
                   "Check remote for new version", False),
        ParityItem("update", "Download update", "electron/main.cjs:updates:apply", "MISSING", "MISSING", "P1",
                   "Download and stage update", False),
        ParityItem("update", "Apply update", "electron/main.cjs:update-relaunch", "MISSING", "MISSING", "P1",
                   "Restart with new version", False),
        ParityItem("update", "Update branch", "electron/main.cjs:updates:branch", "MISSING", "MISSING", "P2",
                   "Switch between stable/beta/dev", False),
        ParityItem("update", "Update marker", "electron/update-marker.cjs", "MISSING", "MISSING", "P2",
                   "Track update state", False),
    ])
    
    # ── GATEWAY CONNECTION ────────────────────────────────────────────────
    features.extend([
        ParityItem("gateway", "WebSocket client", "apps/shared/src/json-rpc-gateway.ts", "MISSING", "MISSING", "P0",
                   "Connect to gateway via WebSocket", False),
        ParityItem("gateway", "Gateway probe", "electron/gateway-ws-probe.cjs", "MISSING", "MISSING", "P0",
                   "Check if gateway is reachable", False),
        ParityItem("gateway", "Gateway URL config", "electron/connection-config.cjs", "MISSING", "MISSING", "P0",
                   "Configure gateway URL", False),
        ParityItem("gateway", "Auth ticket", "electron/dashboard-token.cjs", "MISSING", "MISSING", "P1",
                   "Dashboard auth token management", False),
        ParityItem("gateway", "Connection revalidate", "electron/main.cjs:connection:revalidate", "MISSING", "MISSING", "P1",
                   "Reconnect on connection loss", False),
        ParityItem("gateway", "OAuth login", "electron/main.cjs:connection-config:oauth-login", "MISSING", "MISSING", "P2",
                   "OAuth flow for gateway auth", False),
    ])
    
    # ── FILE OPERATIONS ───────────────────────────────────────────────────
    features.extend([
        ParityItem("file", "File read text", "electron/main.cjs:readFileText", "src/web_app.c", "DONE", "P0",
                   "Read file as text", False),
        ParityItem("file", "File read data URL", "electron/main.cjs:readFileDataUrl", "MISSING", "MISSING", "P1",
                   "Read file as base64 data URL", False),
        ParityItem("file", "File write", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Write file contents", False),
        ParityItem("file", "File delete", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Delete file", False),
        ParityItem("file", "Directory create", "electron/main.cjs", "MISSING", "MISSING", "P1",
                   "Create directory", False),
        ParityItem("file", "Directory listing", "electron/fs-read-dir.cjs", "src/web_app.c", "DONE", "P0",
                   "List directory contents", False),
        ParityItem("file", "File watch", "electron/main.cjs:watchPreviewFile", "MISSING", "MISSING", "P2",
                   "Watch file for changes", False),
        ParityItem("file", "Git root", "electron/git-root.cjs", "MISSING", "MISSING", "P2",
                   "Find git repository root", False),
        ParityItem("file", "Git worktrees", "electron/git-worktrees.cjs", "MISSING", "MISSING", "P3",
                   "List git worktrees", False),
        ParityItem("file", "Clipboard image save", "electron/main.cjs:saveClipboardImage", "MISSING", "MISSING", "P2",
                   "Save clipboard image to file", False),
        ParityItem("file", "Image from URL save", "electron/main.cjs:saveImageFromUrl", "MISSING", "MISSING", "P2",
                   "Download and save image", False),
    ])
    
    # ── UNINSTALL ─────────────────────────────────────────────────────────
    features.extend([
        ParityItem("uninstall", "Uninstall summary", "electron/desktop-uninstall.cjs", "MISSING", "MISSING", "P2",
                   "Show what will be removed", False),
        ParityItem("uninstall", "Run uninstall", "electron/desktop-uninstall.cjs", "MISSING", "MISSING", "P2",
                   "Remove app and optionally data", False),
    ])
    
    # ── LOGS ──────────────────────────────────────────────────────────────
    features.extend([
        ParityItem("logs", "Recent logs", "electron/main.cjs:logs:recent", "MISSING", "MISSING", "P2",
                   "Show recent log entries", False),
        ParityItem("logs", "Reveal logs", "electron/main.cjs:logs:reveal", "MISSING", "MISSING", "P2",
                   "Open log file in editor", False),
    ])
    
    # ── MARKETPLACE ───────────────────────────────────────────────────────
    features.extend([
        ParityItem("marketplace", "Theme marketplace", "electron/vscode-marketplace.cjs", "MISSING", "MISSING", "P3",
                   "Browse and install themes", False),
        ParityItem("marketplace", "Theme search", "electron/vscode-marketplace.cjs", "MISSING", "MISSING", "P3",
                   "Search marketplace themes", False),
    ])
    
    return features


def main():
    parser = argparse.ArgumentParser(description="Desktop Parity Audit")
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--detail', type=str, help='Filter by area')
    parser.add_argument('--priority', choices=['P0', 'P1', 'P2', 'P3', 'ALL'], default='ALL')
    args = parser.parse_args()
    
    features = build_feature_map()
    
    if args.detail:
        features = [f for f in features if f.area == args.detail]
    if args.priority != 'ALL':
        features = [f for f in features if f.priority == args.priority]
    
    if args.json:
        print(json.dumps({
            'summary': {
                'total': len(build_feature_map()),
                'done': len([f for f in build_feature_map() if f.status == 'DONE']),
                'partial': len([f for f in build_feature_map() if f.status == 'PARTIAL']),
                'stub': len([f for f in build_feature_map() if f.status == 'STUB']),
                'missing': len([f for f in build_feature_map() if f.status == 'MISSING']),
                'p0': len([f for f in build_feature_map() if f.priority == 'P0']),
                'p1': len([f for f in build_feature_map() if f.priority == 'P1']),
                'p2': len([f for f in build_feature_map() if f.priority == 'P2']),
                'p3': len([f for f in build_feature_map() if f.priority == 'P3']),
            },
            'features': [asdict(f) for f in features],
        }, indent=2))
    else:
        print("🖥️  DESKTOP PARITY AUDIT — Electron/TS → C11")
        print("=" * 70)
        
        # Summary
        all_features = build_feature_map()
        done = len([f for f in all_features if f.status == 'DONE'])
        partial = len([f for f in all_features if f.status == 'PARTIAL'])
        stub = len([f for f in all_features if f.status == 'STUB'])
        missing = len([f for f in all_features if f.status == 'MISSING'])
        total = len(all_features)
        
        print(f"\n  Total features: {total}")
        print(f"  ✅ DONE:     {done:3d} ({done*100//total}%)")
        print(f"  🟡 PARTIAL:  {partial:3d} ({partial*100//total}%)")
        print(f"  🟠 STUB:     {stub:3d} ({stub*100//total}%)")
        print(f"  🔴 MISSING:  {missing:3d} ({missing*100//total}%)")
        
        for p in ['P0', 'P1', 'P2', 'P3']:
            p_features = [f for f in all_features if f.priority == p]
            p_done = len([f for f in p_features if f.status in ('DONE', 'PARTIAL')])
            print(f"\n  {p}: {p_done}/{len(p_features)} complete")
        
        # By area
        areas = {}
        for f in all_features:
            if f.area not in areas:
                areas[f.area] = {'total': 0, 'done': 0, 'partial': 0, 'stub': 0, 'missing': 0}
            areas[f.area]['total'] += 1
            areas[f.area][f.status.lower()] += 1
        
        print(f"\n{'='*70}")
        print("  BY AREA")
        print(f"{'='*70}")
        for area, stats in sorted(areas.items()):
            done_pct = (stats['done'] + stats['partial']) * 100 // stats['total']
            bar = '█' * (done_pct // 5) + '░' * (20 - done_pct // 5)
            print(f"  {area:15s} [{bar}] {done_pct:3d}%  ({stats['done']}✅ {stats['partial']}🟡 {stats['stub']}🟠 {stats['missing']}🔴)")
        
        # Detailed by priority
        for p in ['P0', 'P1', 'P2', 'P3']:
            p_features = [f for f in all_features if f.priority == p]
            if not p_features:
                continue
            print(f"\n{'='*70}")
            print(f"  {p} PRIORITIES")
            print(f"{'='*70}")
            for f in p_features:
                icon = {'DONE': '✅', 'PARTIAL': '🟡', 'STUB': '🟠', 'MISSING': '🔴'}.get(f.status, '?')
                print(f"  {icon} {f.feature:40s} {f.status:8s}  {f.ts_source[:40]}")
                if f.status != 'DONE':
                    print(f"     → {f.detail}")
        
        # CUA analysis
        print(f"\n{'='*70}")
        print("  CUA ANALYSIS")
        print(f"{'='*70}")
        print("""
  CUA (trycua/cua) is NOT relevant for Hermes Desktop.
  
  CUA provides:    VM-based desktop CONTROL (screen capture, mouse, keyboard)
  Hermes needs:    Native desktop APPLICATION (windows, menus, rendering)
  
  Hermes Desktop is an Electron app that:
  - Creates native windows (BrowserWindow)
  - Renders React UI (chat, settings, terminal)
  - Connects to gateway (WebSocket)
  - Integrates with OS (clipboard, notifications, files)
  
  What we need in C11:
  1. Native window system (DONE: Wayland, Win32, macOS)
  2. UI rendering (needs: immediate-mode or retained-mode GUI)
  3. Terminal emulation (needs: VT100/xterm)
  4. WebSocket client (needs: libwebsockets or similar)
  5. OS integrations (needs: platform-specific APIs)
  
  Recommendation: Build native C11 desktop framework, not CUA wrapper.
  Use: Wayland/X11 + EGL for rendering, libwebsockets for gateway,
       libvterm for terminal, native APIs for OS integration.
""")


if __name__ == '__main__':
    sys.exit(main())
