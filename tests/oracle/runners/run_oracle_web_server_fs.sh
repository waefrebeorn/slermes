#!/usr/bin/env bash
# run_oracle_web_server_fs.sh — /api/fs cluster oracle.
# Builds a real filesystem sandbox, substitutes @SBX@ into fixtures, and
# diffs the C harness against LIVE Python web_server.py logic per case.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_fs/cases_*.in
HARNESS=/tmp/tt_wsf
ORACLE=tests/sta_oracle_web_server_fs.py

# ── sandbox ────────────────────────────────────────────────────────────────
SBX=$(mktemp -d /tmp/wsf_sbx.XXXXXX)
mkdir -p "$SBX/proj/src" "$SBX/proj/node_modules" "$SBX/proj/.git" "$SBX/proj/docs"
printf 'hello world\nline two\n' > "$SBX/proj/readme.md"
printf '#include <stdio.h>\nint main(void){return 0;}\n' > "$SBX/proj/src/main.c"
printf '\x89PNG\r\n\x1a\n\x00\x00tinypng' > "$SBX/proj/logo.png"
printf 'key: value\n' > "$SBX/proj/config.yml"
printf 'binary\x00blob\x01\x02\x03' > "$SBX/proj/blob.bin"
# invalid UTF-8 in a text file (exercises errors="replace")
printf 'caf\xc3\xa9 ok, bad: \xff\xfe end\n' > "$SBX/proj/mixed.txt"

# ── link harness ───────────────────────────────────────────────────────────
LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_fs.c }
eval "$LINKCMD" 2>/tmp/wsf_link.log || { echo "LINK FAILED"; tail -5 /tmp/wsf_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness $HARNESS"; rm -rf "$SBX"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  echo "${line//@SBX@/$SBX}" > /tmp/wsf_case.json
  c_out=$("$HARNESS" /tmp/wsf_case.json 2>/dev/null)
  py_out=$(python3 "$ORACLE" /tmp/wsf_case.json 2>/dev/null)
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
rm -rf "$SBX"
echo "=== web_server_fs oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
