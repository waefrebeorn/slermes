#!/usr/bin/env bash
# run_watercup_json_escape.sh — language-agnostic water-cup deep-gate for the
# JSON-escape helper.
#
# Audits THREE translations of the same logic:
#   A = src/web_server.c :: json_escape_append   (control chars -> \u00XX)  [correct]
#   B = src/web_app.c     :: web_json_escape       (NO control-char escape)  [drifted]
#   PY= LIVE Python json.dumps                       (authoritative)
#
# The water-cup is language-agnostic: C11-vs-C11 and C11-vs-Python cups are
# compared purely from execution traces. Copy B has drifted from both A and
# LIVE Python (it dropped the <0x20 control-char escape branch) — a real FAP.
#
# Fail-closed: any cup mismatch => exit 1 => CI blocks the build.
set -euo pipefail
cd "$(dirname "$0")/../.."   # slermes root (script lives at tests/oracle/)
ROOT="$(pwd)"
WC="$ROOT/tests/watercup"
BUILD="$ROOT/tests/oracle/build"
mkdir -p "$BUILD"

echo "=== Water-Cup deep-gate: JSON-escape (C11 copy A vs B vs LIVE Python) ==="

if ! cc -I "$WC" "$WC/audit_json_escape.c" -o "$BUILD/audit_json_escape" 2>"$BUILD/watercup_je_build.log"; then
    echo "BUILD FAILED:"; cat "$BUILD/watercup_je_build.log"; exit 2
fi

if python3 "$WC/audit_json_escape_py.py" > "$BUILD/watercup_je_audit.log" 2>&1; then
    echo "WATER-CUP: JSON-escape cups MATCH across translations."
    cat "$BUILD/watercup_je_audit.log"
    exit 0
else
    echo "WATER-CUP: JSON-escape cup MISMATCH (FAP — drifted escape helper)."
    cat "$BUILD/watercup_je_audit.log"
    exit 1
fi
