#!/usr/bin/env bash
# _oracle_one.sh - run exactly ONE oracle. Worker for run_all_oracles.sh.
#
# Arg: one tab-separated line "<name>\t<harness>\t<deps>"
#   name    - oracle discovery name (tests/oracle_<name>.py)
#   harness - harness basename (tests/t_port_<harness>.c); may differ from name
#   deps    - MINIMAL_OBJS object list (space separated)
#
# Taken as a single argument so xargs can dispatch with one bash spawn per
# oracle instead of wrapping it in `bash -c`.
#
# Prints exactly one line: "PASS <name>  (N cases, 0 mismatches)",
# "FAIL <name>  rc=N  ..." or "MISS <name> ...". Exit 0 iff PASS.
#
# Kept as a separate file because bash cannot export associative arrays
# (HARNESS_ALIAS / DEPS) to the subshells xargs spawns, so the parent resolves
# them and passes the results in as plain arguments.
set -u
cd "$(dirname "$0")/.." || exit 1

IFS=$'\t' read -r name harness deps libdir <<< "$1"

oracle="tests/oracle_$name.py"
harness_c="tests/t_port_${harness}.c"

if [ ! -f "$oracle" ]; then
  echo "MISS oracle_$name.py"
  exit 1
fi
if [ ! -f "$harness_c" ]; then
  echo "MISS t_port_${harness}.c (harness for $name)"
  exit 1
fi

out=$(MINIMAL_OBJS="$deps" bash tests/run_one_oracle.sh "$harness" "oracle_$name.py" "$libdir" 2>&1)
rc=$?

# One grep capturing the whole matching line. `|| true` keeps an empty result
# from tripping `set -u`.
line=$(printf '%s\n' "$out" | grep -E "ORACLE [A-Za-z0-9_]+: [0-9]+ cases, 0 mismatches" | head -1 || true)

if [ $rc -eq 0 ] && [ -n "$line" ]; then
  echo "PASS $name  (${line#ORACLE *: })"
  exit 0
fi

echo "FAIL $name  rc=$rc  ${line:-no ORACLE line}"
exit 1
