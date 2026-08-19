#!/usr/bin/env bash
# run_watercup.sh — water-cup deep-gate oracle for the WEB auth gate.
#
# This is the production CI deep-gate the design doc calls for: it builds the
# instrumented C audit harness (audit_web_auth.c, traced with watercup.h) and
# runs it against LIVE Python's real control flow (hermes_cli/dashboard_auth/
# public_paths.py + hermes_cli/web_server.py), then FAILS (exit 1) on ANY cup
# mismatch.
#
# Why a deep gate and not the diff oracle: the cup is carved from EXECUTION, not
# the declared return. A stub that `return json_object()` or a wrong constant
# set cannot reproduce the real algorithm's trace, so the output-diff oracle
# would pass it while the water-cup catches it. This closes the "PORTED but
# missing from index / constant-return" cheat surface for the web subsystem.
#
# Registered in tests/oracle/registry.json under "watercup_web_auth" so
# run_oracles.sh picks it up automatically (it is dispatched here because the
# standard diff runner cannot express a cup-hash verdict).
set -euo pipefail
cd "$(dirname "$0")/../.."   # slermes root (run_watercup.sh is at tests/oracle/, 2 deep)
ROOT="$(pwd)"
WC="$ROOT/tests/watercup"
BUILD="$ROOT/tests/oracle/build"
mkdir -p "$BUILD"

# LIVE Python source of truth lives in the parent dev tree (hermes-agent-dev).
DEV_ROOT="$(cd "$ROOT/.." && pwd)"
echo "=== Water-Cup deep-gate: WEB auth gate (C compiled vs LIVE Python) ==="
echo "    C harness : $WC/audit_web_auth.c"
echo "    Py twin   : $DEV_ROOT/hermes_cli/dashboard_auth/public_paths.py + web_server.py"

# Build the C audit harness (instrumented with watercup.h).
if ! cc -I "$WC" "$WC/audit_web_auth.c" -o "$BUILD/audit_web_auth" 2>"$BUILD/watercup_build.log"; then
    echo "BUILD FAILED:"; cat "$BUILD/watercup_build.log"; exit 2
fi

# Run the cross-language cup audit. audit_web_auth_py.py imports the LIVE
# Python source and compares cups; exit code 0 == all cups match.
if env PYTHONPATH="$DEV_ROOT:$DEV_ROOT/hermes_cli" python3 "$WC/audit_web_auth_py.py" > "$BUILD/watercup_audit.log" 2>&1; then
    echo "WATER-CUP: all web-auth cups MATCH LIVE Python."
    cat "$BUILD/watercup_audit.log"
    exit 0
else
    echo "WATER-CUP: MISMATCH detected (FAP in web auth gate)."
    cat "$BUILD/watercup_audit.log"
    exit 1
fi
