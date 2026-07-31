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
# Harnesses ALSO include headers from src/ subdirs (e.g. "port_web_git.h" at
# src/cli/) via quote-includes, which need each src/<subdir> on the path.
SRCINCS=$(find src -type d 2>/dev/null | sed 's#^#-I #' | tr '\n' ' ')
LIBINCS="-I lib $(grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk 2>/dev/null | sed 's#^#-I #' | tr '\n' ' ')"
LINKCMD="$LINKCMD $LIBINCS $SRCINCS"
# Always rebuild the harness from the current object closure (never reuse a
# stale binary from a prior run with a different object set).
rm -f "$BUILD_DIR/tt_$NAME"
# CFLAGS in the captured command use -O2 -g etc.; keep them. Build it.
TMPH="$BUILD_DIR/home_$NAME"
rm -rf "$TMPH"; mkdir -p "$TMPH/.hermes/cron"

# shellcheck disable=SC2086
bash -c "$LINKCMD -Wl,--allow-multiple-definition" 2>&1 | grep -iE 'error|undefined' || true

FAIL=0
# Load normalize rules from registry.json (strip_patterns: regex on raw output;
# strip_fields: JSON object keys to drop before diffing). This is what lets the
# harness correctly classify FALSE FAPs (env noise — real cwd, git status,
# hostname) as non-failures instead of spurious MISMATCHes.
REG_JSON="${REG_JSON:-tests/oracle/registry.json}"
STRIP_PATTERNS=$(python3 -c "import json,sys; print('\n'.join(json.load(open('$REG_JSON')).get('normalize',{}).get('strip_patterns',[])))" 2>/dev/null)
STRIP_FIELDS=$(python3 -c "import json,sys; print(' '.join(json.load(open('$REG_JSON')).get('normalize',{}).get('strip_fields',[])))" 2>/dev/null)

normalize_out() {  # $1 = file
  local f="$1"
  local tmp; tmp="$(mktemp)"
  cp "$f" "$tmp"
  # strip_patterns: treat each as a Python regex, remove every match
  if [ -n "$STRIP_PATTERNS" ]; then
    tmp2="$(mktemp)"
    python3 - "$tmp" "$STRIP_PATTERNS" <<'PY' > "$tmp2" || cp "$tmp" "$tmp2"
import re, sys
src, pats = sys.argv[1], sys.argv[2]
text = open(src, encoding='utf-8', errors='replace').read()
for p in pats.split('\n'):
    p = p.strip()
    if p:
        text = re.sub(p, '', text)
open(sys.argv[1].replace('/tmp/','/tmp/n_'), 'w').write(text) if False else None
sys.stdout.write(text)
PY
    mv "$tmp2" "$tmp"
  fi
  # strip_fields: drop top-level JSON object keys (per-line JSON)
  if [ -n "$STRIP_FIELDS" ]; then
    tmp2="$(mktemp)"
    python3 - "$tmp" $STRIP_FIELDS <<'PY' > "$tmp2"
import json, sys
src = sys.argv[1]; fields = set(sys.argv[2:])
out = []
for line in open(src, encoding='utf-8', errors='replace'):
    line = line.strip()
    if not line: continue
    try:
        obj = json.loads(line)
        if isinstance(obj, dict):
            obj = {k: v for k, v in obj.items() if k not in fields}
            line = json.dumps(obj)
    except Exception:
        pass
    out.append(line)
open(src, 'w').write('\n'.join(out) + '\n')
PY
    mv "$tmp2" "$tmp"
  fi
  cat "$tmp"; rm -f "$tmp"
}

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
  normalize_out "$BUILD_DIR/oracle_${NAME}_c_${case}.json" > "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json"
  normalize_out "$BUILD_DIR/oracle_${NAME}_py_${case}.json" > "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json"
  if diff -q "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json" "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json" >/dev/null; then
    echo "$case: MATCH"
  else
    echo "$case: MISMATCH"; FAIL=1
    echo "  C : $(cat "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json")"
    echo "  PY: $(cat "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json")"
  fi
done
rm -rf "$TMPH"
exit $FAIL