#!/usr/bin/env bash
# oracle_cron_scheduler_orche.sh — run the cron scheduler orchestration oracle.
#
# For each fixture in tests/oracle/fixtures/cron_scheduler_orche/cases.in:
#   - expand "__SCRIPT__"-style no_agent bodies into real temp script files
#   - run the C harness  -> /tmp/orc_c_<n>.json
#   - run the Python oracle -> /tmp/orc_py_<n>.json
#   - diff (structural: Run Time line already stripped by both sides)
# Exit 1 on any mismatch.
set -uo pipefail
cd "$(dirname "$0")/../../.."   # slermes root
ROOT="$(pwd)"
FIXDIR="$ROOT/tests/oracle/fixtures/cron_scheduler_orche"
CASES="$FIXDIR/cases.in"
HARNESS="/tmp/tt_cronorchestr"
PYORACLE="$ROOT/tests/sta_oracle_cron_scheduler_orchestr.py"
TMPD="$(mktemp -d /tmp/cronoracle.XXXXXX)"

[ -x "$HARNESS" ] || { echo "MISSING harness $HARNESS (build it first)"; exit 2; }
[ -f "$PYORACLE" ] || { echo "MISSING $PYORACLE"; exit 2; }

# Ensure the oracle python can import cron.scheduler
export PYTHONPATH="/home/wubu/.hermes/hermes-agent:${PYTHONPATH:-}"

n=0; failures=0; total=0
while IFS= read -r line; do
    [ -z "$line" ] && continue
    n=$((n+1))
    # Build a per-case fixture file, expanding no_agent script_body into a
    # real temp script under a temp HERMES_HOME.
    fx="$TMPD/fx_$n.json"
    hm="$TMPD/hm_$n"; scripts="$hm/scripts"; mkdir -p "$scripts"
    export HERMES_HOME="$hm"
    # rewrite script_body -> script (real path) for this case
    FX="$fx" SCRIPTS="$scripts" python3 - <<'PY'
import sys, json, os
line = sys.stdin.read().strip()
fx = os.environ["FX"]; scripts = os.environ["SCRIPTS"]
obj = json.loads(line)
if obj.get("op") == "no_agent" and "script_body" in obj:
    p = os.path.join(scripts, "job.sh")
    with open(p, "w") as fh:
        fh.write(obj.pop("script_body") + "\n")
    os.chmod(p, 0o755)
    obj["script"] = p
with open(fx, "w") as fh:
    json.dump(obj, fh)
PY
    total=$((total+1))
    c_out="$TMPD/c_$n.json"; py_out="$TMPD/py_$n.json"
    "$HARNESS" "$fx" > "$c_out" 2>/dev/null
    python3 "$PYORACLE" "$fx" > "$py_out" 2>/dev/null
    op="$(echo "$line" | python3 -c 'import sys,json; print(json.loads(sys.stdin.read()).get("op",""))')"
    if diff -q "$c_out" "$py_out" >/dev/null; then
        echo "PASS case $n ($op)"
    else
        failures=$((failures+1))
        echo "FAIL case $n"
        echo "  C:   $(cat "$c_out")"
        echo "  PY:  $(cat "$py_out")"
    fi
done < "$CASES"

rm -rf "$TMPD"
echo "=== cron scheduler orchestration oracle: $total cases, $failures failures ==="
[ "$failures" -eq 0 ]
