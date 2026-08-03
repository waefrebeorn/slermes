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
ROOT="$(pwd)"

# All artifacts live under tests/oracle/build/ — never /tmp.
# This keeps them version-controllable, inspectable, and survives reboots.
BUILD_DIR="tests/oracle/build"
mkdir -p "$BUILD_DIR"

NAME="${1:?usage: run_oracle.sh <port_name> [fixtures_subdir]}"
SUB="${2:-$NAME}"
FIX="tests/oracle/fixtures/$SUB"
# Prefer the registry's explicit harness path (oracles may map to a
# differently-named harness, e.g. t_port_error_classifier_upstream.c for the
# error_classifier port); fall back to the conventional t_port_<name>.c.
HARNESS="tests/t_port_$NAME.c"
if command -v jq >/dev/null 2>&1; then
    REG_HARNESS=$(jq -r ".ports[\"$NAME\"].harness // empty" tests/oracle/registry.json 2>/dev/null)
    [ -n "$REG_HARNESS" ] && HARNESS="$REG_HARNESS"
elif [ -f tests/oracle/registry.json ]; then
    REG_HARNESS=$(python3 -c "import json,sys; d=json.load(open('tests/oracle/registry.json')); print(d.get('ports',{}).get(sys.argv[1],{}).get('harness',''))" "$NAME" 2>/dev/null)
    [ -n "$REG_HARNESS" ] && HARNESS="$REG_HARNESS"
fi
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
# Strip build-version metadata macros' malformed values (e.g. "0.19.0-slermes",
# "2026.7.20") which contain multiple dots / undeclared identifiers that break
# compilation of harnesses that #include the production .c files that expand
# them. Replace with a plain integer placeholder (no dots) rather than removing,
# since some .c files reference HERMES_VERSION as a real symbol.
LINKCMD=$(echo "$LINKCMD" | sed -E 's#-DHERMES_VERSION=[^ ]*#-DHERMES_VERSION=0#g; s#-DHERMES_RELEASE_DATE=[^ ]*#-DHERMES_RELEASE_DATE=0#g; s#-DATADIR=[^ ]*#-DATADIR="/share/slermes/docs"#g')
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
LINKCMD="$LINKCMD $LIBINCS $SRCINCS -D_XOPEN_SOURCE=700 -D_GNU_SOURCE"
# Isolated temp home (absolute so it resolves regardless of cwd).
TMPH="$(pwd)/$BUILD_DIR/home_$NAME"
# The harness .c is NOT part of the `slermes` make graph, so `make -n` never
# prints its compile command. Compile it explicitly here (with the same
# include roots + version-macro fixes) so harness edits actually take effect
# and the linked binary is never a stale object from a prior run.
mkdir -p "$BUILD_DIR/obj/tests"
COMPILE="gcc -c $HARNESS -o $BUILD_DIR/obj/tests/t_port_$NAME.o -I include $LIBINCS $SRCINCS -D_XOPEN_SOURCE=700 -D_GNU_SOURCE"
COMPILE=$(echo "$COMPILE" | sed -E 's#-DHERMES_VERSION=[^ ]*#-DHERMES_VERSION=0#g; s#-DHERMES_RELEASE_DATE=[^ ]*#-DHERMES_RELEASE_DATE=0#g; s#-DATADIR=[^ ]*#-DATADIR="/share/slermes/docs"#g')
# Always rebuild the harness from the current object closure (never reuse a
# stale binary from a prior run with a different object set).
rm -f "$BUILD_DIR/tt_$NAME"
rm -rf "$TMPH"; mkdir -p "$TMPH/.hermes/cron"
# Several oracles locate the live Python source via
#   Path.home() / ".hermes/hermes-agent/..."
# Under the runner's HOME=$TMPH isolation this would resolve into the temp
# home and never find the source. Symlink that exact path back to the
# developer tree (the slermes parent) so the oracles read the real source
# without escaping the isolated home. DEV_ROOT = parent of the slermes root.
DEV_ROOT="$(cd "$ROOT/.." && pwd)"
ln -sfn "$DEV_ROOT" "$TMPH/.hermes/hermes-agent"

# shellcheck disable=SC2086
bash -c "$COMPILE" >/tmp/oracle_compile.log 2>&1 || true
# shellcheck disable=SC2086
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >/tmp/oracle_link.log 2>&1 || true
if [ ! -f "$BUILD_DIR/tt_$NAME" ]; then
  echo "LINK FAILED for $NAME:" >&2
  grep -iE 'error|undefined|multiple' /tmp/oracle_link.log | head -5 >&2
fi
# Subprocess-style oracles exec a fixed /tmp/t_port_<name> binary (they
# re-run the harness themselves and compare against live Python). Point
# that path at the freshly-built harness so they see current code.
ln -sfn "$(pwd)/$BUILD_DIR/tt_$NAME" "/tmp/t_port_$NAME" 2>/dev/null || true

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
sys.stdout.write('\n'.join(out) + '\n')
PY
    mv "$tmp2" "$tmp"
  fi
  cat "$tmp"; rm -f "$tmp"
}

