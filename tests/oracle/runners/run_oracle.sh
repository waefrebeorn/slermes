#!/usr/bin/env bash
# run_oracle.sh <port_name> [fixtures_subdir]
#
# Generic "diff" oracle runner (contract B from README.md).
# Compiles tests/t_port_<port_name>.c together with the project object set,
# then for every fixture in tests/oracle/fixtures/<subdir|port_name>/:
#     runs the C harness  -> tests/oracle/build/oracle_<port>_c_<case>.json
#     runs the Python oracle -> tests/oracle/build/oracle_<port>_py_<case>.json
#     diffs the two. Any difference => mismatch (exit 1).
#
# All artifacts live under tests/oracle/build/ — never /tmp.
# This keeps them version-controllable, inspectable, and survives reboots.
#
# Requires: tests/t_port_<port_name>.c (reads fixture path from argv[1])
#           tests/sta_oracle_<port_name>.py (reads fixture path from argv[1])
#           fixtures under tests/oracle/fixtures/<subdir>/*.in
set -euo pipefail
cd "$(dirname "$0")/../../.."   # slermes root (runners/ is 3 deep under root)

# All artifacts live under tests/oracle/build/ — never /tmp.
# This keeps them version-controllable, inspectable, and survives reboots.
BUILD_DIR="tests/oracle/build"
mkdir -p "$BUILD_DIR"

NAME="${1:?usage: run_oracle.sh <port_name> [fixtures_subdir]}"
SUB="${2:-$NAME}"
FIX="tests/oracle/fixtures/$SUB"
HARNESS="tests/t_port_$NAME.c"
ORACLE="tests/sta_oracle_$NAME.py"

[ -f "$HARNESS" ] || { echo "MISSING $HARNESS"; exit 2; }
[ -f "$ORACLE" ]  || { echo "MISSING $ORACLE"; exit 2; }
[ -d "$FIX" ]     || { echo "MISSING fixture dir $FIX"; exit 2; }

# Extract the EXACT link command `make` uses for the real `slermes` binary and
# adapt it for the oracle harness. This guarantees the oracle links the same
# resolvable object/lib/static-archive closure the shipped binary uses — no
# missing whisper .a, no SDL/orphan-object drag-in, no hand-maintained lib list.
# We grab the gcc link recipe (from the `gcc ... -o slermes` line through the
# final `libwhisper.a` static archive — the true end of the link command), drop
# the output target, and swap src/main.o for the oracle harness .c. make -n
# escapes quotes for display, so we strip \" and join continuations.
LINKCMD=$(make -B -n slermes 2>/dev/null \
  | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' \
  | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')

# Normalize: replace the output target and the main object.
LINKCMD=${LINKCMD// -o slermes / -o "$BUILD_DIR/tt_$NAME" }
LINKCMD=${LINKCMD//src\/main.o /$HARNESS }
# The harness recompiles fresh from the link recipe, which carries no -I flags.
# Harnesses include both "hermes_json.h" (in include/) and "libjson/json.h"
# (in lib/libjson/). The real build injects per-lib include roots via
# build/libs-config.mk; replicate that here. NOTE: an include of "libjson/json.h"
# resolves against -I lib (=> lib/libjson/json.h), NOT -I lib/libjson, so the
# root "lib" must be present alongside the per-lib dirs.
LIBINCS="-I lib $(grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk 2>/dev/null | sed 's#^#-I #' | tr '\n' ' ')"
LINKCMD="$LINKCMD $LIBINCS"
# Always rebuild the harness from the current object closure (never reuse a
# stale binary from a prior run with a different object set).
rm -f "$BUILD_DIR/tt_$NAME"
# CFLAGS in the captured command use -O2 -g etc.; keep them. Build it.
TMPH="$BUILD_DIR/home_$NAME"
rm -rf "$TMPH"; mkdir -p "$TMPH/.hermes/cron"

# shellcheck disable=SC2086
bash -c "$LINKCMD -Wl,--allow-multiple-definition" 2>&1 | grep -iE 'error|undefined' || true

FAIL=0
for f in "$FIX"/*.in; do
  case="$(basename "$f" .in)"
  extra=""
  [ -f "$FIX/args.$case" ] && extra="$(cat "$FIX/args.$case")"
  # The C engine resolves its home via SLERMES_HOME (kanban_home()), while the
  # Python profiles module resolves via HERMES_HOME (get_default_hermes_root()).
  # They honor DIFFERENT env vars, so we export BOTH to the same temp dir for
  # BOTH the harness and the oracle. That gives each side a single, shared,
  # isolated home so profiles the harness writes are exactly what the oracle
  # reads (and neither ever touches the developer's real ~/.hermes).
  SLERMES_HOME="$TMPH" HERMES_HOME="$TMPH" HOME="$TMPH" "$BUILD_DIR/tt_$NAME" "$f" $extra > "$BUILD_DIR/oracle_${NAME}_c_${case}.json" 2>/dev/null
  SLERMES_HOME="$TMPH" HERMES_HOME="$TMPH" HOME="$TMPH" python3 "$ORACLE" "$f" $extra > "$BUILD_DIR/oracle_${NAME}_py_${case}.json" 2>/dev/null
  if diff -q "$BUILD_DIR/oracle_${NAME}_c_${case}.json" "$BUILD_DIR/oracle_${NAME}_py_${case}.json" >/dev/null; then
    echo "$case: MATCH"
  else
    echo "$case: MISMATCH"; FAIL=1
    echo "  C : $(cat "$BUILD_DIR/oracle_${NAME}_c_${case}.json")"
    echo "  PY: $(cat "$BUILD_DIR/oracle_${NAME}_py_${case}.json")"
  fi
done
rm -rf "$TMPH"
exit $FAIL