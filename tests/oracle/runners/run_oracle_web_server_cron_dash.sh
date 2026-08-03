#!/usr/bin/env bash
# run_oracle_web_server_cron_dash.sh — dashboard cron adapter layer oracle.
# Builds a real profile sandbox with a scripts dir (files, subdir, escape).
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_cron_dash/cases_*.in
HARNESS=/tmp/tt_wscron
ORACLE=tests/sta_oracle_web_server_cron_dash.py

SBX=$(mktemp -d /tmp/wscron_sbx.XXXXXX)
export SLERMES_HOME="$SBX/home"
export HERMES_HOME="$SBX/home"
mkdir -p "$SBX/home/scripts/sub" "$SBX/home/scripts/adir" "$SBX/home/profiles/work"
echo '#!/bin/sh' > "$SBX/home/scripts/good.sh"
echo 'print(1)' > "$SBX/home/scripts/sub/nested.py"
echo '#!/bin/sh' > "$SBX/escape.sh"   # exists but OUTSIDE the sandbox

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_cron_dash.c -Ilib }
eval "$LINKCMD" 2>/tmp/wscron_link.log || { echo "LINK FAILED"; tail -5 /tmp/wscron_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  printf '%s' "${line//@SBX@/$SBX}" > /tmp/wscron_case.json
  "$HARNESS" /tmp/wscron_case.json > /tmp/wscron_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wscron_case.json > /tmp/wscron_py.out 2>/dev/null
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wscron_c.out"))
    p = json.load(open("/tmp/wscron_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(cat /tmp/wscron_c.out)"
    echo "  Py: $(cat /tmp/wscron_py.out)"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_server_cron_dash oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
