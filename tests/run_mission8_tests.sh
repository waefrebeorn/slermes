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
build test_i18n2 tests/test_i18n.c src/agent/i18n.c \
    src/agent/logger.o lib/libjson/json.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h
build test_hook tests/test_hook_registry.c src/agent/hook_registry.c \
    src/agent/logger.o lib/libjson/json.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h
build test_fs tests/test_file_safety.c src/agent/file_safety.c \
    src/agent/logger.o lib/libjson/json.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h
build test_anth tests/test_anthropic_depth.c src/agent/provider_anthropic.c \
    src/agent/logger.o src/tools/url_safety.o lib/libjson/json.o lib/libhttp/http.o \
    lib/libcrypto/crypto.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h \
    -lssl -lcrypto -lz -lws2_32
build test_acct tests/test_account_usage.c src/tools/account_usage.c \
    src/agent/logger.o lib/libjson/json.o lib/libhttp/http.o \
    $(for d in lib/*/; do printf -- "-I %s " "$d"; done) -I include/compat -I include -I src -I lib \
    -include hermes_win32.h -lssl -lcrypto -lz -lws2_32
build test_bedrock tests/test_bedrock_depth.c src/agent/bedrock_adapter.c \
    src/agent/provider_bedrock.o src/agent/logger.o lib/libjson/json.o \
    $(for d in lib/*/; do printf -- "-I %s " "$d"; done) -I include/compat -I include -I src -I lib \
    -include hermes_win32.h -lssl -lcrypto -lws2_32
build test_aux tests/test_auxiliary_client.c src/agent/auxiliary_client.c \
    src/tools/url_safety.o src/agent/logger.o lib/libjson/json.o -lws2_32 \
    $(for d in lib/*/; do printf -- "-I %s " "$d"; done) -I include/compat -I include -I src -I lib \
    -include hermes_win32.h
build test_think tests/test_think_scrubber.c src/agent/think_scrubber.c \
    lib/libjson/json.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h
build test_cron tests/test_cron_extras_util.c src/cron/cron_extras.c \
    lib/libjson/json.o $(for d in lib/*/; do printf -- "-I %s " "$d"; done) \
    -I include/compat -I include -I src -I lib -include hermes_win32.h
build test_threat tests/test_threat_patterns.c src/agent/threat_patterns.c \
    lib/libjson/json.o -I include/compat -I include -include hermes_win32.h
build test_ansi tests/test_ansi_strip.c lib/libansi/ansi_strip.c \
    -I include/compat -I include -I lib/libansi -include hermes_win32.h

echo

count_and_report() {
    local out="$1"
    local p="" f="" tot="" summary
    # Harnesses in this repo print three different summary shapes:
    #   "Results: 27 passed, 0 failed"
    #   "Results: 63/63 passed"
    #   "file_safety: 26/26 passed"
    # plus some that print no numbers at all. Parse the shapes explicitly and
    # fall back to counting lines; never assume a number.
    summary=$(printf '%s\n' "$out" | grep -oE '[0-9]+/[0-9]+ passed' | tail -1)
    if [ -n "$summary" ]; then
        p=$(printf '%s' "$summary" | sed -E 's|([0-9]+)/([0-9]+) passed|\1|')
        tot=$(printf '%s' "$summary" | sed -E 's|([0-9]+)/([0-9]+) passed|\2|')
        f=$((tot - p))
    else
        summary=$(printf '%s\n' "$out" | grep -oE 'Results: [0-9]+ passed, [0-9]+ failed' | tail -1)
        if [ -n "$summary" ]; then
            p=$(printf '%s' "$summary" | sed -E 's|Results: ([0-9]+) passed.*|\1|')
            f=$(printf '%s' "$summary" | sed -E 's|.*passed, ([0-9]+) failed.*|\1|')
        else
            echo "  NOTE: no numeric summary line; counting PASS/FAIL text"
            p=$(printf '%s\n' "$out" | grep -cE '^[[:space:]]*(PASS|OK)' || true)
            f=$(printf '%s\n' "$out" | grep -cE '^[[:space:]]*FAIL' || true)
        fi
    fi
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
    # A freshly linked exe is briefly refused by the OS (AV holds a handle on
    # the new file): rc 126 with no output. Retry once rather than report a
    # false "cannot execute".
    if [ "$rc" -eq 126 ]; then
        sleep 20
        out="$("$bin" "$@" 2>&1)"
        rc=$?
    fi
    count_and_report "$out"
    if [ "$rc" -eq 126 ]; then
        echo "  cannot execute (rc 126 even after retry)"
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
run_suite "ANSI Strip" "$B/test_ansi.exe"
run_suite "Threat Patterns" "$B/test_threat.exe"
run_suite "Cron Extras" "$B/test_cron.exe"
run_suite "Think Scrubber" "$B/test_think.exe"
run_suite "Auxiliary Client" "$B/test_aux.exe"
run_suite "Bedrock Depth" "$B/test_bedrock.exe"
run_suite "Account Usage" "$B/test_acct.exe"
run_suite "Anthropic Depth" "$B/test_anth.exe"
run_suite "File Safety" "$B/test_fs.exe"
run_suite "Hook Registry" "$B/test_hook.exe"
run_suite "i18n Core" "$B/test_i18n2.exe"

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
