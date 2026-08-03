#!/usr/bin/env bash
# run_oracle_web_server_managed.sh — managed-files security cluster oracle.
# Diffs the C harness (real object closure) against LIVE Python
# hermes_cli/web_server.py per fixture case.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_managed/cases_*.in
HARNESS=/tmp/tt_wsm
ORACLE=tests/sta_oracle_web_server_managed.py

# Link the harness against the full slermes object closure (swap main.o).
LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_managed.c }
eval "$LINKCMD" 2>/tmp/wsm_link.log || { echo "LINK FAILED"; tail -5 /tmp/wsm_link.log; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness $HARNESS"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  echo "$line" > /tmp/wsm_case.json
  c_out=$("$HARNESS" /tmp/wsm_case.json 2>/dev/null)
  py_out=$(python3 "$ORACLE" /tmp/wsm_case.json 2>/dev/null)
  # normalize whitespace for diff robustness
  c_n=$(echo "$c_out" | tr -d ' \n')
  p_n=$(echo "$py_out" | tr -d ' \n')
  if [ "$c_n" = "$p_n" ]; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $c_out"
    echo "  Py: $py_out"
  fi
done < <(cat $FIX_GLOB)
echo "=== web_server_managed oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
