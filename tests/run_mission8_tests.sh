#!/bin/bash
# run_mission8_tests.sh — Build and run Mission 8 test suites
# Usage: bash tests/run_mission8_tests.sh
#
# Self-contained: builds every test binary, auto-spawns a pty for the UI test
# (which needs a real terminal) and auto-starts the web_server for the API
# integration tests, so nothing silently SKIPs.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PASS=0
FAIL=0
SKIP=0

cd "$PROJECT_DIR"

# Locate a pty helper (script) so the UI test runs with a real tty.
PTY_HELPER=""
if command -v script >/dev/null 2>&1; then
    PTY_HELPER="script -qec"
fi

# Decide web_server binary name (built target).
WS_BIN=""
if [ -x ./web_server ]; then
    WS_BIN=./web_server
elif [ -x "$PROJECT_DIR/web_server" ]; then
    WS_BIN="$PROJECT_DIR/web_server"
fi

echo "=== Mission 8 Test Suite ==="
echo ""

# ── Build all test binaries ──
echo "--- Building tests ---"

# API Integration Tests
if gcc -O2 -g -I include -o /tmp/test_api tests/integration/test_api_endpoints.c \
    lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o -lssl -lcrypto -lz -lm -lpthread 2>/dev/null; then
    echo "  Built: test_api_endpoints"
else
    echo "  FAIL: test_api_endpoints (build failed)"
    FAIL=$((FAIL + 1))
fi

# CLI Tests
if gcc -O2 -g -I include -o /tmp/test_cli tests/cli/test_cli_tests.c \
    lib/libjson/json.o -lm -lpthread 2>/dev/null; then
    echo "  Built: test_cli"
else
    echo "  FAIL: test_cli (build failed)"
    FAIL=$((FAIL + 1))
fi

# State DB Tests
if gcc -O2 -g -I include -I lib/libdb -o /tmp/test_state_db tests/state_db/test_state_db.c \
    lib/libdb/sqlite3.o lib/libdb/db.o -lm -lpthread 2>/dev/null; then
    echo "  Built: test_state_db"
else
    echo "  FAIL: test_state_db (build failed)"
    FAIL=$((FAIL + 1))
fi

# UI Tests
# Link against explicit .so.6 runtime libs (the ncurses *dev* symlinks may be
# absent on a minimal host; the desktop/tui targets do the same). tui.o pulls
# in libjson + libbase64, so link those too.
if gcc -O2 -g -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=199309L -I include -I lib/libncurses/include -I lib/libtui \
    -o /tmp/test_ui tests/ui/test_ui_harness.c lib/libtui/tui.o \
    lib/libcurses_widget/curses_widget.o lib/libjson/json.o lib/libbase64/base64.o \
    /usr/lib/x86_64-linux-gnu/libncursesw.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.6 -lpanelw -lm -lpthread 2>/dev/null; then
    echo "  Built: test_ui"
else
    echo "  FAIL: test_ui (build failed)"
    FAIL=$((FAIL + 1))
fi

echo ""

# ── Run State DB Tests (no server needed) ──
echo "--- State DB Tests ---"
if [ -x /tmp/test_state_db ]; then
    OUT=$(/tmp/test_state_db 2>&1)
    PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
    FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
    echo "$OUT" | sed 's/^/  /'
else
    echo "  SKIP (not built)"
fi

echo ""

# ── Run CLI Tests ──
echo "--- CLI Tests ---"
if [ -x /tmp/test_cli ]; then
    OUT=$(/tmp/test_cli ./slermes 2>&1)
    PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
    FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
    echo "$OUT" | sed 's/^/  /'
else
    echo "  SKIP (not built)"
fi

echo ""

# ── Run UI Tests (auto-spawn a pty so isatty() is true) ──
echo "--- UI Tests ---"
if [ -x /tmp/test_ui ]; then
    if [ -n "$PTY_HELPER" ]; then
        OUT=$($PTY_HELPER "/tmp/test_ui" /tmp/test_ui_pty.log 2>/dev/null; cat /tmp/test_ui_pty.log)
        # Strip the script wrapper header/footer lines.
        OUT=$(echo "$OUT" | grep -E 'PASS|FAIL|Results')
    else
        OUT=$(/tmp/test_ui 2>&1)
    fi
    PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
    FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
    echo "$OUT" | sed 's/^/  /'
else
    echo "  SKIP (not built)"
fi

echo ""

# ── Run API Integration Tests (auto-start web_server if needed) ──
echo "--- API Integration Tests ---"
if [ -x /tmp/test_api ]; then
    NEED_STOP=0
    if ! curl -s http://localhost:5174/health >/dev/null 2>&1; then
        if [ -n "$WS_BIN" ]; then
            echo "  (starting web_server on 5174 for this run)"
            "$WS_BIN" 5174 >/tmp/ws_auto.log 2>&1 &
            WS_PID=$!
            NEED_STOP=1
            # Wait for health endpoint (max ~10s).
            for _ in $(seq 1 20); do
                if curl -s http://localhost:5174/health >/dev/null 2>&1; then
                    break
                fi
                sleep 0.5
            done
        else
            echo "  WARN: web_server binary not found; run 'make web_server' first"
        fi
    fi
    if curl -s http://localhost:5174/health >/dev/null 2>&1; then
        OUT=$(/tmp/test_api 5174 2>&1)
        PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
        FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
        echo "$OUT" | sed 's/^/  /'
    else
        echo "  SKIP (server not running on port 5174)"
        SKIP=$((SKIP + 15))
    fi
    if [ "$NEED_STOP" -eq 1 ] && [ -n "${WS_PID:-}" ]; then
        kill "$WS_PID" 2>/dev/null || true
    fi
else
    echo "  SKIP (not built)"
fi

echo ""
echo "=== Mission 8 Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
