#!/usr/bin/env bash
# run_one_oracle.sh <harness_basename> [oracle_py_basename]
# Compiles tests/t_port_<name>.c against the full project .o set and pipes
# stdout into tests/sta_oracle_<name>.py (or the provided oracle).
set -e
cd "$(dirname "$0")/.."
NAME="$1"
ORACLE="${2:-sta_oracle_$NAME.py}"
OBJSET=$(find src lib -name '*.o' ! -name 'main.o' | tr '\n' ' ')
TMPH=$(mktemp -d)
mkdir -p "$TMPH/.hermes/cron"
gcc -O2 -std=gnu11 -D_GNU_SOURCE -I include -I src/tools -I lib/libjson5 \
  "tests/t_port_$NAME.c" $OBJSET -o /tmp/tt_$NAME \
  -lstdc++ -lm -ldl -lpthread -lz -lpcre2-8 -lssl -lcrypto \
  -Wl,--allow-multiple-definition 2>&1 | grep -iE 'error|undefined' || true
SLERMES_HOME="$TMPH" HOME="$TMPH" /tmp/tt_$NAME 2>/dev/null | python3 "tests/$ORACLE"
RC=$?
rm -rf "$TMPH"
exit $RC
