#!/usr/bin/env bash
# run_oracle_web_server_events.sh — events/PTY support layer oracle.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_events/cases_*.in
HARNESS=/tmp/tt_wse
ORACLE=tests/sta_oracle_web_server_events.py

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_events.c }
eval "$LINKCMD" 2>/tmp/wse_link.log || { echo "LINK FAILED"; tail -5 /tmp/wse_link.log; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  printf '%s' "$line" > /tmp/wse_case.json
  c_out=$("$HARNESS" /tmp/wse_case.json 2>/dev/null | tr -d ' \n')
  py_out=$(python3 "$ORACLE" /tmp/wse_case.json 2>/dev/null | tr -d ' \n')
  if [ "$c_out" = "$py_out" ] && [ -n "$c_out" ]; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $c_out"
    echo "  Py: $py_out"
  fi
done < <(cat $FIX_GLOB)
echo "=== web_server_events oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
