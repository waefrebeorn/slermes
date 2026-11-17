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

# Phase timing, off by default. Speed work needs measured phase costs, not
# assumptions: ORACLE_TIME=1 prints elapsed milliseconds per phase to stderr.
ORACLE_TIME="${ORACLE_TIME:-0}"
_T_MARK=$(date +%s%N)
mark() {  # $1 = phase label
  [ "$ORACLE_TIME" = 1 ] || return 0
  local _now; _now=$(date +%s%N)
  printf 'TIME %-22s %6d ms\n' "$1" $(( (_now - _T_MARK) / 1000000 )) >&2
  _T_MARK=$_now
}

NAME="${1:?usage: run_oracle.sh <port_name> [fixtures_subdir]}"
SUB="${2:-$NAME}"
FIX="tests/oracle/fixtures/$SUB"
# Prefer the registry's explicit harness path (oracles may map to a
# differently-named harness, e.g. t_port_error_classifier_upstream.c for the
# error_classifier port); fall back to the conventional t_port_<name>.c.
HARNESS="tests/t_port_$NAME.c"
if [ -f tests/oracle/registry.json ]; then
    eval "$("$PY" tests/oracle/runners/registry_lookup.py "$NAME" 2>/dev/null)"
fi
[ -n "${REG_HARNESS:-}" ] && HARNESS="$REG_HARNESS"
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
if [ "${REG_STATUS:-}" = "incomplete" ]; then
    echo "SKIP $NAME: declared incomplete (${REG_REASON:-})"
    exit 0
fi

[ -f "$HARNESS" ] || { echo "MISSING $HARNESS"; exit 2; }
[ -f "$ORACLE" ]  || { echo "MISSING $ORACLE"; exit 2; }
[ -d "$FIX" ]     || { echo "MISSING fixture dir $FIX"; exit 2; }

