#!/usr/bin/env bash
# run_oracle.sh <port_name> [fixtures_subdir]
#
# --- environment -----------------------------------------------------
# This runner must work when the caller has set nothing up. A Hermes-promoted
# background process inherits a pure Windows PATH: no /usr/bin, no
# /mingw64/bin, no winlibs, and WindowsApps alias stubs that exit 0 for
# --version then die with "Permission denied" on real use. So build the
# environment here rather than trusting the caller.
_sd="${0%/*}"
_env_file="$_sd/../../../build/winlibs-env.sh"
if [ -f "$_env_file" ]; then
  . "$_env_file" >/dev/null 2>&1 || true
fi
_clean_path=""
IFS=':' read -r -a _path_parts <<< "$PATH"
for _d in "${_path_parts[@]}"; do
  case "$_d" in
    */WindowsApps*) continue ;;
  esac
  _clean_path="${_clean_path:+$_clean_path:}$_d"
done
PATH="$_clean_path"
export PATH
# Fallback tool dirs for hosts without build/winlibs-env.sh (Linux/macOS).
# /usr/bin carries no gcc or "$PY" here. /mingw64/bin DOES carry gcc -- the
# MSYS2 one whose backend is blocked on this box -- so it is only added when no
# compiler is resolvable at all; otherwise it shadows the winlibs toolchain.
[ -d /usr/bin ] && PATH="/usr/bin:$PATH"
if ! command -v gcc >/dev/null 2>&1; then
  [ -d /c/msys64/mingw64/bin ] && PATH="/c/msys64/mingw64/bin:$PATH"
fi
export PATH

# Resolve the interpreter ABSOLUTELY. PATH lookup of `python3` is unreliable on
# this box: WindowsApps App Execution Aliases shadow the name and intermittently
# resolve to a stub that prints "Python was not found", which under
# `set -euo pipefail` silently kills a runner mid-suite.
PY=""
# Prefer the Hermes venv: the oracles import the dev tree's modules, which pull
# third-party deps (httpx, ...). The MSYS interpreters carry none.
for _c in /c/Users/eman5/hermes-agent/.venv/Scripts/python.exe \
          /c/msys64/mingw64/bin/python3.exe /c/msys64/usr/bin/python3.exe \
          /usr/bin/python3.exe /c/winlibs/mingw64/bin/python3.exe; do
  if [ -x "$_c" ]; then PY="$_c"; break; fi
done
[ -n "$PY" ] || { echo "FATAL: no python3 interpreter found" >&2; exit 2; }
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
# Temp files live under BUILD_DIR, not /tmp. A Hermes-promoted background
# process has a broken /tmp mapping and mktemp then yields a path that does not
# exist, so the oracle dies with FileNotFoundError. This also matches the
# runner's own rule that artifacts stay under tests/oracle/build.
_tmp_seq=0
tmpfile() {
  _tmp_seq=$((_tmp_seq + 1))
  local _f="$BUILD_DIR/tmp_${NAME:-run}_$$_${_tmp_seq}"
  : > "$_f"
  printf '%s' "$_f"
}
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
    REG_HARNESS=$("$PY" -c "import json,sys; d=json.load(open('tests/oracle/registry.json')); print(d.get('ports',{}).get(sys.argv[1],{}).get('harness',''))" "$NAME" 2>/dev/null)
    [ -n "$REG_HARNESS" ] && HARNESS="$REG_HARNESS"
fi
# Fixture setup: some fixtures need a git repo / binary blob / seeded DB that
# cannot be committed (nested .git, binary artifacts). A gen_fixture.sh in the
# fixture dir, when present and executable, (re)materializes the fixture in
# place. Run it only if repo/.git is absent — cheap and idempotent.
if [ -x "$FIX/gen_fixture.sh" ] && [ ! -d "$FIX/repo/.git" ]; then
    bash "$FIX/gen_fixture.sh"
fi
ORACLE="tests/sta_oracle_$NAME.py"

