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
OBJARG="${MINIMAL_OBJS:-@$OBJSET}"
# Use the repo-local ccache wrapper when present (sets MSYS env vars + pins
# the cache to <repo>/.ccache). Subsequent identical harness compiles of the
# SAME port .c hash-hit, so re-gating a port is near-instant even when the
# harness was rebuilt for a different object set.
COMPILER="gcc"
if [ -x "./.ccache/launch.sh" ]; then
  COMPILER="./.ccache/launch.sh gcc"
fi
build() {   # build <out> <opt>
  $COMPILER "$2" -std=gnu11 -D_GNU_SOURCE -I include -I src $LIBINCS \
    "tests/t_port_$NAME.c" $OBJARG -o "$1" \
    -lstdc++ -lm $EXTRA_LIBS $WIN_LIBS -lz -lpcre2-8 -ltre -lssl -lcrypto \
    -Wl,--allow-multiple-definition
}
OUT=".testbin/tt_$NAME"
build "$OUT" -O2

# Smart App Control refuses some freshly linked binaries outright (rc 126) and
# only PowerShell names the reason. A relink with an extra optimization level
# produces a different image, which it lets through — retry once that way
# rather than reporting a false "0 cases".
SLERMES_HOME="$TMPH" HOME="$TMPH" "$OUT" > "$TMPH/harness.out" 2> "$TMPH/harness.err"
HRC=$?
if [ "$HRC" -eq 126 ] || [ "$HRC" -eq 127 ]; then
  build "${OUT}_alt" -O1
  SLERMES_HOME="$TMPH" HOME="$TMPH" "${OUT}_alt" > "$TMPH/harness.out" 2> "$TMPH/harness.err"
  HRC=$?
fi
if [ "$HRC" -ne 0 ] && [ ! -s "$TMPH/harness.out" ]; then
  echo "ORACLE $NAME: harness could not run (rc $HRC)" >&2
  sed -n '1,3p' "$TMPH/harness.err" >&2 || true
  rm -rf "$TMPH"
  exit 1
fi
"$PYBIN" "tests/$ORACLE" < "$TMPH/harness.out"
RC=$?
rm -rf "$TMPH"
exit $RC
