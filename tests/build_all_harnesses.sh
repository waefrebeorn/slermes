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
# Harnesses #include the port .c by bare name; add every src subdir.
SRCINCS=$(find src -maxdepth 3 -type d | sed 's#^#-I #' | tr '\n' ' ')

# FULL harnesses link against a single static archive instead of ~2241 loose
# objects; ld then pulls only what each harness actually references.
ARCHIVE_PATH=$(bash tests/make_harness_archive.sh | tail -1)
[ -f "$ARCHIVE_PATH" ] || { echo "harness archive unavailable" >&2; exit 1; }

# Ensure every explicit dependency object exists (SAC-safe -O0 hash image,
# local stubs, unbuilt port objects) before the batch link.
ALL_OBJS=""
for spec in "${LIVE[@]}"; do
  IFS=$'\t' read -r _n _h o <<< "$spec"
  [ "$o" = "FULL" ] && continue
  ALL_OBJS="$ALL_OBJS $o"
done
# shellcheck disable=SC2086
bash tests/ensure_oracle_deps.sh $ALL_OBJS || { echo "dependency build failed" >&2; exit 1; }

{
  echo "CC      := gcc"
  echo "CFLAGS  := -O2 -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h -I include/compat -I include -I src -I lib $SRCINCS $LIBINCS -I C:/winlibs/extra-inc"
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
    if [ "$o" = "FULL" ] && [ "${HARNESS_LINK_MODE:-archive}" != "explicit" ]; then
      # Static archive: ld pulls only the objects the harness references.
      # Measured 2.5 s per link vs ~9 s, and 217 KB vs 19 MB images.
      o="$ARCHIVE_PATH"
    elif [ "$o" = "FULL" ]; then
      # Legacy path: force every registered object into the image (~9 s link,
      # 19 MB binary). Kept as a fallback if archive resolution ever misses a
      # symbol a harness needs at runtime.
      {
        grep -hoE '[A-Za-z0-9_./\\-]+\.o' build/*.mk \
          | tr '\\' '/' | sed 's,^\./,,' | sort -u \
          | while read -r obj; do
              case "$obj" in
                */main.o|*/hash.o|*/hermes_regex.o|*/auxiliary_client_runtime.o) continue ;;
              esac
              if [ -f "$obj" ]; then printf '%s ' "$obj"; fi
            done
        if [ -f .testbin/hash_O0.o ]; then printf '%s ' .testbin/hash_O0.o; fi
        if [ -f .testbin/hermes_regex_O0.o ]; then printf '%s ' .testbin/hermes_regex_O0.o; fi
        echo
      } > "$WORK/full_objs.rsp"
      o=$(cat "$WORK/full_objs.rsp")
    fi
    # The object list can exceed the Windows ~32k command-line limit (a FULL
    # link is 1205 objects), so hand it to gcc through a response file.
    rsp="$WORK/$h.rsp"
    printf '%s\n' $o > "$rsp"
    printf '%s/%s.exe: tests/t_port_%s.c\n\t$(CC) $(CFLAGS) $< @%s -o $@ $(LDFLAGS) $(LIBS)\n\n' \
      "$WORK" "$h" "$h" "$rsp"
  done
} > "$MK"

make -f "$MK" -j"${HARNESS_JOBS:-8}" all > .testbin/build_all_harnesses.log 2>&1 || \
  echo "WARNING: batch harness build had failures; see .testbin/build_all_harnesses.log" >&2

# Publish the binaries where run_one_oracle.sh expects them (keyed by harness
# name), then stamp the cache. run_one_oracle.sh treats any object or harness
# source newer than this stamp as a cache miss and recompiles from source, so
# a stale binary can never stand in for the current code.
mkdir -p .testbin/harnesses
for spec in "${SPECS[@]}"; do
  IFS=$'\t' read -r _n h _o <<< "$spec"
  # A failed batch target must not abort the whole publish loop: publish the
  # ones that built, and let run_one_oracle compile the few missing ones
  # individually with their resolved deps.
  if [ -f "$WORK/$h.exe" ]; then
    cp "$WORK/$h.exe" ".testbin/harnesses/$h.exe"
  else
    echo "WARN: harness $h did not build; will retry per-oracle" >&2
  fi
done
: > .testbin/harnesses/.stamp
echo "built ${#SPECS[@]} harnesses -> .testbin/harnesses/"
rm -rf "$WORK"
