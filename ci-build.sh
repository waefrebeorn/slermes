#!/usr/bin/env bash
# ── ci-build.sh — CI build script for Slermes ─────────────────────────
# Works around GitHub Actions Make 4.3 pattern-rule resolution bug where
# the default target chain skips pattern rules.
#
# Usage: CC=gcc [AR=ar] [MAKE_OPTS="..."] [BUILD_DIR=.] ./ci-build.sh
#
# Strategy: extract all compilation targets from `make -n all`, then compile
# each .o target explicitly via make (single-target pattern rules work fine).
# Finally run the link step.

set -euo pipefail

: "${BUILD_DIR:=.}"
: "${CC:=gcc}"
: "${AR:=ar}"
MAKE_OPTS="${MAKE_OPTS:-} CC=$CC AR=$AR"

cd "$BUILD_DIR"

# ── 1. Extract build plan ──────────────────────────────────────────
echo "=== ci-build.sh: CC=$CC AR=$AR ==="
echo "--- Extracting object targets from Makefile ---"

# Get all object files that 'slermes' depends on (the full chain)
eval make -n -B all $MAKE_OPTS 2>&1 > /tmp/make_plan.txt || true

# Extract all .o targets (compilation commands have -o target.o source.c pattern)
echo "--- Compiling objects via make (single targets) ---"
grep -oE '\-o [a-zA-Z_/.-]+\.o ' /tmp/make_plan.txt 2>/dev/null | sed 's/-o //' | tr -d ' ' | sort -u > /tmp/all_targets.txt
NUM_TARGETS=$(wc -l < /tmp/all_targets.txt)
echo "  Total .o targets: $NUM_TARGETS"

if [ "$NUM_TARGETS" -eq 0 ]; then
    echo "  WARNING: No targets found — falling back to plain make"
    make all $MAKE_OPTS 2>&1
    ./slermes --version
    exit 0
fi

# Compile each target individually
ERRORS=0
while IFS= read -r target; do
    [ -f "$target" ] && echo "  SKIP $target (exists)" && continue
    echo "  MAKE $target"
    if ! make "$target" $MAKE_OPTS 2>&1; then
        echo "  FAILED: $target"
        ERRORS=$((ERRORS+1))
    fi
done < /tmp/all_targets.txt

if [ "$ERRORS" -gt 0 ]; then
    echo "=== COMPILATION FAILED: $ERRORS errors ==="
    exit 1
fi

echo "  All objects compiled successfully"

# ── 2. Link ────────────────────────────────────────────────────────
echo "--- Linking slermes ---"
eval make slermes $MAKE_OPTS 2>&1

if [ -f slermes ]; then
    echo "=== Build complete: $(ls -lh slermes | awk '{print $5}') ===  CC=$CC"
    ./slermes --version
elif [ -f slermes.exe ]; then
    echo "=== Build complete: $(ls -lh slermes.exe | awk '{print $5}') ===  CC=$CC"
    file slermes.exe
else
    echo "=== LINK FAILED ==="
    exit 1
fi
