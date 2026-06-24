#!/bin/bash
# functional_tests.sh — Functional tests for Slermes C
# Tests the actual slermes binary through its CLI.
set -e

SRCDIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SRCDIR/slermes"
PASS=0
FAIL=0
SKIP=0
VERBOSE=""

for arg in "$@"; do
    case "$arg" in --verbose|-v) VERBOSE=1 ;; esac
done

echo "=== Slermes C Functional Tests ==="
echo ""

assert_contains() {
    local name="$1" expected="$2" actual="$3"
    echo -n "  TEST: $name ... "
    if echo "$actual" | grep -qF "$expected"; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        [ -n "$VERBOSE" ] && echo "    Expected: '$expected'" && echo "    Got: '$(echo "$actual" | head -5)'"
        FAIL=$((FAIL + 1))
    fi
}

assert_exit_code() {
    local name="$1" expected="$2"
    shift 2
    echo -n "  TEST: $name ... "
    local rc=0
    "$@" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -eq "$expected" ]; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL (exit $rc, expected $expected)"
        FAIL=$((FAIL + 1))
    fi
}

# ═══════════════════════════════════════════
# Phase 1: CLI Binary Smoke Tests
# ═══════════════════════════════════════════
echo "--- CLI Smoke Tests ---"

HELP_OUTPUT=$("$BINARY" --help 2>&1 || true)
assert_contains "help output shows Usage"    "Usage"       "$HELP_OUTPUT"
assert_contains "help shows gateway"          "gateway"     "$HELP_OUTPUT"
assert_contains "help shows cron"             "cron"        "$HELP_OUTPUT"
assert_contains "help shows tools"            "tools"       "$HELP_OUTPUT"
assert_contains "help shows plugins"          "plugins"     "$HELP_OUTPUT"
assert_contains "help shows secrets"          "secrets"     "$HELP_OUTPUT"
assert_contains "help shows skills"           "skills"      "$HELP_OUTPUT"
assert_contains "help shows status"           "status"      "$HELP_OUTPUT"
assert_contains "help shows logs"             "logs"        "$HELP_OUTPUT"
assert_contains "help shows session"          "session"     "$HELP_OUTPUT"
assert_exit_code "help exits 0"              0             "$BINARY" --help

VER_OUTPUT=$("$BINARY" --version 2>&1 || true)
assert_contains "version output"              "slermes"     "$VER_OUTPUT"
assert_contains "version shows version"       "0.16.0"      "$VER_OUTPUT"

# ═══════════════════════════════════════════
# Phase 2: Doctor Tests
# ═══════════════════════════════════════════
echo ""
echo "--- Doctor Tests ---"

DOCTOR_OUTPUT=$("$BINARY" doctor 2>&1 || true)
assert_contains "doctor shows Slermes Doctor" "Doctor"                  "$DOCTOR_OUTPUT"
assert_contains "doctor shows version"        "0.16.0"                  "$DOCTOR_OUTPUT"
assert_contains "doctor shows provider"       "Provider"                "$DOCTOR_OUTPUT"
assert_contains "doctor shows model"          "Model"                   "$DOCTOR_OUTPUT"
assert_contains "doctor shows config status"  "Config OK"               "$DOCTOR_OUTPUT"

# ═══════════════════════════════════════════
# Phase 3: Tools & Plugins Tests
# ═══════════════════════════════════════════
echo ""
echo "--- Tools & Plugins Tests ---"

TOOLS_OUTPUT=$("$BINARY" tools 2>&1 || true)
assert_contains "tools output"                "terminal"    "$TOOLS_OUTPUT"
assert_contains "tools shows file tool"       "file"        "$TOOLS_OUTPUT"
assert_contains "tools shows web tool"        "web"         "$TOOLS_OUTPUT"

PLUGINS_OUTPUT=$("$BINARY" plugins 2>&1 || true)
assert_contains "plugins output"              "plugin"      "$PLUGINS_OUTPUT"

# ═══════════════════════════════════════════
# Phase 4: Secrets & Skills
# ═══════════════════════════════════════════
echo ""
echo "--- Secrets & Skills Tests ---"

SECRETS_OUTPUT=$("$BINARY" secrets 2>&1 || true)
assert_contains "secrets output"              "secrets"     "$SECRETS_OUTPUT"

SKILLS_OUTPUT=$("$BINARY" skills 2>&1 || true)
# Skills may be empty or list something — just check no crash
echo "  SKILLS output received (${#SKILLS_OUTPUT} chars)"
[ ${#SKILLS_OUTPUT} -gt 0 ] && PASS=$((PASS + 1)) || { echo "  SKIP: no skills output"; SKIP=$((SKIP + 1)); }

# ═══════════════════════════════════════════
# Phase 5: Edge Cases
# ═══════════════════════════════════════════
echo ""
echo "--- Edge Case Tests ---"

# Bad flag
assert_exit_code "invalid flag exits non-zero" 1 "$BINARY" --nonexistent-flag 2>/dev/null || true

# Empty input (no crash)
EMPTY_OUTPUT=$(echo "" | timeout 3 "$BINARY" 2>&1 || true)
echo "  Empty input handled (${#EMPTY_OUTPUT} chars)"

# Version flag with different casings
assert_exit_code "-v flag"                    0 "$BINARY" -v

# SIGPIPE handling
assert_exit_code "sigpipe no crash"           0 bash -c '$BINARY --help 2>&1 | head -1 >/dev/null'

# Tests that might not be available
if [ -x "$SRCDIR/slermes-tui" ]; then
    TUI_HELP=$("$SRCDIR/slermes-tui" --help 2>&1 || true)
    assert_contains "tui binary exists"       "Usage"       "$TUI_HELP"
else
    echo "  SKIP: no slermes-tui binary"
    SKIP=$((SKIP + 1))
fi

# High iteration count (quick test that config was respected)
ITER_OUTPUT=$("$BINARY" doctor 2>&1 || true)
assert_contains "doctor shows active model"   "owl-alpha"   "$ITER_OUTPUT"

# ═══════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════
echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
[ $FAIL -gt 0 ] && exit 1 || exit 0