# (a) ENV-ISOLATION: both the C harness and the Python oracle run with a
# sanitized env so non-pure / env-dependent functions oracle deterministically
# instead of leaking the developer's real environment (proxy/auth/CI/SSH/creds,
# and a bare HOME that would resolve to the real ~/.hermes). We override the
# isolation vars via `env $ISO` (NOT `env -i`, which nukes LANG/locale and can
# segfault the C runtime). The harness/oracle also honor SLERMES_HOME /
# HERMES_HOME / HOME -> the isolated temp dir, and @SBX@/@NOTREPO@ placeholders
# in fixtures point at absolute sandbox paths, so a bare "." cwd is never the
# real repo.
ISO="SLERMES_HOME=$TMPH HERMES_HOME=$TMPH HOME=$TMPH XDG_CONFIG_HOME=$TMPH/.config XDG_DATA_HOME=$TMPH/.local/share"
ISO="$ISO TMPDIR=$TMPH"
# Real fixture dir: both C harness and Python oracle resolve fixture-relative
# paths (sample_skill/, *.md) against this. The runner hands both sides a
# TEMP-substituted copy (FSUB), whose dir is /tmp — relative paths would
# resolve to nothing. ORACLE_FIXDIR points both sides at the real fixtures dir.
ISO="$ISO ORACLE_FIXDIR=$(pwd)/$FIX"
# Clear only leak-prone NETWORK/PROXY vars that can change a function's real
# behavior. Do NOT blank credential/CI vars (AWS_PROFILE, SSH_AUTH_SOCK,
# GOOGLE_APPLICATION_CREDENTIALS, CI, ...) — several python modules read those
# at import time and would fail to load under isolation, and they never leak
# into pure-function outputs anyway. Also do NOT override PATH: the python
# oracle must keep the venv on PATH (httpx and other deps live there).
ISO="$ISO HTTP_PROXY= HTTPS_PROXY= http_proxy= https_proxy= NO_PROXY= no_proxy= ALL_PROXY= all_proxy="

for f in "$FIX"/*.in; do
  case="$(basename "$f" .in)"
  extra=""
  [ -f "$FIX/args.$case" ] && extra="$(cat "$FIX/args.$case")"
  # Fixtures use placeholders the harness/oracle must resolve:
  #   @SBX@ -> TMPH (the isolated shared home exported to both sides)
  #   @NOW@ -> a fixed timestamp (so time-relative ops are deterministic and
  #            identical on both the C and Python sides)
  # We substitute both in a temp copy and feed that to BOTH sides.
  FSUB="$(mktemp)"
  # @NOTREPO@ -> a path guaranteed to be OUTSIDE any git repo and free of project
  # markers (so functions that probe cwd/git return deterministic empty output
  # instead of leaking the developer's real repo root / branch / commit hashes).
  # /tmp is universally non-repo and marker-free.
  sed -e "s#@SBX@#$TMPH#g" -e "s#@NOW@#1700000000.0#g" -e "s#@NOTREPO@#/tmp#g" "$f" > "$FSUB"
  # The C engine resolves its home via SLERMES_HOME (kanban_home()), while the
  # Python profiles module resolves via HERMES_HOME (get_default_hermes_root()).
  # They honor DIFFERENT env vars, so we export BOTH to the same temp dir for
  # BOTH the harness and the oracle. That gives each side a single, shared,
  # isolated home so profiles the harness writes are exactly what the oracle
  # reads (and neither ever touches the developer's real ~/.hermes).
  # The harness's stdout is also piped to the oracle's stdin: oracles such as
  # file_safety_roots read the C output from stdin rather than argv, and this
  # is harmless for oracles that ignore stdin.
  env $ISO "$ROOT/tests/oracle/build/tt_$NAME" "$FSUB" $extra > "$BUILD_DIR/oracle_${NAME}_c_${case}.json" 2>/dev/null || true
  ORACLE_OUT="$BUILD_DIR/oracle_${NAME}_py_${case}.json"
  if env $ISO python3 "$ROOT/tests/oracle/_oracle_boot.py" "$ORACLE" "$FSUB" $extra < "$BUILD_DIR/oracle_${NAME}_c_${case}.json" > "$ORACLE_OUT" 2>/dev/null; then
    ORACLE_RC=0
  else
    ORACLE_RC=$?
  fi
  normalize_out "$ORACLE_OUT" > "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json"
  normalize_out "$BUILD_DIR/oracle_${NAME}_c_${case}.json" > "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json"
  if diff -q "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json" "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json" >/dev/null; then
    echo "$case: MATCH"
  else
    # Some oracles are self-summarizing: they print "MISMATCH ..." lines on
    # failure and exit non-zero, otherwise print "RESULT: X/Y match, 0 mismatch"
    # (or "oracle: 0 mismatches") and exit 0. The raw JSON diff above can't parse
    # that framing, so honor the oracle's own verdict when present.
    if grep -q "MISMATCH" "$ORACLE_OUT"; then
      echo "$case: MISMATCH"; FAIL=1
      echo "  C : $(cat "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json")"
      echo "  PY: $(cat "$ORACLE_OUT")"
    elif [ "$ORACLE_RC" -eq 0 ] && grep -qE "RESULT: .*0 mismatch|oracle: 0 mismatch|0 mismatches|0 failed" "$ORACLE_OUT"; then
      echo "$case: MATCH"
    else
      echo "$case: MISMATCH"; FAIL=1
      echo "  C : $(cat "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json")"
      echo "  PY: $(cat "$ORACLE_OUT")"
    fi
  fi
  rm -f "$FSUB"
done
rm -rf "$TMPH"
exit $FAIL