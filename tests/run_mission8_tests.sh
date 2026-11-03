#!/usr/bin/env bash
# run_mission8_tests.sh — build and run the Mission 8 suite, honestly.
#
# Rewritten after audit. The original version:
#   * wrote binaries to /tmp, which native MinGW gcc cannot resolve, so every
#     build failed and every suite reported "SKIP";
#   * added INVENTED skip counts (SKIP+=40, +=8, +=18, +=10, +=15) instead of
#     counting anything;
#   * exited 0 whenever FAIL was 0, so a total build failure looked green.
# It reported "27 passed, 0 failed, 68 skipped" while running one of four suites.
#
# Rules now enforced:
#   * a suite that cannot build is a FAILURE, not a skip;
#   * PASS/FAIL are counted from real output, never assumed;
#   * the exit code reflects what actually ran.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/.testbin"

PASS=0
FAIL=0
BROKEN=0
BROKEN_LIST=()

cd "$PROJECT_DIR" || exit 1
mkdir -p "$BUILD_DIR"

# Native binaries need a Windows-resolvable path; /tmp is POSIX-only here.
B="$BUILD_DIR"

echo "=== Mission 8 Test Suite ==="
echo "binaries: $B"
echo

build() {
    local out="$1"; shift
    if ! gcc -O2 -g -o "$B/$out" "$@" 2>"$B/$out.build.log"; then
        echo "  BUILD FAILED: $out"
        sed 's/^/    /' "$B/$out.build.log" | head -6
        BROKEN=$((BROKEN + 1))
        BROKEN_LIST+=("$out (build)")
        return 1
    fi
    echo "  built: $out"
    return 0
}

echo "--- Building ---"
build test_state_db tests/state_db/test_state_db.c \
    lib/libdb/sqlite3.o lib/libdb/db.o -I include/compat -I include -I lib/libdb -lm -lpthread
build test_cli tests/cli/test_cli_tests.c \
    lib/libjson/json.o -I include/compat -I include -lm -lpthread
build test_ui tests/ui/test_ui_harness.c \
    lib/libtui/tui.o lib/libcurses_widget/curses_widget.o lib/libjson/json.o \
    -I include/compat -I include -I lib/libncurses/include -I lib/libtui -I lib/libjson -include hermes_win32.h \
    -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=199309L \
    -lncursesw -lpanelw -lm -lpthread
build test_api tests/integration/test_api_endpoints.c \
    lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o \
    -I include/compat -I include -include hermes_win32.h -lssl -lcrypto -lz -lws2_32 -lm -lpthread
build test_i18n tests/test_i18n_languages.c src/agent/i18n_languages.c \
    lib/libjson/json.o -I include/compat -I include -include hermes_win32.h -lm

echo

count_and_report() {
    local out="$1"
    local p f
    p=$(printf '%s\n' "$out" | grep -c 'PASS' || true)
    f=$(printf '%s\n' "$out" | grep -c 'FAIL' || true)
    PASS=$((PASS + p))
    FAIL=$((FAIL + f))
    printf '%s\n' "$out" | sed 's/^/  /'
}

run_suite() {
    local name="$1"; shift
    local bin="$1"; shift
    echo "--- $name ---"
    if [ ! -x "$bin" ]; then
        echo "  NOT RUN (binary missing)"
        BROKEN=$((BROKEN + 1))
        BROKEN_LIST+=("$name (missing binary)")
        echo
        return
    fi
    local out rc
    out="$("$bin" "$@" 2>&1)"
    rc=$?
    count_and_report "$out"
    if [ "$rc" -eq 126 ]; then
        echo "  cannot execute (rc 126) — the OS refused to run $bin"
        BROKEN=$((BROKEN + 1))
        BROKEN_LIST+=("$name (OS refused to execute)")
    elif [ "$rc" -ne 0 ]; then
        echo "  exited $rc"
        BROKEN=$((BROKEN + 1))
        BROKEN_LIST+=("$name (exit $rc)")
    fi
    echo
}

run_suite "State DB Tests" "$B/test_state_db.exe"
run_suite "CLI Tests" "$B/test_cli.exe" slermes.exe
run_suite "UI Tests" "$B/test_ui.exe"
run_suite "i18n Languages" "$B/test_i18n.exe"

echo "--- API Integration Tests ---"
if [ ! -x "$B/test_api.exe" ]; then
    echo "  NOT RUN (binary missing)"
    BROKEN=$((BROKEN + 1))
    BROKEN_LIST+=("API integration (missing binary)")
elif ! curl -s --max-time 2 http://localhost:5174/health >/dev/null 2>&1; then
    echo "  NOT RUN (no server on :5174). Start one with: ./web_server &"
    BROKEN=$((BROKEN + 1))
    BROKEN_LIST+=("API integration (server not running)")
else
    out="$("$B/test_api.exe" 5174 2>&1)"
    count_and_report "$out"
    echo
fi

echo "=== Results: $PASS passed, $FAIL failed, $BROKEN not run ==="
if [ "$BROKEN" -gt 0 ]; then
    echo "Suites that did not run:"
    for s in "${BROKEN_LIST[@]}"; do echo "  - $s"; done
fi

# Fail closed: anything not run is not a pass.
if [ "$FAIL" -gt 0 ] || [ "$BROKEN" -gt 0 ]; then
    exit 1
fi
exit 0
