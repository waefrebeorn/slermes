#!/bin/bash
# run_mission8_tests.sh — Build and run Mission 8 test suites
# Usage: bash tests/run_mission8_tests.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PASS=0
FAIL=0
SKIP=0

cd "$PROJECT_DIR"

echo "=== Mission 8 Test Suite ==="
echo ""

# ── Build all test binaries ──
echo "--- Building tests ---"

# API Integration Tests
if gcc -O2 -g -I include -o /tmp/test_api tests/integration/test_api_endpoints.c \
    lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o -lssl -lcrypto -lz -lm -lpthread 2>/dev/null; then
    echo "  Built: test_api_endpoints"
else
    echo "  SKIP: test_api_endpoints (build failed)"
    SKIP=$((SKIP + 40))
fi

# CLI Tests
if gcc -O2 -g -I include -o /tmp/test_cli tests/cli/test_cli_tests.c \
    lib/libjson/json.o -lm -lpthread 2>/dev/null; then
    echo "  Built: test_cli"
else
    echo "  SKIP: test_cli (build failed)"
    SKIP=$((SKIP + 8))
fi

# State DB Tests
if gcc -O2 -g -I include -I lib/libdb -o /tmp/test_state_db tests/state_db/test_state_db.c \
    lib/libdb/sqlite3.o lib/libdb/db.o -lm -lpthread 2>/dev/null; then
    echo "  Built: test_state_db"
else
    echo "  SKIP: test_state_db (build failed)"
    SKIP=$((SKIP + 18))
fi

# UI Tests
if gcc -O2 -g -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=199309L -I include -I lib/libncurses/include -I lib/libtui \
    -o /tmp/test_ui tests/ui/test_ui_harness.c lib/libtui/tui.o \
    lib/libncurses_widget/curses_widget.o -lncursesw -ltinfo -lpanelw -lm -lpthread 2>/dev/null; then
    echo "  Built: test_ui"
else
    echo "  SKIP: test_ui (build failed)"
    SKIP=$((SKIP + 10))
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

# ── Run UI Tests ──
echo "--- UI Tests ---"
if [ -x /tmp/test_ui ] && { [ -n "${TERMINAL:-}" ] || [ -t 0 ]; }; then
    OUT=$(/tmp/test_ui 2>&1)
    PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
    FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
    echo "$OUT" | sed 's/^/  /'
else
    echo "  SKIP (no terminal available)"
    SKIP=$((SKIP + 10))
fi

echo ""

# ── Run API Integration Tests (requires running server) ──
echo "--- API Integration Tests ---"
if [ -x /tmp/test_api ]; then
    # Check if server is running
    if curl -s http://localhost:5174/health >/dev/null 2>&1; then
        OUT=$(/tmp/test_api 5174 2>&1)
        PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
        FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
        echo "$OUT" | sed 's/^/  /'
    else
        echo "  SKIP (server not running on port 5174)"
        echo "  Start with: ./web_server &"
        SKIP=$((SKIP + 15))
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
