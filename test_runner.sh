#!/bin/bash
# test_runner.sh — Slermes C Test Suite Runner
# Run: bash test_runner.sh [--verbose] [--build-only]
set -e

SRCDIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
ARGS=()

# Parse args
for arg in "$@"; do
    case "$arg" in
        --verbose|-v) ARGS+=("--verbose") ;;
        --build-only) ARGS+=("--build-only") ;;
        --help|-h) echo "Usage: $0 [--verbose] [--build-only]"; exit 0 ;;
    esac
done

echo "=== Slermes C Test Suite ==="
echo ""

# ── Phase 1: Smoke tests (fast checks) ──
echo "--- Phase 1: Smoke Tests ---"
run_smoke() {
    local name="$1"
    shift
    echo -n "  TEST: $name ... "
    if "$@" >/dev/null 2>&1; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
    fi
}

run_smoke "binary exists"          test -x "$SRCDIR/slermes"
run_smoke "help output"            bash -c './slermes --help 2>&1 | grep -q Usage'
run_smoke "version output"         bash -c './slermes --version 2>&1 | grep -q slermes'
run_smoke "no crash on --help"     bash -c './slermes --help >/dev/null 2>&1'

# ── Phase 2: Functional tests (runs through CLI) ──
echo ""
echo "--- Phase 2: Functional Tests ---"
if [ -z "$SKIP_FUNC" ]; then
    bash "$SRCDIR/functional_tests.sh" "${ARGS[@]}"
else
    echo "  SKIP (SKIP_FUNC set)"
fi

# ── Summary ──
echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
if [ $FAIL -gt 0 ]; then
    exit 1
fi