# Declared-incomplete ports: the registry records an intention whose harness or
# oracle does not exist yet. Skip with a stated reason rather than failing, so
# the run reports SKIP instead of a broken port -- but ONLY when the registry
# declares it. An undeclared dangling reference is a gate failure.
STATUS=$("$PY" -c "import json,sys; d=json.load(open('tests/oracle/registry.json')); print(d.get('ports',{}).get(sys.argv[1],{}).get('status',''))" "$NAME" 2>/dev/null || true)
if [ "$STATUS" = "incomplete" ]; then
    REASON=$("$PY" -c "import json,sys; d=json.load(open('tests/oracle/registry.json')); print(d.get('ports',{}).get(sys.argv[1],{}).get('incomplete_reason',''))" "$NAME" 2>/dev/null || true)
    echo "SKIP $NAME: declared incomplete ($REASON)"
    exit 0
fi

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
  | sed -e ':a;N;$!ba;s/\\\n//g' \
  | sed 's/  */ /g; s/\\"//g')
# ccache cannot exec the compiler from a Hermes-promoted background process
# ("execute_noreturn of ... failed: No such file or directory") even though the
# same gcc runs fine directly. The harness link is one invocation and gains
# nothing from caching, so drop the wrapper.
LINKCMD="${LINKCMD#ccache }"

