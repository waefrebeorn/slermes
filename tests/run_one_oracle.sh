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
# Pick an interpreter with a working hashlib: the MSYS2 /mingw64 python3 that
# PATH resolves to first has a broken OpenSSL binding (blake2b/blake2s raise at
# import), which would make a sha256 oracle fail for an unrelated reason.
pick_python() {
  for cand in "${PYTHON_BIN:-}" python python3 py \
              /c/Users/*/AppData/Local/hermes/tools/python-*/python.exe; do
    [ -n "$cand" ] || continue
    if { command -v "$cand" >/dev/null 2>&1 || [ -x "$cand" ]; } &&
       "$cand" -c "import hashlib,json; hashlib.sha256(b'x').hexdigest()" >/dev/null 2>&1; then
      printf '%s\n' "$cand"
      return 0
    fi
  done
  return 1
}
PYBIN="$(pick_python)" || { echo "ORACLE $NAME: no python with working hashlib" >&2; exit 1; }
# A link failure must be fatal — piping gcc through `grep ... || true` let a
# broken link reach the oracle as "0 cases".
gcc -O2 -std=gnu11 -D_GNU_SOURCE -I include -I src $LIBINCS \
  "tests/t_port_$NAME.c" "@$OBJSET" -o ".testbin/tt_$NAME" \
  -lstdc++ -lm $EXTRA_LIBS $WIN_LIBS -lz -lpcre2-8 -ltre -lssl -lcrypto \
  -Wl,--allow-multiple-definition
SLERMES_HOME="$TMPH" HOME="$TMPH" "./.testbin/tt_$NAME" 2>/dev/null | "$PYBIN" "tests/$ORACLE"
RC=$?
rm -rf "$TMPH"
exit $RC
