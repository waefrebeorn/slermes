#!/usr/bin/env bash
# run_oracle_web_server_prune.sh — prune engine oracle. Seeds a REAL session
# DB with rich attribute coverage via the REAL SessionDB class.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_prune/cases_*.in
HARNESS=/tmp/tt_wsprune
ORACLE=tests/sta_oracle_web_server_prune.py

SBX=$(mktemp -d /tmp/wsprune_sbx.XXXXXX)
DB="$SBX/state.db"
python3 - "$DB" <<'EOF'
import sys, sqlite3
from pathlib import Path
sys.path.insert(0, str(Path.home() / ".hermes/hermes-agent"))
from hermes_state import SessionDB

db = SessionDB(Path(sys.argv[1]))

def mk(sid, source, msgs, started, *, ended=True, archived=False, parent=None,
       title=None, model=None, provider=None, user_id=None, chat_id=None,
       chat_type=None, branch=None, cwd=None, itok=None, otok=None,
       actual=None, est=None, tools=None, end_reason="completed",
       msg_times=None):
    db.create_session(sid, source, parent_session_id=parent)
    for i in range(msgs):
        db.append_message(sid, "user" if i % 2 == 0 else "assistant", f"m{i}",
                          timestamp=(msg_times[i] if msg_times else started + 10 * (i + 1)))
    if ended:
        db.end_session(sid, end_reason=end_reason)
    sets, vals = ["started_at = ?"], [started]
    for col, v in [("title", title), ("model", model),
                   ("billing_provider", provider), ("user_id", user_id),
                   ("chat_id", chat_id), ("chat_type", chat_type),
                   ("git_branch", branch), ("cwd", cwd),
                   ("input_tokens", itok), ("output_tokens", otok),
                   ("actual_cost_usd", actual), ("estimated_cost_usd", est),
                   ("tool_call_count", tools)]:
        if v is not None:
            sets.append(f"{col} = ?"); vals.append(v)
    if archived:
        sets.append("archived = 1")
    vals.append(sid)
    db._conn.execute(f"UPDATE sessions SET {', '.join(sets)} WHERE id = ?", vals)
    db._conn.commit()

T = 1600000000
mk("s_old_cli", "cli", 2, T, title="Fix the BUG in parser", model="claude-sonnet-4",
   provider="openrouter", cwd="/home/u/proj/sub", itok=100, otok=80,
   actual=0.75, branch="feature/fix-bug", tools=5,
   msg_times=[T + 100, T + 200])
mk("s_old_tg", "telegram", 3, T + 1000, user_id="u1", chat_id="c9",
   chat_type="group", model="gpt-4o", est=0.30, tools=2, itok=40, otok=20,
   msg_times=[T + 1100, T + 1200, T + 1300])
mk("s_recent", "cli", 1, 9e9, model="claude-opus", cwd="/home/u/proj",
   msg_times=[9e9 + 50])   # far future → excluded by any older_than cutoff
mk("s_nomsg", "gateway", 0, T + 2000, end_reason="error", est=0.05)
mk("s_arch", "cli", 2, T + 3000, archived=True, title="bug report archive",
   itok=2000, otok=1500, actual=2.5, msg_times=[T + 3100, T + 3200])
mk("s_live", "cli", 1, T + 4000, ended=False, msg_times=[T + 4100])
mk("s_child", "cli", 1, T + 5000, parent="s_old_cli", branch="feat/child",
   msg_times=[T + 5100])
mk("s_win", "discord", 4, 1400000000, title="ancient win",
   msg_times=[1400000100, 1400000200, 1400000300, 1400000400])
db.close()
EOF
[ $? -eq 0 ] || { echo "SEED FAILED"; rm -rf "$SBX"; exit 1; }

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_prune.c -Ilib }
eval "$LINKCMD" 2>/tmp/wsprune_link.log || { echo "LINK FAILED"; tail -5 /tmp/wsprune_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

cp "$DB" "$SBX/mut_c.db";  cp "$DB" "$SBX/mut_py.db"
cp "$DB" "$SBX/mut2_c.db"; cp "$DB" "$SBX/mut2_py.db"

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  c_line="${line//@DBMUT2@/$SBX/mut2_c.db}";  c_line="${c_line//@DBMUT@/$SBX/mut_c.db}";  c_line="${c_line//@DB@/$DB}"
  p_line="${line//@DBMUT2@/$SBX/mut2_py.db}"; p_line="${p_line//@DBMUT@/$SBX/mut_py.db}"; p_line="${p_line//@DB@/$DB}"
  printf '%s' "$c_line" > /tmp/wsprune_case_c.json
  printf '%s' "$p_line" > /tmp/wsprune_case_py.json
  "$HARNESS" /tmp/wsprune_case_c.json > /tmp/wsprune_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wsprune_case_py.json > /tmp/wsprune_py.out 2>/dev/null
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wsprune_c.out"))
    p = json.load(open("/tmp/wsprune_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(cat /tmp/wsprune_c.out)"
    echo "  Py: $(cat /tmp/wsprune_py.out)"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_server_prune oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