# Normalize: replace the output target and the main object.
LINKCMD=${LINKCMD// -o slermes / -o "$BUILD_DIR/tt_$NAME" }
LINKCMD=${LINKCMD//src\/main.o /$HARNESS }
# The slermes link recipe passes its objects through @slermes.link.rsp, a file
# the recipe creates and deletes. `make -n` never creates it, so the extracted
# command named a nonexistent file, the link failed, and -- because a failed
# run was not treated as a failure -- the oracle reported MATCH against an
# EMPTY C-side output. Substitute the real closure from `make print-objs`.
OBJS=$(make -s --no-print-directory print-objs 2>/dev/null)
if [ -z "$OBJS" ]; then
  echo "FATAL: 'make print-objs' returned nothing; cannot link the harness" >&2
  exit 2
fi
# Inlining 2,158 objects into the command line exceeds the Windows limit, and
# the tail of the list is silently dropped -- the link then fails with ~125
# undefined references from whichever object sat earliest. The real build uses
# a response file for exactly this reason, so write one and pass @file.
RSP="$BUILD_DIR/tt_$NAME.rsp"
# The harness replaces src/main.o, but src/main.o also carries WinMain, which
# -lSDL2main requires. Drop it from the object list and satisfy WinMain with a
# stub, so the harness's own main() is the one that runs (linking src/main.o
# too lets its main win and the harness prints the banner instead of JSON).
OBJS=$(printf '%s\n' $OBJS | grep -v '^src/main\.o$' | tr '\n' ' ')
: > "$RSP"
for o in $OBJS; do echo "$o" >> "$RSP"; done
STUB_OBJ="$BUILD_DIR/winmain_stub_$NAME.o"
ccache gcc -c "tests/oracle/runners/winmain_stub.c" -o "$STUB_OBJ" \
  -I include -I lib >/dev/null 2>&1 \
  || gcc -c "tests/oracle/runners/winmain_stub.c" -o "$STUB_OBJ" \
       -I include -I lib >/dev/null 2>&1 || true
LINKCMD=${LINKCMD//@slermes.link.rsp/@$RSP}
[ -f "$STUB_OBJ" ] && LINKCMD="$LINKCMD $STUB_OBJ"
# The harness object must be on the link line. It used to be substituted in via
# `src/main.o`, but src/main.o now lives inside the response file, so that
# substitution matched nothing: the harness was compiled and then never linked,
# and main() came from src/hermes_cli/main.o -- the binary printed the agent
# banner and the oracle compared two empty outputs. Put it first so its main()
# wins under -Wl,--allow-multiple-definition.
HARNESS_OBJ="$BUILD_DIR/obj/tests/t_port_$NAME.o"
# Object files must precede the libraries: the link line ends with
# $(LDFLAGS) $(LIBS) (-lssl -lcrypto ...), and appending the harness after them
# leaves its references (e.g. MD5_Init from libcrypto) unresolved.
#
# The harness must also come BEFORE the object list. With
# -Wl,--allow-multiple-definition the first definition wins, and the minimal
# closure can pull in an object that defines main() -- if that object precedes
# the harness, the harness never runs and produces no output at all.
LINKCMD=${LINKCMD//@slermes.link.rsp/$HARNESS_OBJ @$RSP}
# There is no `diff` on this box: MSYS2 here ships no diffutils, and PATH
# resolves the name to a WindowsApps alias stub that fails with "Permission
# denied". Depending on an external diff would also make the runner fragile on
# the Linux/macOS targets, so compare bytes with Python -- the runner already
# requires "$PY" for the oracle side.
files_equal() {
  "$PY" -c 'import sys
a = open(sys.argv[1], "rb").read()
b = open(sys.argv[2], "rb").read()
sys.exit(0 if a == b else 1)' "$1" "$2"
}
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
#
# The extracted link line may reference .o files from lib/ that the main
# build only creates as .a archives or that were cleaned. Pre-compile any
# missing .o files so the harness link succeeds.
mkdir -p "$BUILD_DIR/obj/tests"
# Pre-compile any .o files referenced in the link line that don't exist on disk
for obj in $LINKCMD; do
    case "$obj" in
        *.o)
            if [ ! -f "$obj" ]; then
                src="${obj%.o}.c"
                if [ -f "$src" ]; then
                    echo "Pre-compiling $obj from $src..." >&2
                    gcc -c "$src" -o "$obj" \
                        -I include $LIBINCS $SRCINCS \
                        -D_XOPEN_SOURCE=700 -D_GNU_SOURCE \
                        -DHERMES_VERSION=0 -DHERMES_RELEASE_DATE=0 \
                        -DATADIR=""/share/slermes/docs"" 2>/dev/null || true
                fi
            fi
            ;;
    esac
done
# The harness must compile with the same include paths as the real build. It
# was missing -I include/compat (the POSIX shims: sys/wait.h, sys/socket.h,
# sys/utsname.h, regex.h, ...) and CFLAGS_EXTRA (the borrowed third-party
# headers), so 70 harnesses failed on headers the shipped build resolves fine.
COMPILE="gcc -c $HARNESS -o $BUILD_DIR/obj/tests/t_port_$NAME.o -include hermes_win32.h -I include/compat -I include $LIBINCS $SRCINCS $CFLAGS_EXTRA -D_XOPEN_SOURCE=700 -D_GNU_SOURCE"
COMPILE=$(echo "$COMPILE" | sed -E 's#-DHERMES_VERSION=[^ ]*#-DHERMES_VERSION=0#g; s#-DHERMES_RELEASE_DATE=[^ ]*#-DHERMES_RELEASE_DATE=0#g; s#-DATADIR=[^ ]*#-DATADIR="\/share\/slermes\/docs"#g')
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
# MSYS2's `ln -s` silently COPIES when it cannot create a symlink. Linking the
# developer home that way copied all of C:/Users/eman5 -- 13 GB, including
# .git-credentials -- into tests/oracle/build/home_<port>/ on every oracle run.
# Use a real Windows directory link, and if that is unavailable leave the path
# absent and WARN: a missing link fails loudly in the oracle that needs it,
# while a copy fails silently and fills the disk.
rm -rf "$TMPH/.hermes/hermes-agent"
# A directory junction needs no privilege (unlike a symlink, which needs
# SeCreateSymbolicLinkPrivilege) and is transparent to POSIX path resolution.
if ! "$PY" "tests/oracle/runners/mklink_dir.py" \
        "$TMPH/.hermes/hermes-agent" "$DEV_ROOT" >/dev/null 2>&1; then
    echo "WARN: could not link \$TMPH/.hermes/hermes-agent -> $DEV_ROOT; oracles that resolve the live source through Path.home() will not find it" >&2
fi

# A stale harness binary must not be diffed, and a failed link must FAIL the
# run. Previously the binary was left in place and the run continued: the
# harness produced a 0-byte C output, the oracle read empty stdin, self-reported
# "0 mismatches", and the runner exited 0. That is a false pass, and it is worse
# than a red gate.
rm -f "$BUILD_DIR/tt_$NAME"
# shellcheck disable=SC2086
bash -c "$COMPILE" >$BUILD_DIR/oracle_compile_$NAME.log 2>&1 || true
if [ ! -f "$HARNESS_OBJ" ]; then
  echo "FATAL: harness did not compile -> $HARNESS_OBJ" >&2
  head -12 "$BUILD_DIR/oracle_compile_$NAME.log" >&2
  exit 2
fi
# Minimal closure (default on). Linking every harness against all 1,231
# objects costs ~19 s each; a typical harness needs 2-20. The index is built
# once from `make print-objs` and cached, so the per-port cost is one BFS.
# Set ORACLE_MINIMAL=0 to link the full closure.
if [ "${ORACLE_MINIMAL:-1}" = "1" ]; then
  if [ ! -s "$BUILD_DIR/sym_index.json" ]; then
    make -s --no-print-directory print-objs 2>/dev/null > "$BUILD_DIR/objs.txt"
    "$PY" "tests/oracle/runners/sym_index.py" build "@$BUILD_DIR/objs.txt" >/dev/null 2>&1
  fi
  # Fail safe: if the closure cannot be computed, keep the full object list.
  # A slow correct link beats a fast wrong one.
  # `|| true`: under `set -euo pipefail` a failing substitution in an
  # assignment aborts the whole runner silently (exit 2, no message).
  MINOBJS=$("$PY" "tests/oracle/runners/sym_index.py" closure \
              "$BUILD_DIR/sym_index.json" "$HARNESS_OBJ" 2>/dev/null \
            | "$PY" -c 'import json,sys
try:
    d = json.load(sys.stdin)
    print(" ".join(d["objects"]))
except Exception:
    pass' || true)
  if [ -n "$MINOBJS" ]; then
    : > "$RSP"
    for o in $MINOBJS; do echo "$o" >> "$RSP"; done
    echo "minimal closure: $(printf '%s' "$MINOBJS" | wc -w) objects" >&2
  else
    echo "WARN: minimal closure unavailable; linking the full object set" >&2
  fi
fi
# shellcheck disable=SC2086
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >$BUILD_DIR/oracle_link_$NAME.log 2>&1 || true
if [ ! -f "$BUILD_DIR/tt_$NAME" ]; then
  echo "LINK FAILED for $NAME:" >&2
  grep -iE 'error|undefined|multiple' $BUILD_DIR/oracle_link_$NAME.log | head -8 >&2
  exit 1
fi
# Remove any stale subprocess-style binary from prior runs so a failed
# link cannot be masked by diffing an old binary (false-pass bug).
rm -f "/tmp/t_port_$NAME"
# Subprocess-style oracles exec a fixed /tmp/t_port_<name> binary (they
# re-run the harness themselves and compare against live Python). Point
# that path at the freshly-built harness so they see current code.
ln -sfn "$BUILD_DIR/tt_$NAME" "/tmp/t_port_$NAME" 2>/dev/null || true

FAIL=0
# Load normalize rules from registry.json (strip_patterns: regex on raw output;
# strip_fields: JSON object keys to drop before diffing). This is what lets the
# harness correctly classify FALSE FAPs (env noise — real cwd, git status,
# hostname) as non-failures instead of spurious MISMATCHes.
REG_JSON="${REG_JSON:-tests/oracle/registry.json}"
STRIP_PATTERNS=$("$PY" -c "
import json,sys
d = json.load(open('$REG_JSON'))
norm = d.get('normalize',{}).get('strip_patterns',[])
port = d.get('ports',{}).get(sys.argv[1],{})
port_strip = port.get('strip_patterns',[]) if isinstance(port, dict) else []
print('\\n'.join(list(norm) + list(port_strip)))
" "$NAME" 2>/dev/null)
STRIP_FIELDS=$("$PY" -c "import json,sys; print(' '.join(json.load(open('$REG_JSON')).get('normalize',{}).get('strip_fields',[])))" 2>/dev/null)
# Per-port float/int canonicalization: C's JSON writer renders integral
# doubles as "1" while Python emits "1.0"; when the port opts in
# (normalize.float_int_equal), rewrite integral floats to ints on both sides.
FLOAT_INT_EQUAL=$("$PY" -c "import json,sys; d=json.load(open('$REG_JSON')); print(1 if d.get('ports',{}).get(sys.argv[1],{}).get('normalize',{}).get('float_int_equal') else 0)" "$NAME" 2>/dev/null)

normalize_out() {  # $1 = file
  local f="$1"
  local tmp; tmp="$(tmpfile)"
  cp "$f" "$tmp"
  # Canonicalize every JSON line: JSON object key order is semantically
  # irrelevant, but C's serializer emits insertion order while Python's
  # json.dumps may sort (or preserve dict order). Always re-serialize with
  # sort_keys=True so both sides compare on sorted keys (byte-identical diff).
  # float_int_equal additionally coerces integral floats to ints (C's JSON
  # writer renders 1.0 as "1" via %.0f; Python emits "1.0").
  tmp2="$(tmpfile)"
  "$PY" - "$tmp" "$FLOAT_INT_EQUAL" <<'PY' > "$tmp2"
import json, sys
FLOAT_INT_EQUAL = sys.argv[2] == "1"
def canon(o):
    if FLOAT_INT_EQUAL and isinstance(o, float) and o == int(o):
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, list):
        return [canon(v) for v in o]
    return o
out = []
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    line = line.strip()
    if not line: continue
    try:
        out.append(json.dumps(canon(json.loads(line)), sort_keys=True))
    except Exception:
        out.append(line)
sys.stdout.write('\n'.join(out) + '\n')
PY
  mv "$tmp2" "$tmp"
  # strip_patterns: treat each as a Python regex, remove every match
  if [ -n "$STRIP_PATTERNS" ]; then
    tmp2="$(tmpfile)"
    "$PY" - "$tmp" "$STRIP_PATTERNS" <<'PY' > "$tmp2" || cp "$tmp" "$tmp2"
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
    tmp2="$(tmpfile)"
    "$PY" - "$tmp" $STRIP_FIELDS <<'PY' > "$tmp2"
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
# Deterministic set iteration: several oracles (plumber_fuzz) depend on the
# iteration order of Python frozensets (invisible-unicode blocklist), which
# changes with PYTHONHASHSEED. Pin it to 0 so live-Python order is stable and
# matches the C port's fixed tables.
ISO="$ISO PYTHONHASHSEED=0"
# Plugin discovery: both the C harness and the Python oracle resolve the
# bundled plugins directory independently — the C via getbundled_plugins_dir()
# (HERMES_BUNDLED_PLUGINS env or cwd-relative "plugins"), the Python via
# get_bundled_plugins_dir() (HERMES_BUNDLED_PLUGINS or Path(__file__).parent).
# Point both at the slermes repo's own plugins/ so they discover the same
# manifests. $ROOT is the slermes root (runners/ is 3 dirs deep).
ISO="$ISO HERMES_BUNDLED_PLUGINS=$ROOT/plugins"
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

# ---- phase 1: run the C harness for every case --------------------------
BATCH_TSV="$BUILD_DIR/batch_$NAME.tsv"
: > "$BATCH_TSV"
for f in "$FIX"/*.in; do
  case="$(basename "$f" .in)"
  extra=""
  [ -f "$FIX/args.$case" ] && extra="$(cat "$FIX/args.$case")"
  # Fixtures use placeholders the harness/oracle must resolve:
  #   @SBX@ -> TMPH (the isolated shared home exported to both sides)
  #   @NOW@ -> a fixed timestamp (so time-relative ops are deterministic and
  #            identical on both the C and Python sides)
  # We substitute both in a temp copy and feed that to BOTH sides.
  FSUB="$(tmpfile)"
  # @NOTREPO@ -> a path guaranteed to be OUTSIDE any git repo and free of project
  # markers (so functions that probe cwd/git return deterministic empty output
  # instead of leaking the developer's real repo root / branch / commit hashes).
  # /tmp is universally non-repo and marker-free.
  # Binary fixtures (e.g. session_detail's *.in IS a seeded sqlite DB) must NOT
  # go through sed — placeholder substitution on binary data corrupts it.
  # Detect via NUL-byte DENSITY in the first 8KB (a real DB is mostly NULs; a
  # text fixture may contain at most a stray NUL) and copy verbatim instead.
  if head -c 8192 "$f" | tr -cd '\0' | wc -c | awk '{exit ($1 > 16) ? 0 : 1}'; then
    cp "$f" "$FSUB"
  else
    sed -e "s#@SBX@#$TMPH#g" -e "s#@NOW@#1700000000.0#g" -e "s#@NOTREPO@#/tmp#g" "$f" > "$FSUB"
  fi
  # The C engine resolves its home via SLERMES_HOME (kanban_home()), while the
  # Python profiles module resolves via HERMES_HOME (get_default_hermes_root()).
  # They honor DIFFERENT env vars, so we export BOTH to the same temp dir for
  # BOTH the harness and the oracle.
  # Two harness conventions: some read the fixture from argv[1], some read it
  # from stdin (fgets(stdin)). Feed it the way the harness expects; getting
  # this wrong makes a harness run, produce nothing, and look like an
  # oracle-side failure.
  if grep -q "stdin" "$HARNESS"; then
    env $ISO "$ROOT/tests/oracle/build/tt_$NAME" $extra < "$FSUB" \
      > "$BUILD_DIR/oracle_${NAME}_c_${case}.json" 2>/dev/null || true
  else
    env $ISO "$ROOT/tests/oracle/build/tt_$NAME" "$FSUB" $extra \
      > "$BUILD_DIR/oracle_${NAME}_c_${case}.json" 2>/dev/null || true
  fi
  printf '%s\t%s\t%s\t%s\n' "$case" "$FSUB" \
         "$BUILD_DIR/oracle_${NAME}_c_${case}.json" "$extra" >> "$BATCH_TSV"
done

# ---- phase 2: one Python process runs every case ------------------------
# Spawning the oracle per case cost 182 interpreter startups on a 182-case
# port, which dwarfed the C side. One process, modules imported once.
SPEC="$BUILD_DIR/batch_$NAME.json"
if ! "$PY" "tests/oracle/runners/make_batch_spec.py" \
       "$BATCH_TSV" "$ORACLE" "$BUILD_DIR" > /dev/null 2>&1; then
  echo "WARN: batch spec builder failed for $NAME" >&2
fi
BATCH_OUT="$BUILD_DIR/batch_$NAME.results.json"
if [ -n "$SPEC" ] && env $ISO "$PY" "tests/oracle/_oracle_boot.py" --batch "@$SPEC" \
     > "$BATCH_OUT" 2>"$BUILD_DIR/batch_$NAME.err"; then
  :
else
  echo "WARN: batch oracle failed for $NAME; see $BUILD_DIR/batch_$NAME.err" >&2
fi

# ---- phase 3: compare ---------------------------------------------------
while IFS=$'\t' read -r case FSUB C_OUT EXTRA; do
  [ -z "$case" ] && continue
  ORACLE_OUT="$BUILD_DIR/oracle_${NAME}_py_${case}.json"
  ORACLE_RC=0
  [ -s "$ORACLE_OUT" ] || ORACLE_RC=1
  # A missing/empty oracle output must never compare equal to anything. Two
  # empty files are byte-identical, so without this guard a failed oracle
  # reports MATCH -- the false pass this runner was rebuilt to prevent.
  if [ "$ORACLE_RC" -ne 0 ]; then
    echo "$case: MISMATCH (oracle produced no output)"; FAIL=1
    rm -f "$FSUB"
    continue
  fi
  normalize_out "$ORACLE_OUT" > "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json"
  normalize_out "$BUILD_DIR/oracle_${NAME}_c_${case}.json" > "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json"
  if files_equal "$BUILD_DIR/oracle_${NAME}_c_${case}_norm.json" "$BUILD_DIR/oracle_${NAME}_py_${case}_norm.json"; then
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
done < "$BATCH_TSV"
rm -rf "$TMPH"
exit $FAIL