#!/bin/bash
# run_mission8_tests.sh — Build and run Mission 8 test suites.
#
# Cross-platform. On Windows/MinGW the tests are compiled against the project's
# compat surface (-I include/compat -include hermes_win32.h -D_GNU_SOURCE),
# linked with the winlibs libs, and output to .testbin — a native gcc cannot
# resolve /tmp, and -lpthread does not exist. The Linux path keeps /tmp.
#
# A suite that cannot run is reported as an explicit SKIP with its reason, never
# as a silent pass; a build failure is a failure, not a skip.
#
# Usage: bash tests/run_mission8_tests.sh
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PASS=0
FAIL=0
SKIP=0
cd "$PROJECT_DIR"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    OUTDIR=".testbin"
    FLAGS="-O2 -g -D_GNU_SOURCE -include hermes_win32.h -I include/compat -I include"
    LIBS="-L C:/winlibs/extra-libs -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 -lole32 -luuid -lstdc++ -lm -lz -lpcre2-8 -ltre -lssl -lcrypto"
    SLERMES_BIN="slermes.exe"
    ;;
  *)
    OUTDIR="/tmp"
    FLAGS="-O2 -g -I include"
    LIBS="-lm -lpthread"
    SLERMES_BIN="./slermes"
    ;;
esac
mkdir -p "$OUTDIR"

# Locate a pty helper (script) so the UI test runs with a real tty.
PTY_HELPER=""
if command -v script >/dev/null 2>&1; then
  PTY_HELPER="script -qec"
fi
# A pty run that hangs (the Windows pty never answers a terminal query) must
# not take the whole gate down with it.
PTY_TIMEOUT="25"

# Decide web_server binary name (built target).
WS_BIN=""
if [ -x ./web_server ]; then
  WS_BIN=./web_server
elif [ -x "$PROJECT_DIR/web_server" ]; then
  WS_BIN="$PROJECT_DIR/web_server"
fi

echo "=== Mission 8 Test Suite ==="
echo ""
echo "--- Building tests ---"

if gcc $FLAGS -o "$OUTDIR/test_api" tests/integration/test_api_endpoints.c \
    lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o $LIBS \
    2> "$OUTDIR/build_api.err"; then
  echo "  Built: test_api_endpoints"
else
  echo "  FAIL: test_api_endpoints (build failed)"
  sed -n '1,5p' "$OUTDIR/build_api.err"
  FAIL=$((FAIL + 1))
fi

if gcc $FLAGS -o "$OUTDIR/test_cli" tests/cli/test_cli_tests.c \
    lib/libjson/json.o $LIBS 2> "$OUTDIR/build_cli.err"; then
  echo "  Built: test_cli"
else
  echo "  FAIL: test_cli (build failed)"
  sed -n '1,5p' "$OUTDIR/build_cli.err"
  FAIL=$((FAIL + 1))
fi

if gcc $FLAGS -I lib/libdb -o "$OUTDIR/test_state_db" tests/state_db/test_state_db.c \
    lib/libdb/sqlite3.o lib/libdb/db.o $LIBS 2> "$OUTDIR/build_state.err"; then
  echo "  Built: test_state_db"
else
  echo "  FAIL: test_state_db (build failed)"
  sed -n '1,5p' "$OUTDIR/build_state.err"
  FAIL=$((FAIL + 1))
fi

# UI Tests: tui.o pulls in libjson + libbase64, so link those too.
if gcc $FLAGS -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=199309L -I lib/libncurses/include -I lib/libtui \
    -o "$OUTDIR/test_ui" tests/ui/test_ui_harness.c lib/libtui/tui.o \
    lib/libcurses_widget/curses_widget.o lib/libjson/json.o lib/libbase64/base64.o \
    $LIBS 2> "$OUTDIR/build_ui.err"; then
  echo "  Built: test_ui"
else
  echo "  FAIL: test_ui (build failed)"
  sed -n '1,5p' "$OUTDIR/build_ui.err"
  FAIL=$((FAIL + 1))
fi

echo ""

# ── Run State DB Tests (no server needed) ──
echo "--- State DB Tests ---"
if [ -x "$OUTDIR/test_state_db" ]; then
  OUT=$("$OUTDIR/test_state_db" 2>&1)
  PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
  FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
  echo "$OUT" | sed 's/^/  /'
else
  echo "  SKIP (not built)"
  SKIP=$((SKIP + 27))
fi

echo ""

# ── Run CLI Tests ──
echo "--- CLI Tests ---"
if [ -x "$OUTDIR/test_cli" ]; then
  OUT=$("$OUTDIR/test_cli" "$SLERMES_BIN" 2>&1)
  PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
  FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
  echo "$OUT" | sed 's/^/  /'
else
  echo "  SKIP (not built)"
  SKIP=$((SKIP + 9))
fi

echo ""

# ── Run UI Tests (auto-spawn a pty so isatty() is true) ──
echo "--- UI Tests ---"
if [ -x "$OUTDIR/test_ui" ]; then
  if [ -n "$PTY_HELPER" ]; then
    timeout "$PTY_TIMEOUT" $PTY_HELPER "$OUTDIR/test_ui" "$OUTDIR/test_ui_pty.log" \
      > "$OUTDIR/ui_pty.out" 2>&1
    rc=$?
    if [ "$rc" -eq 124 ]; then
      echo "  SKIP: test_ui (pty run timed out after ${PTY_TIMEOUT}s — the Windows pty never"
      echo "        answers the terminal query, so the harness blocks on stdin)"
      SKIP=$((SKIP + 12))
    else
      OUT=$(grep -aE 'PASS|FAIL|Results' "$OUTDIR/ui_pty.out" 2>/dev/null)
      PASS=$((PASS + $(echo "$OUT" | grep -c "PASS" || true)))
      FAIL=$((FAIL + $(echo "$OUT" | grep -c "FAIL" || true)))
      echo "$OUT" | sed 's/^/  /'
    fi
  else
    echo "  SKIP: test_ui (no pty helper 'script' on PATH)"
    SKIP=$((SKIP + 12))
  fi
else
  echo "  SKIP (not built)"
  SKIP=$((SKIP + 12))
fi

echo ""

# ── Run API Integration Tests (auto-start web_server if needed) ──
echo "--- API Integration Tests ---"
if [ -x "$OUTDIR/test_api" ]; then
  NEED_STOP=0
  if ! curl -s http://localhost:5174/health >/dev/null 2>&1; then
    if [ -n "$WS_BIN" ]; then
      echo "  (starting web_server on 5174 for this run)"
      "$WS_BIN" 5174 > "$OUTDIR/ws_auto.log" 2>&1 &
      WS_PID=$!
      NEED_STOP=1
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
    OUT=$("$OUTDIR/test_api" 5174 2>&1)
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
  SKIP=$((SKIP + 15))
fi

echo ""
echo "=== Mission 8 Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
exit 0
