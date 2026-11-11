#!/usr/bin/env bash
# build_all_harnesses.sh — compile every oracle harness in one make invocation.
#
# WHY THIS EXISTS: run_one_oracle.sh spawns ~20 processes per oracle (bash,
# dirname, grep/sed/tr, mkdir, find, uname, python probe, gcc -> cc1/as/ld/
# collect2, harness, oracle python, rm). On MSYS2/Windows a process spawn costs
# 150-300ms, so the gate was paying ~4s of pure spawn overhead per oracle —
# ~400s for 102 oracles, almost none of it compilation.
#
# Batching the compiles into a single `make -j` amortises all of that: one make
# process, N parallel gcc invocations, no per-oracle shell prologue.
#
# Usage: build_all_harnesses.sh <specs-file>
# The file holds one "name<TAB>harness<TAB>objs" per line; objs is
# space-separated, or the sentinel FULL for the whole project object set.
# name is the oracle name (tests/oracle_<name>.py); harness is the harness
# basename (tests/t_port_<harness>.c) and may differ for aliased ports.
set -e
cd "$(dirname "$0")/.."

SPECS_FILE="${1:-}"
if [ -n "$SPECS_FILE" ]; then
  mapfile -t SPECS < "$SPECS_FILE"
else
  SPECS=("$@")
fi
# Drop registrations whose harness does not exist (stale entries), and
# de-duplicate by harness: aliased oracles (audit_bounded -> audit) share one
# binary, and building it twice would only race on the same output path.
LIVE=()
declare -A SEEN_HARNESS=()
for spec in "${SPECS[@]}"; do
  [ -n "$spec" ] || continue
  IFS=$'\t' read -r n h o <<< "$spec"
  [ -n "$n" ] || continue
  [ -n "$h" ] || h="$n"
  [ -f "tests/t_port_$h.c" ] || continue
  [ -n "${SEEN_HARNESS[$h]:-}" ] && continue
  SEEN_HARNESS[$h]=1
  LIVE+=("$n"$'\t'"$h"$'\t'"$o")
done
SPECS=("${LIVE[@]}")

WORK=".testbin/harnesses.$$"
mkdir -p "$WORK"
MK="$WORK/harnesses.mk"

# --- collect the link arguments -------------------------------------------------
LINK_LIBS="-lstdc++ -lm -lz -lpcre2-8 -ltre -lssl -lcrypto"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    LINK_LIBS="$LINK_LIBS -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 -lole32 -luuid -lSDL2main -lSDL2 -lSDL2_ttf"
    ;;
  *) LINK_LIBS="$LINK_LIBS -ldl -lpthread" ;;
esac

# LIBINCS is identical for every harness; compute it once.
LIBINCS=$(grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk | sed 's#^#-I #' | tr '\n' ' ')

{
  echo "CC      := gcc"
  echo "CFLAGS  := -O2 -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h -I include/compat -I include -I src $LIBINCS -I C:/winlibs/extra-inc"
  echo "LDFLAGS := -L C:/winlibs/extra-libs -Wl,--allow-multiple-definition"
  echo "LIBS    := $LINK_LIBS"
  echo
  echo ".PHONY: all"
  echo -n "all:"
  for spec in "${SPECS[@]}"; do
    IFS=$'\t' read -r _n h _o <<< "$spec"
    printf ' %s' "$WORK/$h.exe"
  done
  echo
  echo
  for spec in "${SPECS[@]}"; do
    IFS=$'\t' read -r _n h o <<< "$spec"
    # A harness that needs the whole project object set passes "FULL".
    if [ "$o" = "FULL" ]; then
      o=$(find src lib -name '*.o' ! -name 'main.o' | tr '\n' ' ')
    fi
    # The object list can exceed the Windows ~32k command-line limit (a FULL
    # link is 1205 objects), so hand it to gcc through a response file.
    rsp="$WORK/$h.rsp"
    printf '%s\n' $o > "$rsp"
    printf '%s/%s.exe: tests/t_port_%s.c\n\t$(CC) $(CFLAGS) $< @%s -o $@ $(LDFLAGS) $(LIBS)\n\n' \
      "$WORK" "$h" "$h" "$rsp"
  done
} > "$MK"

make -f "$MK" -j"${HARNESS_JOBS:-8}" all >/dev/null

# Publish the binaries where run_one_oracle.sh expects them (keyed by harness
# name), then stamp the cache. run_one_oracle.sh treats any object or harness
# source newer than this stamp as a cache miss and recompiles from source, so
# a stale binary can never stand in for the current code.
mkdir -p .testbin/harnesses
for spec in "${SPECS[@]}"; do
  IFS=$'\t' read -r _n h _o <<< "$spec"
  cp "$WORK/$h.exe" ".testbin/harnesses/$h.exe"
done
: > .testbin/harnesses/.stamp
echo "built ${#SPECS[@]} harnesses -> .testbin/harnesses/"
rm -rf "$WORK"
