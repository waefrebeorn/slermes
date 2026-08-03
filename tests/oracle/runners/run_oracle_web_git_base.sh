#!/usr/bin/env bash
# run_oracle_web_git_base.sh — base_branch_list + review_list sort parity.
# Builds a REAL git repo sandbox, diffs C vs live Python hermes_cli.web_git.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_git_base/cases_*.in
HARNESS=/tmp/tt_wgb
ORACLE=tests/sta_oracle_web_git_base.py

SBX=$(mktemp -d /tmp/wgb_sbx.XXXXXX)
mkdir -p "$SBX/repo" "$SBX/norepo"
(
  cd "$SBX/repo"
  git init -q -b main
  git -c user.email=t@t -c user.name=T commit -q --allow-empty -m init
  git branch feature/one
  git branch fix-two
  # dirty state for review_list: a modified tracked file + untracked files
  printf 'a\nb\nc\n' > tracked.txt
  git add tracked.txt
  git -c user.email=t@t -c user.name=T commit -q -m add-tracked
  printf 'a\nCHANGED\nc\nd\n' > tracked.txt
  printf 'x1\nx2\n' > zeta-untracked.txt
  printf 'no trailing newline' > alpha-untracked.txt
)

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_git_base.c -Isrc\/cli -Ilib }
eval "$LINKCMD" 2>/tmp/wgb_link.log || { echo "LINK FAILED"; tail -5 /tmp/wgb_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  echo "${line//@SBX@/$SBX}" > /tmp/wgb_case.json
  c_out=$("$HARNESS" /tmp/wgb_case.json 2>/dev/null | tr -d ' \n')
  py_out=$(python3 "$ORACLE" /tmp/wgb_case.json 2>/dev/null | tr -d ' \n')
  if [ "$c_out" = "$py_out" ]; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $c_out"
    echo "  Py: $py_out"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_git_base oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
