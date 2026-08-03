#!/usr/bin/env bash
# run_oracle_web_server_sessions_admin.sh — sessions admin cluster oracle.
# Seeds a REAL session DB via the REAL SessionDB class (schema + rows).
# @DB@ = shared read-only DB; @DBMUT@ = per-side mutable copy (C and Python
# each mutate their own copy; outputs must match).
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_sessions_admin/cases_*.in
HARNESS=/tmp/tt_wssess
ORACLE=tests/sta_oracle_web_server_sessions_admin.py

SBX=$(mktemp -d /tmp/wssess_sbx.XXXXXX)
DB="$SBX/state.db"
python3 - "$DB" <<'EOF'
import sys, time
from pathlib import Path
sys.path.insert(0, str(Path.home() / ".hermes/hermes-agent"))
from hermes_state import SessionDB

db = SessionDB(Path(sys.argv[1]))
now = time.time()

def mk(sid, source, msgs=0, ended=True, archived=False, parent=None, **kw):
    db.create_session(sid, source, parent_session_id=parent, **kw)
    for i in range(msgs):
        db.append_message(sid, "user" if i % 2 == 0 else "assistant", f"m{i}")
    if ended:
        db.end_session(sid, end_reason=kw.get("end_reason_val", "completed"))
    if archived:
        db._conn.execute("UPDATE sessions SET archived = 1 WHERE id = ?", (sid,))
        db._conn.commit()

mk("s_root", "cli", msgs=4)
mk("s_tg", "telegram", msgs=2)
mk("s_empty1", "cli", msgs=0)               # empty, ended → deletable
mk("s_empty2", "gateway", msgs=0)           # empty, ended → deletable
mk("s_empty_live", "cli", msgs=0, ended=False)   # live → protected
mk("s_empty_arch", "cli", msgs=0, archived=True) # archived → protected
mk("s_child", "cli", msgs=1, parent="s_empty1")  # child of deletable parent
mk("s_sub", "cli", msgs=3, parent="s_root")      # non-branch child (hidden)
mk("s_arch_full", "discord", msgs=5, archived=True)
mk("s_nosrc", "", msgs=1)                        # falsy source → "cli" bucket
db.close()
EOF
[ $? -eq 0 ] || { echo "SEED FAILED"; rm -rf "$SBX"; exit 1; }

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_sessions_admin.c -Ilib }
eval "$LINKCMD" 2>/tmp/wssess_link.log || { echo "LINK FAILED"; tail -5 /tmp/wssess_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

# Per-side mutable copies (delete_empty mutates; each side gets its own).
cp "$DB" "$SBX/mut_c.db"
cp "$DB" "$SBX/mut_py.db"

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  c_line="${line//@DBMUT@/$SBX/mut_c.db}";  c_line="${c_line//@DB@/$DB}"
  p_line="${line//@DBMUT@/$SBX/mut_py.db}"; p_line="${p_line//@DB@/$DB}"
  printf '%s' "$c_line" > /tmp/wssess_case_c.json
  printf '%s' "$p_line" > /tmp/wssess_case_py.json
  "$HARNESS" /tmp/wssess_case_c.json > /tmp/wssess_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wssess_case_py.json > /tmp/wssess_py.out 2>/dev/null
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wssess_c.out"))
    p = json.load(open("/tmp/wssess_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(cat /tmp/wssess_c.out)"
    echo "  Py: $(cat /tmp/wssess_py.out)"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_server_sessions_admin oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
