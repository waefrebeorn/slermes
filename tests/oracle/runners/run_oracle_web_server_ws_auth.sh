#!/usr/bin/env bash
# run_oracle_web_server_ws_auth.sh — WS upgrade gate family oracle.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_ws_auth/cases_*.in
HARNESS=/tmp/tt_wsauth
ORACLE=tests/sta_oracle_web_server_ws_auth.py

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_ws_auth.c -Ilib }
eval "$LINKCMD" 2>/tmp/wsauth_link.log || { echo "LINK FAILED"; tail -5 /tmp/wsauth_link.log; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  printf '%s' "$line" > /tmp/wsauth_case.json
  "$HARNESS" /tmp/wsauth_case.json > /tmp/wsauth_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wsauth_case.json > /tmp/wsauth_py.out 2>/dev/null
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wsauth_c.out"))
    p = json.load(open("/tmp/wsauth_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(cat /tmp/wsauth_c.out)"
    echo "  Py: $(cat /tmp/wsauth_py.out)"
  fi
done < <(cat $FIX_GLOB)
echo "=== web_server_ws_auth oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
