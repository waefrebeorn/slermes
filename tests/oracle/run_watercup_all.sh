#!/usr/bin/env bash
# run_watercup_all.sh — PROJECT-WIDE water-cup parity gate.
#
# Runs every gate under tests/watercup/gates/ through the generic engine
# (wc_run.py), which builds each traced C harness, imports the LIVE Python twin
# for the same function, and diffs the carved cups. Any cup mismatch (a FAP —
# real or stub — across ANY translation to C11) fails the build.
#
# This is the contract-C deep gate layered on top of the diff oracle: a port is
# parity-closed only when BOTH the output-diff oracle AND the water-cup say
# MATCH. The water-cup is language-agnostic — it catches wrong-path ports that
# return the right shape but execute different control flow.
#
# Registered as the "watercup_all" port in tests/oracle/registry.json and
# dispatched by run_oracles.sh, so it runs automatically in CI alongside the
# diff oracle.
set -euo pipefail
cd "$(dirname "$0")/../.."   # slermes root (script lives at tests/oracle/)
ROOT="$(pwd)"
WC="$ROOT/tests/watercup"
BUILD="$ROOT/tests/oracle/build"
mkdir -p "$BUILD"

echo "=== Water-Cup project-wide parity gate (all gates) ==="
if python3 "$WC/wc_run.py" | tee "$BUILD/watercup_all.log"; then
    # wc_run.py returns 0 only when ALL gates match. tee passes the pipeline rc.
    if [ "${PIPESTATUS[0]}" -eq 0 ]; then
        echo "WATER-CUP: all gates MATCH LIVE Python across translations."
        exit 0
    fi
fi
echo "WATER-CUP: cup MISMATCH detected (FAP in a translated function)."
exit 1
