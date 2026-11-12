#!/usr/bin/env bash
# run_one_oracle.sh <harness_basename> [oracle_py_basename]
# Compiles tests/t_port_<name>.c against the full project .o set and pipes
# stdout into tests/sta_oracle_<name>.py (or the provided oracle).
set -e
cd "$(dirname "$0")/.."
NAME="$1"
ORACLE="${2:-sta_oracle_$NAME.py}"
# Borrowed third-party lib dir, passed as an argument by the runner because the
# ccache wrapper strips LIBRARY_PATH and this box's bash drops exported vars in
# nested children. Empty means "use the default search path".
LIBDIR="${3:-}"
# Computed once, not per oracle: the grep|sed|tr pipeline costs 3 process
# spawns (~450ms) for a value that never changes.
LIBINCS_FILE=".testbin/libincs.cache"
if [ ! -s "$LIBINCS_FILE" ] || [ "build/libs-config.mk" -nt "$LIBINCS_FILE" ]; then
  mkdir -p .testbin
  grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk | sed 's#^#-I #' | tr '\n' ' ' > "$LIBINCS_FILE"
fi
LIBINCS=$(cat "$LIBINCS_FILE")
# Windows/MinGW: the inline object list overflows the command line (gcc then
# silently drops the tail), /tmp is a POSIX path a native gcc.exe cannot
# resolve, and there is no libdl/libpthread. Use a response file + .testbin.
mkdir -p .testbin
# Per-INVOCATION paths ($$), not per-harness: run_all_oracles.sh runs several
# oracles concurrently and two runs of the SAME harness would otherwise clobber
# each other's response file, binary and scratch dir.
OBJSET=.testbin/oracle_objs_${NAME}_$$.rsp
# "FULL" is a sentinel meaning the entire project object set (for ports whose
# kernel lives in a core .o like cli_cmd_system.o that the default per-port set
# under-binds). Healthy link: stubs resolve, duplicate definitions collapse.
# The response file must be written for BOTH the empty and the FULL case: gcc
# is handed @$OBJSET either way, and a missing file is a hard link error.
OBJARG="${MINIMAL_OBJS:-}"
if [ -z "$OBJARG" ] || [ "$OBJARG" = "FULL" ]; then
  find src lib -name '*.o' ! -name 'main.o' | tr '\n' ' ' > "$OBJSET"
  echo >> "$OBJSET"
  OBJARG="@$OBJSET"
fi
EXTRA_LIBS=""
WIN_LIBS=""
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    WIN_LIBS="-lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 -lole32 -luuid -lSDL2main -lSDL2 -lSDL2_ttf"
    ;;
  *) EXTRA_LIBS="-ldl -lpthread" ;;
esac
TMPH="$(pwd)/.testbin/oracle_tmp_${NAME}_$$"
mkdir -p "$TMPH/.hermes/cron"
# Pick an interpreter with a working hashlib: the MSYS2 /mingw64 python3 that
# PATH resolves to first has a broken OpenSSL binding (blake2b/blake2s raise at
# import), which would make a sha256 oracle fail for an unrelated reason.
PYCACHE=".testbin/pytestbin.cache"
if [ -s "$PYCACHE" ] && "$(cat "$PYCACHE")" -c "import hashlib,json" >/dev/null 2>&1; then
  PYBIN="$(cat "$PYCACHE")"
else
  mkdir -p .testbin
  pick_python() {
    for cand in "${PYTHON_BIN:-}" /c/Users/*/AppData/Local/hermes/tools/python-*/python.exe \
                python python3 py; do
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
  printf '%s\n' "$PYBIN" > "$PYCACHE"
fi
# A link failure must be fatal — piping gcc through `grep ... || true` let a
# broken link reach the oracle as "0 cases".
# Use the repo-local ccache wrapper when present (sets MSYS env vars + pins
# the cache to <repo>/.ccache). Subsequent identical harness compiles of the
# SAME port .c hash-hit, so re-gating a port is near-instant even when the
# harness was rebuilt for a different object set.
# ccache only pays off when the same translation unit is compiled repeatedly
# (the main build). An oracle harness is compiled once and thrown away, so the
# wrapper's ~660ms of bash+ccache startup is pure overhead here.
COMPILER="gcc"
ORACLE_LIBDIR_ARGS=()
if [ -n "$LIBDIR" ]; then
  ORACLE_LIBDIR_ARGS=(-L "$LIBDIR")
fi
build() {   # build <out> <opt>
  # -L MUST precede the -l flags: the linker resolves each -l against the
  # search path as it is encountered, so a -L placed after them finds nothing.
  $COMPILER "$2" -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h -I include/compat -I include -I src -I lib $LIBINCS \
    -I "${ORACLE_INC:-C:/winlibs/extra-inc}" \
    "tests/t_port_$NAME.c" $OBJARG -o "$1" \
    "${ORACLE_LIBDIR_ARGS[@]}" \
    -L "C:/winlibs/extra-libs" \
    -lstdc++ -lm $EXTRA_LIBS $WIN_LIBS -lz -lpcre2-8 -ltre -lssl -lcrypto \
    -Wl,--allow-multiple-definition
}
OUT=".testbin/tt_${NAME}_$$"
# Fast path: build_all_harnesses.sh precompiles every harness in one make -j
# invocation and stamps .testbin/harnesses/.stamp. Reusing it turns a ~4s
# per-oracle compile into a file copy -- but ONLY while the cache is fresh.
# A harness linked against a since-rebuilt .o would silently test the OLD
# code and report a green gate for a regression, so any newer object or
# harness source invalidates it.
prebuilt_fresh=0
if [ -x ".testbin/harnesses/$NAME.exe" ] && [ -z "${ORACLE_REBUILD:-}" ]; then
  if [ "${ORACLE_PREBUILT_OK:-0}" = "1" ]; then
    # The full gate validated the whole cache in one find before starting.
    prebuilt_fresh=1
  elif [ -z "$(find src lib tests \( -name '*.o' -o -name 't_port_*.c' \) -newer ".testbin/harnesses/$NAME.exe" -print -quit 2>/dev/null)" ]; then
    prebuilt_fresh=1
  fi
fi
if [ "$prebuilt_fresh" = "1" ]; then
  cp ".testbin/harnesses/$NAME.exe" "$OUT"
else
  build "$OUT" -O2
fi

# Smart App Control refuses some freshly linked binaries outright (rc 126) and
# only PowerShell names the reason. A relink with an extra optimization level
# produces a different image, which it lets through — retry once that way
# rather than reporting a false "0 cases".
SLERMES_HOME="$TMPH" HOME="$TMPH" "$OUT" > "$TMPH/harness.out" 2> "$TMPH/harness.err"
HRC=$?
if [ "$HRC" -eq 126 ] || [ "$HRC" -eq 127 ]; then
  if [ "$prebuilt_fresh" = "1" ]; then
    cp ".testbin/harnesses/$NAME.exe" "${OUT}_alt"
  else
    build "${OUT}_alt" -O1
  fi
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