mark setup
# Extract the EXACT link command `make` uses for the real `slermes` binary and
# adapt it for the oracle harness. This guarantees the oracle links the same
# resolvable object/lib/static-archive closure the shipped binary uses — no
# missing whisper .a, no SDL/orphan-object drag-in, no hand-maintained lib list.
# We grab the gcc link recipe (from the `gcc ... -o slermes` line through the
# final `libwhisper.a` static archive — the true end of the link command), drop
# the output target, and swap src/main.o for the oracle harness .c. make -n
# escapes quotes for display, so we strip \" and join continuations.
#
# This depends only on the makefiles, not on the port, so it is computed ONCE
# per tree state and shared by every port. `make -B -n` forces make to consider
# rebuilding the whole tree, which made it the largest fixed cost per port.
LINKCACHE="$BUILD_DIR/linkcmd.cache"
LINKKEY=$(stat -c '%n %Y %s' Makefile build/*.mk 2>/dev/null | sha256sum | cut -d' ' -f1)
if [ -s "$LINKCACHE" ] && [ "$(head -n 1 "$LINKCACHE")" = "$LINKKEY" ]; then
  LINKCMD=$(tail -n +2 "$LINKCACHE")
fi
if [ -z "${LINKCMD:-}" ]; then
  LINKCMD=$(make -B -n slermes 2>/dev/null \
    | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' \
    | sed -e ':a;N;$!ba;s/\\\n//g' \
    | sed 's/  */ /g; s/\\"//g')
  # ccache cannot exec the compiler from a Hermes-promoted background process
  # ("execute_noreturn of ... failed: No such file or directory") even though the
  # same gcc runs fine directly. The harness link is one invocation and gains
  # nothing from caching, so drop the wrapper.
  LINKCMD="${LINKCMD#ccache }"
  { printf '%s\n' "$LINKKEY"; printf '%s\n' "$LINKCMD"; } > "$LINKCACHE"
fi

mark make-probes
# Normalize: replace the output target and the main object.
LINKCMD=${LINKCMD// -o slermes / -o "$BUILD_DIR/tt_$NAME" }
LINKCMD=${LINKCMD//src\/main.o /$HARNESS }
# The slermes link recipe passes its objects through @slermes.link.rsp, a file
# the recipe creates and deletes. `make -n` never creates it, so the extracted
# command named a nonexistent file, the link failed, and -- because a failed
# run was not treated as a failure -- the oracle reported MATCH against an
# EMPTY C-side output. Substitute the real closure from `make print-objs`.
# Separate cache: the object list also depends only on the makefiles.
OBJCACHE="$BUILD_DIR/objs.cache"
if [ -s "$OBJCACHE" ] && [ "$(head -n 1 "$OBJCACHE")" = "$LINKKEY" ]; then
  OBJS=$(tail -n +2 "$OBJCACHE")
fi
if [ -z "${OBJS:-}" ]; then
  OBJS=$(make -s --no-print-directory print-objs 2>/dev/null)
  if [ -n "$OBJS" ]; then
    { printf '%s\n' "$LINKKEY"; printf '%s\n' "$OBJS"; } > "$OBJCACHE"
  fi
fi
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
# Comparator controls. A comparator that cannot fail reports MATCH forever and
# makes the whole suite meaningless. These exercise the REAL comparator --
# canon_compare.verdict, the code path phase 3 uses. Controlling a separate
# helper would validate something that no longer does the comparing.
if ! "$PY" - <<'PY'
import importlib.util
s = importlib.util.spec_from_file_location(
    "canon_compare", "tests/oracle/runners/canon_compare.py")
cc = importlib.util.module_from_spec(s)
s.loader.exec_module(cc)
cfg = {"strip_patterns": [], "strip_fields": [], "float_int_equal": False}
# positive control: differing input must be a mismatch
v, _, _ = cc.verdict('{"a":1}\n', '{"a":2}\n', cfg)
assert v == "MISMATCH", v
# negative control: identical input must match
v, _, _ = cc.verdict('{"a":1}\n', '{"a":1}\n', cfg)
assert v == "MATCH", v
# key order must not matter -- normalisation sorts keys
v, _, _ = cc.verdict('{"a":1,"b":2}\n', '{"b":2,"a":1}\n', cfg)
assert v == "MATCH", v
# empty oracle output must never compare equal to real output
v, _, _ = cc.verdict('{"a":1}\n', '', cfg)
assert v == "MISMATCH", v
print("comparator controls: OK")
PY
then
  echo "FATAL: comparator controls failed" >&2
  exit 2
fi
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
# without escaping the isolated home. The link target must be the Hermes
# CHECKOUT (~/.hermes/hermes-agent/hermes_cli/...), not the slermes parent:
# linking the parent put C:/Users/eman5/hermes_cli on the path, which does not
# exist, so every oracle that resolves the live source through Path.home() died.
for _cand in "$ROOT/../hermes-agent" "$ROOT/../hermes-agent-dev" "$ROOT/../hermes"; do
    if [ -f "$ROOT/../$(basename "$_cand")/hermes_cli/plugins.py" ]; then
        DEV_ROOT="$(cd "$_cand" && pwd)"
        break
    fi
done
DEV_ROOT="${DEV_ROOT:-$(cd "$ROOT/.." && pwd)}"
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
# Cache the harness object by the CONTENT of its transitive includes, not just
# the harness source: a harness #includes the port's .c, so hashing only the
# harness would reuse a stale object after a port edit and report a parity
# result for code that was never compiled. ~3.3 s saved per unchanged port.
HCACHE="$BUILD_DIR/cache"
mkdir -p "$HCACHE"
HK=$("$PY" tests/oracle/runners/harness_deps.py "$HARNESS" "$COMPILE")
if [ -f "$HCACHE/$HK.o" ]; then
  cp "$HCACHE/$HK.o" "$HARNESS_OBJ"
  echo "harness compile: cache hit" >&2
elif bash -c "$COMPILE" >$BUILD_DIR/oracle_compile_$NAME.log 2>&1; then
  cp "$HARNESS_OBJ" "$HCACHE/$HK.o"
else
  echo "FATAL: harness did not compile -> $HARNESS_OBJ" >&2
  head -12 "$BUILD_DIR/oracle_compile_$NAME.log" >&2
  exit 2
fi
mark harness-compile
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
# Cache the linked harness. The link depends only on the harness object, the
# closure objects and the link command, so it is keyed on those -- object
# MTIMES, which is what make itself uses: any rebuild changes them, and a
# rebuilt object must not be linked against a stale binary.
LK=$("$PY" - "$RSP" "$HK" "$LINKCMD -Wl,--allow-multiple-definition" <<'PY'
import hashlib, sys
from pathlib import Path
rsp, hk, cmd = sys.argv[1], sys.argv[2], sys.argv[3]
h = hashlib.sha256()
h.update(cmd.encode("utf-8")); h.update(hk.encode("utf-8"))
for line in Path(rsp).read_text(encoding="utf-8").splitlines():
    line = line.strip()
    if not line:
        continue
    try:
        h.update(f"{line}:{Path(line).stat().st_mtime_ns}".encode("utf-8"))
    except OSError:
        h.update(line.encode("utf-8"))
print(h.hexdigest())
PY
)
if [ -f "$HCACHE/bin_$LK" ]; then
  cp "$HCACHE/bin_$LK" "$BUILD_DIR/tt_$NAME"
  echo "harness link: cache hit" >&2
elif bash -c "$LINKCMD -Wl,--allow-multiple-definition" >$BUILD_DIR/oracle_link_$NAME.log 2>&1; then
  cp "$BUILD_DIR/tt_$NAME" "$HCACHE/bin_$LK"
else
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
# The batch oracle writes JSON that legitimately contains non-ASCII (U+2500 box
# rules, accented text). On a cp1252 console stdout, print() raises
# UnicodeEncodeError mid-line, truncating the output and turning a correct port
# into a MISMATCH. Force UTF-8 on the interpreter's stdio for both sides.
ISO="$ISO PYTHONIOENCODING=utf-8 PYTHONUTF8=1"

mark harness-link
# ---- phase 1: run the C harness for every case --------------------------
BATCH_TSV="$BUILD_DIR/batch_$NAME.tsv"
: > "$BATCH_TSV"
# Substitution happens in ONE Python process for the whole directory. Doing it
# per case cost `head|tr|wc|awk` (~103 ms) plus `sed` (~66 ms) -- more process
# startup than the C harness it was preparing input for.
PREP_TSV="$BUILD_DIR/prep_$NAME.tsv"
"$PY" tests/oracle/runners/prep_fixtures.py "$FIX" "$BUILD_DIR/subst_$NAME" "$TMPH" > "$PREP_TSV"
while IFS=$'\t' read -r case FSUB extra; do
  [ -z "$case" ] && continue
  # Defence in depth: a CR smuggled into `extra` becomes argv[1] for the C
  # harness, which then fails to open it and prints nothing -- 10 ports read as
  # "C produced no output" before the producer was found. Strip it here too so
  # no future producer can reintroduce the class.
  extra=${extra%$'
'}
  case=${case%$'
'}
  # Fixtures use placeholders the harness/oracle must resolve: @SBX@ -> the
  # isolated shared home, @NOW@ -> a fixed timestamp (so time-relative ops are
  # deterministic and identical on both the C and Python sides), @NOTREPO@ ->
  # a path guaranteed to be outside any git repo and free of project markers.
  # Binary fixtures (*.in that is really a seeded sqlite DB) are copied
  # verbatim: text substitution would corrupt them.
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
done < "$PREP_TSV"

mark c-cases
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

# ---- phase 3: report verdicts the batch process already computed ---------
# Normalisation and comparison happen inside the single oracle interpreter.
# Doing it here cost three Python spawns per case (~170 ms each), which for a
# 182-case port was ~93 s of interpreter startup -- more than the C side it
# was measuring.
mark oracle+compare
VERDICTS="$BUILD_DIR/batch_$NAME.verdicts.json"
if [ -s "$VERDICTS" ]; then
  if "$PY" - "$VERDICTS" <<'PY'
import json, sys
vs = json.load(open(sys.argv[1], encoding="utf-8"))
bad = 0
for v in vs:
    line = f"{v['name']}: {v['verdict']}"
    if v.get("reason"):
        line += f" ({v['reason']})"
    print(line)
    if v["verdict"] != "MATCH":
        bad += 1
        c = (v.get("c_norm") or "").strip()
        p = (v.get("py_norm") or "").strip()
        if c: print("  C : " + c[:400])
        if p: print("  PY: " + p[:400])
sys.exit(1 if bad else 0)
PY
  then
    echo "cases: MATCH"
  else
    echo "cases: MISMATCH"; FAIL=1
  fi
else
  echo "cases: MISMATCH (no verdicts produced)"; FAIL=1
fi
rm -rf "$TMPH"
exit $FAIL