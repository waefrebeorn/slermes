#!/usr/bin/env bash
# run_oracle_web_server_chat_argv.sh — chat PTY env + descendant resolution.
# Builds a REAL sqlite session tree + a real profiles sandbox.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_chat_argv/cases_*.in
HARNESS=/tmp/tt_wschat
ORACLE=tests/sta_oracle_web_server_chat_argv.py

SBX=$(mktemp -d /tmp/wschat_sbx.XXXXXX)
export SLERMES_HOME="$SBX/home"
export HERMES_HOME="$SBX/home"
mkdir -p "$SBX/home/profiles/work"

DB="$SBX/sessions.db"
python3 - "$DB" <<'EOF'
import sqlite3, sys
conn = sqlite3.connect(sys.argv[1])
conn.execute("""CREATE TABLE sessions (
    id TEXT PRIMARY KEY, parent_session_id TEXT, started_at REAL)""")
rows = [
    # root A with two children; child c has its own child d (newest leaf)
    ("20260101_000000_aaaaaa", None,                     1000.0),
    ("20260101_010000_bbbbbb", "20260101_000000_aaaaaa", 1100.0),
    ("20260101_020000_cccccc", "20260101_000000_aaaaaa", 1200.0),
    ("20260102_000000_dddddd", "20260101_020000_cccccc", 2000.0),
    # isolated root E (no children)
    ("20260103_000000_eeeeee", None,                     3000.0),
    # cycle: f <-> g (guards the seen-set)
    ("20260104_000000_ffffff", "20260104_000000_gggggg", 4000.0),
    ("20260104_000000_gggggg", "20260104_000000_ffffff", 4100.0),
    # ambiguous prefix pair sharing "20260103" is NOT ambiguous with eeeeee?
    ("20260103_120000_hhhhhh", None,                     3500.0),
]
conn.executemany("INSERT INTO sessions VALUES (?,?,?)", rows)
conn.commit()
EOF

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_chat_argv.c -Ilib }
eval "$LINKCMD" 2>/tmp/wschat_link.log || { echo "LINK FAILED"; tail -5 /tmp/wschat_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  case "$line" in
    *'"op": "profile_dir"'*|*'"op":"profile_dir"'*) line="${line%\}},\"home\":\"$SBX/home\"}" ;;
  esac
  printf '%s' "${line//@DB@/$DB}" > /tmp/wschat_case.json
  "$HARNESS" /tmp/wschat_case.json > /tmp/wschat_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wschat_case.json > /tmp/wschat_py.out 2>/dev/null
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wschat_c.out"))
    p = json.load(open("/tmp/wschat_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(cat /tmp/wschat_c.out)"
    echo "  Py: $(cat /tmp/wschat_py.out)"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_server_chat_argv oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
