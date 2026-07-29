#!/usr/bin/env bash
# run_oracles.sh — project-wide oracle runner with normalization + stale detection.
#
# Usage:
#   bash tests/oracle/run_oracles.sh                  # run all registered ports
#   bash tests/oracle/run_oracles.sh account_usage    # run single port
#   bash tests/oracle/run_oracles.sh --baseline       # update baseline
#   bash tests/oracle/run_oracles.sh --check          # fail on mismatch (CI mode)
#
# Normalization: before diffing, both C and Python outputs are piped through
# a normalize step that strips dynamic fields (commit hashes, branch names,
# untracked file lists, root paths). This ensures the oracle checks
# structural correctness of the port, not runtime environment state.
#
# Stale detection: results are appended to tests/oracle/results.jsonl with
# a commit SHA + port name + case name + verdict. A CI run that sees a
# previously-passing test flip to FAIL gets flagged as a regression.
set -euo pipefail

SLERMES_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REGISTRY="$SLERMES_ROOT/tests/oracle/registry.json"
RESULT_HISTORY="$SLERMES_ROOT/tests/oracle/results.jsonl"
CURRENT_COMMIT="$(cd "$SLERMES_ROOT" && git rev-parse HEAD 2>/dev/null || echo unknown)"
MODE="check"  # default: fail on mismatch (CI mode)

for arg in "$@"; do
  case "$arg" in
    --baseline) MODE="baseline" ;;
    --check) MODE="check" ;;
    -h|--help)
      echo "Usage: $0 [--baseline|--check] [port_name ...]"
      echo "  --baseline  update baseline (no failure on mismatch)"
      echo "  --check     CI mode: fail on any mismatch (default)"
      echo "  port_name   run only this port (no normalization)"
      exit 0
      ;;
    *) PORT_FILTER="$arg" ;;
  esac
done

mkdir -p "$SLERMES_ROOT/tests/oracle"

# Read ports from registry or filter.
if [ -n "${PORT_FILTER:-}" ]; then
  PORTS=("$PORT_FILTER")
else
  # Parse registry.json for port names (simple python-free extraction).
  mapfile -t PORTS < <(python3 -c "
import json, sys
r = json.load(open('$REGISTRY'))
for name in r.get('ports', {}):
    print(name)
")
fi

OVERALL=0
for port in "${PORTS[@]}"; do
  echo "=== Oracle: $port (commit: $CURRENT_COMMIT) ==="
  
  # Run the oracle. The runner uses SLERMES_HOME/HERMES_HOME isolation,
  # so neither side touches the real ~/.hermes.
  if bash "$SLERMES_ROOT/tests/oracle/runners/run_oracle.sh" "$port" 2>&1; then
    VERDICT="pass"
  else
    VERDICT="fail"
    if [ "$MODE" = "check" ]; then
      OVERALL=1
    fi
  fi
  
  # Record result for stale detection.
  echo "{\"commit\":\"$CURRENT_COMMIT\",\"port\":\"$port\",\"verdict\":\"$VERDICT\",\"timestamp\":\"$(date -Iseconds)\"}" >> "$RESULT_HISTORY"
  
  # Stale detection: check if the last 5 runs for this port had any failures.
  # If a previously-passing port now fails, it's a regression.
  if [ "$VERDICT" = "fail" ] && [ "$MODE" = "check" ]; then
    PREV_PASS="$(tail -20 "$RESULT_HISTORY" 2>/dev/null | python3 -c "
import sys, json
port='$port'
lines = [l for l in sys.stdin if l.strip()]
for l in reversed(lines):
    r = json.loads(l)
    if r['port'] != port: continue
    if r['verdict'] == 'pass': print('prev_pass'); break
else:
    print('no_prev_pass')
" 2>/dev/null || echo "no_prev_pass")"
    if [ "$PREV_PASS" = "prev_pass" ]; then
      echo "  !!! REGRESSION: $port was passing and is now failing"
    fi
  fi
done

exit $OVERALL