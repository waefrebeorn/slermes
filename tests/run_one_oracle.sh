#!/usr/bin/env bash
# run_one_oracle.sh <harness_basename> [oracle_py_basename]
# Compiles tests/t_port_<name>.c against the full project .o set and pipes
# stdout into tests/sta_oracle_<name>.py (or the provided oracle).
set -e
cd "$(dirname "$0")/.."
NAME="$1"
ORACLE="${2:-sta_oracle_$NAME.py}"
LIBINCS=$(grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk | sed 's#^#-I #' | tr '\n' ' ')
# Windows/MinGW: the inline object list overflows the command line (gcc then
# silently drops the tail), /tmp is a POSIX path a native gcc.exe cannot
# resolve, and there is no libdl/libpthread. Use a response file + .testbin.
mkdir -p .testbin
OBJSET=.testbin/oracle_objs.rsp
find src lib -name '*.o' ! -name 'main.o' | tr '\n' ' ' > "$OBJSET"
echo >> "$OBJSET"
EXTRA_LIBS=""
WIN_LIBS=""
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    WIN_LIBS="-lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 -lole32 -luuid -lSDL2main -lSDL2 -lSDL2_ttf"
    ;;
  *) EXTRA_LIBS="-ldl -lpthread" ;;
esac
TMPH="$(pwd)/.testbin/oracle_tmp_$NAME"
mkdir -p "$TMPH/.hermes/cron"
# A link failure must be fatal — piping gcc through `grep ... || true` let a
# broken link reach the oracle as "0 cases".
gcc -O2 -std=gnu11 -D_GNU_SOURCE -I include -I src $LIBINCS \
  "tests/t_port_$NAME.c" "@$OBJSET" -o ".testbin/tt_$NAME" \
  -lstdc++ -lm $EXTRA_LIBS $WIN_LIBS -lz -lpcre2-8 -ltre -lssl -lcrypto \
  -Wl,--allow-multiple-definition
SLERMES_HOME="$TMPH" HOME="$TMPH" "./.testbin/tt_$NAME" 2>/dev/null | python3 "tests/$ORACLE"
RC=$?
rm -rf "$TMPH"
exit $RC
