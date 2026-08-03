#!/usr/bin/env bash
# oracle_web_server_status.sh — run the dashboard operational-backbone oracle.
# Builds temp state.db fixtures (substituting @NOW@ with a recent timestamp),
# runs the C harness and the Python oracle, diffs them.
set -uo pipefail
cd "$(dirname "$0")/../../.."
ROOT="$(pwd)"
FIXDIR="$ROOT/tests/oracle/fixtures/web_server_status"
CASES_GLOB="$FIXDIR"/cases_*.in
HARNESS="/tmp/tt_wss"
PYORACLE="$ROOT/tests/sta_oracle_web_server_status.py"
TMPD="$(mktemp -d /tmp/wss_oracle.XXXXXX)"

[ -x "$HARNESS" ] || { echo "MISSING harness $HARNESS"; exit 2; }
[ -f "$PYORACLE" ] || { echo "MISSING $PYORACLE"; exit 2; }
export PYTHONPATH="/home/wubu/.hermes/hermes-agent:${PYTHONPATH:-}"

# Build the C harness link using the real closure.
LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o \/tmp\/tt_wss }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_status.c }
eval "$LINKCMD" 2>/tmp/harness_wss.log || { echo "harness link FAILED"; tail -5 /tmp/harness_wss.log; exit 1; }

NOW=$(python3 -c "import time;print(time.time())")
n=0; failures=0; total=0
while IFS= read -r line; do
    [ -z "$line" ] && continue
    n=$((n+1))
    line="${line//@NOW@/$NOW}"
    fx="$TMPD/fx_$n.json"
    echo "$line" > "$fx"
    c_out="$TMPD/c_$n.json"; py_out="$TMPD/py_$n.json"
    # For active_sessions, build a real state.db for the C harness from the
    # same `sessions` the Python oracle uses, so both query identical data.
    DBHOME=""
    if echo "$line" | grep -q '"op":"active_sessions"'; then
        DBHOME="$TMPD/db_$n"
        python3 - "$DBHOME" "$NOW" "$fx" <<'PY'
import sys, os, sqlite3, json, time
home, now, fxpath = sys.argv[1], float(sys.argv[2]), sys.argv[3]
os.makedirs(home, exist_ok=True)
db = os.path.join(home, "state.db")
con = sqlite3.connect(db)
con.execute("""CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY, title TEXT, source TEXT, model TEXT,
    started_at REAL, last_active REAL, ended_at REAL,
    message_count INTEGER, input_tokens INTEGER)""")
sessions = json.load(open(fxpath)).get("sessions", [])
for s in sessions:
    sa = s.get("started_at", now - 100)
    la = s.get("last_active", sa)
    ea = s.get("ended_at", None)
    con.execute("INSERT INTO sessions VALUES (?,?,?,?,?,?,?,?,?)",
        (s["id"], s.get("title",""), s.get("source",""), s.get("model",""),
         sa, la, ea, s.get("message_count",0), s.get("input_tokens",0)))
con.commit(); con.close()
PY
        # Rewrite the fixture for C with the db path.
        python3 - "$fx" "$DBHOME" <<'PY'
import sys, json
fx, db = sys.argv[1], sys.argv[2]
o = json.load(open(fx)); o["db"] = db; json.dump(o, open(fx, "w"))
PY
    fi
    "$HARNESS" "$fx" > "$c_out" 2>/dev/null
    python3 "$PYORACLE" "$fx" > "$py_out" 2>/dev/null
    cs=$(tr -d ' \n' < "$c_out"); ps=$(tr -d ' \n' < "$py_out")
    if [ "$cs" = "$ps" ]; then
        echo "$line" >/dev/null
        echo "PASS case $n"
    else
        failures=$((failures+1))
        echo "FAIL case $n"
        echo "  C:  $(cat "$c_out")"
        echo "  PY: $(cat "$py_out")"
    fi
    total=$((total+1))
done < <(cat $CASES_GLOB)

rm -rf "$TMPD"
echo "=== web_server_status oracle: $total cases, $failures failures ==="
[ "$failures" -eq 0 ]

