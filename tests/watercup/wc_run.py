#!/usr/bin/env python3
"""tests/watercup/wc_run.py — PROJECT-WIDE water-cup parity runner (v3).

Generic, registry-driven engine for the trace-cup deep gate. Each gate lives
in tests/watercup/gates/<name>/ and declares itself via gate.json:

    {
      "name": "usage_pricing_format_cost_label",
      "subsystem": "agent",
      "kind": "cup",                  # "cup" (trace diff) | "unit" (assertions)
      "c_src": "harness.c",           # traced C copy of the ported fn
      "c_fn": "format_cost_label",    # argv-dispatch name inside harness.c
      "py_module": "agent/usage_pricing.py",
      "py_fn": "format_cost_label",   # LIVE Python twin (imported, traced)
      "inputs": ["0.0", "5.0", ...],  # correlated input walk (shared C/Py)
      "expect": ["<16-hex cup>", ...] # optional: pinned golden cups
    }

The engine:
  1. builds <gate>/harness.c (compiled with watercup.h) -> build/<name>
     — rebuilds whenever harness.c, watercup.h, or any extra_src is newer
     than the binary (stale-binary protection; the #1 source of false REDs).
  2. runs it over gate["inputs"] -> one cup hash per input.
  3. imports the LIVE <py_module> and runs <py_fn> under the water-cup tracer
     (the gate's twin.py wraps it to trace the SAME branch ladder as the C
     harness — rung-order alignment is the contract; see the skill).
  4. diffs the two cup sequences; any mismatch => FAP for that gate.

The design is language-agnostic: the cup is a byte stream of B/L/R/r/V events
folded with FNV-1a. Any source translated to C11 — Python, TypeScript, Rust —
carves the same shape when the logic is identical. A stub that returns a
constant cannot reproduce the real trace, so the cup never matches.

Run:
    python3 wc_run.py                              # all gates
    python3 wc_run.py <gate> [<gate>...]           # selected gates
    python3 wc_run.py --list                       # list gate names
    python3 wc_run.py --verbose <gate>             # per-input event streams
    python3 wc_run.py --json                       # machine-readable report
    python3 wc_run.py --coverage                   # gated-vs-ungated summary

Exit code is fail-closed: non-zero if ANY gate has a cup mismatch (or a build
error, or a golden-cup drift).
"""
import argparse
import contextlib
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from contextlib import suppress

os.environ["WATERCUP_NO_BROWSER"] = "1"

# The gate's Python twin imports the quarry (agent/, utils/, gateway/), which resolves its own home
# through Path.home(); a launcher that strips USERPROFILE makes every gate fail with "Could not
# determine home directory" while the C side is untouched. Default the three variables the same way
# build/config.mk and tests/unit/run_unit.sh do.
os.environ.setdefault("USERPROFILE", "C:/Users/eman5")
os.environ.setdefault("LOCALAPPDATA", "C:/Users/eman5/AppData/Local")
os.environ.setdefault("HOME", "C:/Users/eman5")

HERE = os.path.dirname(os.path.abspath(__file__))
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(HERE, "build")
# gc-sections: compile with -ffunction-sections/-fdata-sections and link with
# --gc-sections so the linker drops code unreachable from main. A gate whose
# port file has heavy-dep siblings links using only the traced fn's call graph.
_GC_FLAGS = ["-ffunction-sections", "-fdata-sections"]
_GC_LINK = ["-Wl,--gc-sections"]
# Repo root is two levels up: tests/watercup -> tests -> slermes.
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from watercup_paths import ensure_sys_path
ensure_sys_path()  # quarry + watercup dir by cross-OS rules

# ---------------------------------------------------------------------------
# Operator-state isolation.
#
# The LIVE twins import real Hermes modules by path. Some of those modules own
# GLOBAL OPERATOR STATE whose mutation escapes the test process: agent/estop.py
# writes/removes the ESTOP sentinel at $HERMES_HOME/ESTOP, and `hermes pause`
# reads that same file. A watercup run launched from inside a live Hermes
# session therefore ENGAGED THE REAL PAUSE on the operator's own gateway —
# every subsequent gateway turn answered with the "Hermes is paused" notice.
#
# Fix: point HERMES_HOME (and SLERMES_HOME, which the C port honors first) at a
# throwaway dir under build/, so the sentinel lands there. Same convention the
# oracle runner already uses (tests/oracle/runners/run_oracle.sh exports
# SLERMES_HOME/HERMES_HOME/HOME to an isolated temp home). Env, not a config
# file: get_process_hermes_home() reads the env var live, and the profile
# layout (<root>/profiles/<name>) resolves inside the throwaway dir, so the
# fleet-root check in estop._candidate_sentinel_paths() also stays contained.
#
# An operator can still force the real home (e.g. to debug a sentinel-path
# gate) with WATERCUP_REAL_HERMES_HOME=1 — documented in README.md.
_WC_SANDBOX_HOME = os.path.join(BUILD, "home_watercup")
if not os.environ.get("WATERCUP_REAL_HERMES_HOME"):
    os.makedirs(_WC_SANDBOX_HOME, exist_ok=True)
    os.environ["SLERMES_HOME"] = _WC_SANDBOX_HOME
    os.environ["HERMES_HOME"] = _WC_SANDBOX_HOME

# Install browser guard + comprehensive interactive-flow guard BEFORE any LIVE
# module imports. This patches all browser-spawn paths, hermes CLI entry points,
# and web-research tool flows so watercup can never launch a real browser,
# spawn a child process, or touch a live, interactive hermes CLI during import
# or execution.
import browser_guard  # noqa: F401
try:
    import watercup_guard  # noqa: F401
    watercup_guard.install()
except Exception:
    pass
from watercup_py import Cup  # noqa: E402

RESULT_FILE = os.path.join(HERE, "build", "results.jsonl")


def _check_interpreter_deps():
    """Fail loudly when the interpreter is missing a dependency the twins need.

    The fleet runs under whatever ``python3`` resolves to. Hermes' bundled
    interpreter (``~/.hermes/tools/python-3.14...``) has no PyYAML, and the
    context-compressor / gateway-status twins import ``yaml`` transitively, so
    those gates died with an opaque "ERROR: No module named 'yaml'" buried in
    the per-gate output — indistinguishable from a real parity failure.

    Raise a SystemExit-free, actionable error naming the interpreter and the
    project venv that does have the deps. WATERCUP_SKIP_DEP_CHECK=1 opts out
    (e.g. deliberately running the yaml-free subset).
    """
    if os.environ.get("WATERCUP_SKIP_DEP_CHECK"):
        return
    try:
        import yaml  # noqa: F401
        return
    except ImportError:
        pass
    venv_py = os.path.join(os.path.dirname(REPO), "venv", "bin", "python")
    hint = ("  this venv has the deps: %s" % venv_py
            if os.path.exists(venv_py) else
            "  install them: python3 -m pip install pyyaml")
    raise RuntimeError(
        "watercup: interpreter %s has no PyYAML; gates importing yaml "
        "transitively will fail with a misleading 'No module named yaml'.\n"
        "  run the fleet with a Python that has the project deps:\n"
        "%s\n"
        "  or skip this check with WATERCUP_SKIP_DEP_CHECK=1"
        % (sys.executable, hint))


# ──────────────────────────────────────────────────────────────────────
# Canonical value encodings shared C<->Py (mirror watercup.h WCUP_VALSTR).
# ──────────────────────────────────────────────────────────────────────

def enc_double(v):
    return "%.17g" % float(v)


def enc_bool(v):
    return "True" if v else "False"


def enc_str(v):
    return "" if v is None else str(v)


# ──────────────────────────────────────────────────────────────────────
# Gate discovery + build (stale-binary aware)
# ──────────────────────────────────────────────────────────────────────

def discover_gates():
    """Sorted gate names: every gates/<dir> with a gate.json."""
    return sorted(
        d for d in os.listdir(GATES)
        if os.path.isfile(os.path.join(GATES, d, "gate.json"))
    )


def _newer_than(path, mtime):
    try:
        return os.path.getmtime(path) > mtime
    except OSError:
        return False


def _cc_env():
    """Environment for compiler/linker children, with a temp dir they can use.

    On this box the runner inherits TMP/TMPDIR in a POSIX spelling; gcc's spawn of
    cc1/as then dies with rc=1 and an EMPTY stderr, and the gate is reported as a build
    failure with nothing to act on. Give every child a native temp directory.
    """
    env = dict(os.environ)
    home = None
    for k in ("TMP", "TEMP", "TMPDIR"):
        v = env.get(k) or ""
        if len(v) > 2 and v[1] == ":" and v[2] in ("\\", "/"):
            home = home or v
        env.pop(k, None)
    if not home:
        home = os.path.abspath(BUILD)
        try:
            os.makedirs(home, exist_ok=True)
        except OSError:
            pass
    env["TMP"] = env["TEMP"] = env["TMPDIR"] = home
    return env


def _compiler():
    """Compiler command for harness builds, ccache-fronted when available.

    Cold-building 22.6k harnesses is the dominant cost of a clean fleet run.
    ccache makes a re-run (same sources, e.g. after a twin-only edit) nearly
    free. Honoured only when ccache is actually on PATH, so a machine without
    it is unaffected. Override with WATERCUP_CC (e.g. WATERCUP_CC=cc to force
    the bare compiler, or a specific cross-compiler).
    """
    override = os.environ.get("WATERCUP_CC", "").strip()
    if override:
        return override.split()
    # Winlibs gcc FIRST, by full path. On this box shutil.which("gcc") finds
    # C:\msys64\mingw64\bin\gcc.EXE, which is not the compiler the project builds
    # with and which fails on these Windows paths with rc=1 and an EMPTY stderr --
    # so every gate that needed it read as a build failure with nothing after the
    # colon. The MSYS2 toolchain only works from its own bash with /mingw64/bin first.
    _winlibs = "C:/winlibs/mingw64/bin/gcc.exe"
    if os.name == "nt" and os.path.exists(_winlibs):
        cc = _winlibs
    else:
        cc = "gcc" if shutil.which("gcc") else "cc"
    if shutil.which("ccache") and _ccache_works(cc):
        return ["ccache", cc]
    return [cc]


_CCACHE_OK = []


def _ccache_works(cc):
    """Probe `ccache <cc> -c` once before trusting it with 22.6k compiles.

    ccache can be on PATH and still refuse to compile -- a cache directory it cannot write, a
    HOME it cannot resolve -- and when it does it exits non-zero with an EMPTY stderr. The runner
    then reported "BUILD FAIL (compile harness.c)" with nothing after the colon and the gate looked
    like a port gap. CCACHE_DISABLE=1 made the same gate compile, which is how this was found.
    """
    if _CCACHE_OK:
        return _CCACHE_OK[0]
    ok = True
    try:
        tmpdir = os.path.join(BUILD, "_ccache_probe")
        os.makedirs(tmpdir, exist_ok=True)
        src = os.path.join(tmpdir, "probe.c")
        with open(src, "w") as f:
            f.write("int main(void){return 0;}\n")
        r = subprocess.run(["ccache", cc, "-c", src, "-o", os.path.join(tmpdir, "probe.o")],
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        ok = r.returncode == 0
        if not ok:
            sys.stderr.write("watercup: ccache is on PATH but cannot compile (rc=%d: %s); "
                             "falling back to %s\n" % (r.returncode, (r.stderr or "").strip()[:200], cc))
    except Exception as e:  # noqa: BLE001 - a probe must never break the fleet
        ok = False
        sys.stderr.write("watercup: ccache probe failed (%s); falling back to %s\n" % (e, cc))
    _CCACHE_OK.append(ok)
    return ok


def _linker():
    """Bare compiler for LINK steps (ccache refuses links: no -c)."""
    override = os.environ.get("WATERCUP_CC", "").strip()
    if override:
        return override.split()[0]
    return _compiler()[-1]                      # the compiler itself, never the ccache front-end


def _run_link(args):
    """Run a link command through a response file. Windows CreateProcess caps
    the command line (~32k chars); a full-object harness link exceeds it and
    dies with WinError 206 before the compiler ever runs. gcc @file parsing
    treats backslash as an escape, so paths are normalised to forward slashes.
    """
    os.makedirs(BUILD, exist_ok=True)
    rsp = os.path.join(BUILD, "link_%d.rsp" % os.getpid())
    norm = _EXTRA_LIBDIR + [a.replace("\\", "/") for a in args[1:]]
    with open(rsp, "w") as f:
        f.write(" ".join('"%s"' % a if (" " in a or "\t" in a) else a
                         for a in norm))
    try:
        return subprocess.run([args[0], "@" + rsp],
                              capture_output=True, text=True, encoding="utf-8", errors="replace",
                              env=_cc_env())
    finally:
        try:
            os.remove(rsp)
        except OSError:
            pass


# Winlibs extra-libs live outside the default search path on the dev box;
# every harness link needs them for -lssl/-lcrypto/-lpcre2-8/-ltre.
_EXTRA_LIBDIR = (["-L", "C:/winlibs/extra-libs"]
                 if os.path.isdir("C:/winlibs/extra-libs") else [])


def _plat_syslibs(libs):
    """Drop POSIX-only libs on Windows and add the Winsock set the vendored
    http/crypto objects need there."""
    if os.name == "nt":
        drop = {"-ldl", "-lpthread", "-lasound"}
        return [l for l in libs if l not in drop] + [
            "-lws2_32", "-lgdi32", "-lcrypt32", "-ladvapi32", "-luser32",
            "-lshell32", "-lole32", "-luuid"]
    return libs


def _compile_one(g, src_path):
    """Compile one extra_src .c to a gate-prefixed .o. Returns the path or None."""
    tag = g["name"]
    obj = os.path.join(BUILD, "%s.xr.%s.o" % (tag, os.path.basename(src_path)))
    cf = _resolve_cflags(g)
    ccmd = _compiler() + ["-I", HERE] + cf + ["-c", src_path, "-o", obj]
    r = subprocess.run(ccmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                       env=_cc_env())
    return obj if r.returncode == 0 else None


def _compile_stub_shadowed(g, keep_srcs):
    """Compile infrastructure_stubs.c with symbols duplicated elsewhere renamed.

    The stub is a legacy fallback providing 320 symbols, many of which nothing
    else defines — so it must stay on the link line. But it also re-defines
    symbols the real units provide (hermes_get_home in paths.c;
    _parse_absolute_timestamp, _exhausted_ttl, get_custom_provider_pool_key in
    credential_pool_custom.c). Linking both is a "multiple definition" error.

    Compute the overlap against the files actually on this gate's link line and
    macro-rename the stub's copies away — the real implementations win.
    """
    try:
        import fix_all_gates as _fag
    except ImportError:
        return None
    src = os.path.join(REPO, _fag.STUB_SRC)
    if not os.path.isfile(src):
        return None
    txt = open(src, errors="ignore").read()
    stub_defs = set(_fag._defined_names(src, txt))
    shadowed = set()
    for c in keep_srcs:
        if not os.path.isfile(c):
            continue
        shadowed |= stub_defs & set(
            _fag._defined_names(c, open(c, errors="ignore").read()))
    for sym in sorted(shadowed):
        # Rename sym -> sym_stub_shadowed, but do NOT touch a symbol that is
        # ALREADY shadowed (idempotent): a second re-shadow pass over text that
        # still contains the original `sym` as a substring of
        # `sym_stub_shadowed` would double-rename. Read fresh from src each
        # call, so guard the rename target explicitly.
        txt = re.sub(r'(?<![\w])%s(?![\w])' % re.escape(sym),
                     sym + "_stub_shadowed", txt)
    tmp = os.path.join(BUILD, "%s.stub.c" % g["name"])
    os.makedirs(BUILD, exist_ok=True)
    with open(tmp, "w") as f:
        f.write(txt)
    obj = os.path.join(BUILD, "%s.stub.o" % g["name"])
    ccmd = _compiler() + ["-I", HERE] + _resolve_cflags(g) + \
           ["-c", tmp, "-o", obj]
    r = subprocess.run(ccmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                       env=_cc_env())
    return obj if r.returncode == 0 else None


def _resolve_cflags(g):
    """Absolute, include-root-complete cflags for a gate's harness build."""
    cflags = g.get("cflags", [])
    cflags = [os.path.join(REPO, c) if not c.startswith("-") else c
              for c in cflags]
    # Resolve combined -Ipath flags to absolute (REPO-relative) so they
    # work regardless of the invoking CWD. E.g. "-Ilib/libhive" → "-I<REPO>/lib/libhive"
    cflags = [c[:2] + os.path.join(REPO, c[2:])
              if c.startswith("-I") and len(c) > 2 and not os.path.isabs(c[2:])
              else c for c in cflags]
    # Gate cflags carry a bare "-I" with no path, so vendored headers
    # (lib/libhttp/http.h, lib/libplugin/plugin.h and siblings) never resolve
    # and the harness fails to compile. Add the project's standard include
    # roots plus every lib/ subdir that ships headers.
    std_inc = []
    roots = ["include", "lib", "src"]
    libdir = os.path.join(REPO, "lib")
    if os.path.isdir(libdir):
        for d in sorted(os.listdir(libdir)):
            sub = os.path.join(libdir, d)
            if os.path.isdir(sub) and any(f.endswith(".h")
                                          for f in os.listdir(sub)):
                roots.append(os.path.join("lib", d))
    for root in roots:
        p = os.path.join(REPO, root)
        if os.path.isdir(p):
            std_inc += ["-I", p]
    # src/ subdirs ship headers too (src/tools/process_registry.h, etc.) that
    # sibling units #include by bare name. Add every src/ subdir with headers.
    srcdir = os.path.join(REPO, "src")
    if os.path.isdir(srcdir):
        for d in sorted(os.listdir(srcdir)):
            sub = os.path.join(srcdir, d)
            if os.path.isdir(sub) and any(f.endswith(".h")
                                          for f in os.listdir(sub)):
                std_inc += ["-I", sub]
    # The project's three-flag compat surface (same rule as the main build):
    # without it every POSIX shim is invisible on Windows and harnesses fail
    # on localtime_r/mkdir/etc. hermes_win32.h is _WIN32-guarded internally, so
    # these flags are safe on Linux/macOS too.
    compat = ["-include", "hermes_win32.h", "-D_GNU_SOURCE",
              "-I", os.path.join(REPO, "include", "compat")]
    # The version defines the real build passes (build/config.mk). A port unit added through
    # extra_src that mentions HERMES_VERSION failed to compile without them, and the gate read as
    # "the harness cannot build" rather than "the runner forgot a define".
    defs = ['-DHERMES_VERSION="0.0.0-slermes"',
            '-DHERMES_RELEASE_DATE="2026.9.24"',
            '-DATADIR="/share/slermes/docs"']
    # The main build points at winlibs' extra include/lib trees (build/config.mk); a port unit that
    # reaches the HTTP layer includes <openssl/ssl.h>, which lives there. Without it the gate read as
    # "openssl/ssl.h: No such file" -- a build failure that named the wrong subsystem.
    extra_inc = []
    for cand in ("C:/winlibs/extra-inc", "C:/msys64/mingw64/include"):
        if os.path.isdir(cand):
            extra_inc = ["-I", cand]
            break
    return compat + std_inc + defs + extra_inc + cflags


def build_harness(gate_dir, g, force=False):
    """Compile the gate's harness. Skips the compile when the binary is newer
    than every input (harness.c, watercup.h, extra_src, gate.json) unless
    force=True. Returns path to the binary."""
    src = os.path.join(gate_dir, g["c_src"])
    out = os.path.join(BUILD, g["name"])
    os.makedirs(BUILD, exist_ok=True)

    if not force and os.path.exists(out):
        bin_mtime = os.path.getmtime(out)
        deps = [src, os.path.join(HERE, "watercup.h"),
                os.path.join(gate_dir, "gate.json")]
        deps += [os.path.join(REPO, e) for e in g.get("extra_src", [])]
        if not any(_newer_than(d, bin_mtime) for d in deps):
            return out  # fresh — skip rebuild

    cflags = _resolve_cflags(g)
    extra = [os.path.join(REPO, e) for e in g.get("extra_src", [])]
    # The Windows system set belongs on the FIRST link, not only on the retry: a port file that
    # reaches the HTTP layer names WSAPoll/__imp_shutdown, and dropping -lws2_32 from the first
    # attempt made the gate report "undefined reference to `WSAPoll'" as if the port were missing.
    libs = _plat_syslibs(g.get("ldflags", [])) if os.name == "nt" else g.get("ldflags", [])

    # Two-step build: compile each TU to .o, then link. A single
    # `cc -o out a.c b.c` is a LINK step, and ccache explicitly refuses to
    # cache those ("No -c option found" -> "called_for_link"), so the whole
    # cold-build cost stays uncached. Compiling with -c makes every TU
    # individually cacheable — the win that matters for a 22.6k-gate fleet.
    #
    # .o names are gate-prefixed: most harnesses are all named harness.c and
    # share extra_src (lib/libjson/json.c), so a bare basename would collide
    # across gates writing into the same BUILD dir.
    objs = []
    tag = g["name"]
    sources = [(src, "%s.%s.o" % (tag, os.path.basename(src)))]
    # infrastructure_stubs.c must be compiled with its shadowed symbols renamed
    # (see _compile_stub_shadowed) rather than from the raw source.
    stub_obj = None
    extra_real = []
    for e in extra:
        if e == os.path.join(REPO, "src/infrastructure_stubs.c"):
            continue
        extra_real.append(e)
    sources += [(e, "%s.x%d.%s.o" % (tag, i, os.path.basename(e)))
                for i, e in enumerate(extra_real)]

    compile_fail = None
    for s, obj_name in sources:
        obj = os.path.join(BUILD, obj_name)
        objs.append(obj)
        if not force and os.path.exists(obj) and not _newer_than(s, os.path.getmtime(obj)):
            continue  # object fresh
        # -ffunction-sections/-fdata-sections put each fn/var in its own section
        # so the linker's --gc-sections (added to every link line below) can drop
        # code unreachable from main. This lets a gate whose port file has heavy
        # deps link using only the traced function's actual call graph, instead
        # of failing on siblings' undefined symbols.
        ccmd = (_compiler() + ["-I", HERE] + cflags + _GC_FLAGS
                + ["-c", s, "-o", obj])
        r = subprocess.run(ccmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                       env=_cc_env())
        if r.returncode != 0:
            # Print the WHOLE command. Showing only the first six words hid the source
            # file and "-c/-o", so a failed compile read as "gcc -I <dir> -include
            # hermes_win32.h -D_GNU_SOURCE" with nothing to act on.
            _msg = (r.stderr or "") + (r.stdout or "")
            compile_fail = ("BUILD FAIL (compile %s) for %s [cmd: %s]:\n%s%s" % (
                os.path.basename(s), g["name"], " ".join(ccmd),
                _msg,
                "" if _msg.strip() else
                "(the compiler exited %d with no message -- a broken ccache or a missing compiler"
                " does this; see _ccache_works)" % r.returncode))
            break
    if compile_fail:
        raise RuntimeError(compile_fail)

    if any(e == os.path.join(REPO, "src/infrastructure_stubs.c")
           for e in extra):
        stub_obj = _compile_stub_shadowed(g, [src] + extra_real)
        if stub_obj:
            objs.append(stub_obj)

    # WATERCUP_LINK=full: link the harness against every prebuilt object from the start.
    # The incremental attempts below resolve one port unit at a time, and a port unit's real
    # closure (config_py_io.c -> yaml_*, util_rename_replace, hermes_get_home, ...) is most of
    # the program -- chasing it round after round both times out and leaves the gate unbuilt.
    if os.environ.get("WATERCUP_LINK", "").strip() == "full":
        # One archive of every prebuilt object, built once per fleet run: linking ~4,900 loose
        # objects takes ~40s per gate (the driver honours --gc-sections inside a member), while an
        # archive pulls only the members the harness needs.
        _sysf = _plat_syslibs(["-lm", "-lssl", "-lcrypto", "-lz", "-lpcre2-8", "-ltre", "-lstdc++"])
        _ar = _watercup_object_archive()
        # The archive must come AFTER the gate's own objects: ld pulls archive members only for
        # symbols it has already seen, so a port object listed later leaves its own deps (logger.c
        # -> util_rename_replace) unresolved. Group both so order stops mattering.
        _vl = ([_linker(), "-Wl,--allow-multiple-definition", "-Wl,--start-group"]
               + objs + [_ar] + ["-Wl,--end-group"]
               + ["-o", out] + _GC_LINK + _EXTRA_LIBDIR + _sysf + libs)
        _rf = _run_link(_vl)
        if _rf.returncode == 0:
            return out
        raise RuntimeError("BUILD FAIL (link, full tier) for %s:\n%s"
                           % (g["name"], (_rf.stderr or "").strip()[:1500]))

    lcmd = [_linker()] + objs + ["-o", out] + _GC_LINK + ["-lm"] + libs
    r = _run_link(lcmd)
    if r.returncode != 0:
        # System libs the vendored objects need (http.o -> OpenSSL). Computed
        # here so the first retry can use them; the linker resolves them only
        # when they appear AFTER the --start-group block below.
        ssl_libs = []
        # Mirror build/config.mk's PLATFORM_LDFLAGS: -ldl -lpthread -lz
        # -lpcre2-8, plus the SSL libs. ALSA (-lasound) for audio ports, PCRE2
        # for regex-heavy ports. Probe each with -shared (a bare
        # 'cc -lcrypto -o /dev/null -x c -' fails with "undefined reference to
        # main" for EVERY lib, leaving the list empty and the symbols unlinked).
        for cand in ("-lcrypto", "-lssl", "-lz", "-ldl", "-lpthread",
                     "-lpcre2-8", "-lasound", "-lm"):
            if subprocess.run([_linker(), cand, "-shared", "-o", os.devnull,
                               "-x", "c", "-"] + _EXTRA_LIBDIR,
                              input=b"int _probe;", capture_output=True
                              ).returncode == 0:
                ssl_libs.append(cand)
        # A wider dependency sweep correctly discovers callers of the vendored
        # libs (sqlite3_*, http_*, crypto_*). Those are LIBRARIES, not .c files
        # — adding more source files without linking the vendored objects makes
        # the gate fail to link. Retry with ALL of them.
        #
        # The vendored objects reference EACH OTHER and the system libs
        # (http.o needs OpenSSL, crypto.o defines crypto_sha256 that http/ports
        # call), and the linker resolves left-to-right in a single pass. Without
        # --start-group a symbol defined in a later object looks undefined from
        # an earlier one. Wrap the vendored objects in a group so the linker
        # iterates until stable.
        vendored = [p for p in (
            os.path.join(REPO, "lib/libdb/sqlite3.o"),
            os.path.join(REPO, "lib/libhttp/http.o"),
            os.path.join(REPO, "lib/libcrypto/crypto.o"),
            os.path.join(REPO, "lib/libyaml/yaml.o"),
            os.path.join(REPO, "lib/libregex/hermes_regex.o"),
            os.path.join(REPO, "lib/libjson/json.o"),
        ) if os.path.exists(p)]
        # Drop any vendored .o whose .c source is already on the link line
        # (json.c in extra_src) to avoid "multiple definition".
        _src_bases = {os.path.basename(e) for e in g.get("extra_src", [])}
        vendored = [p for p in vendored
                    if not (os.path.basename(p)[:-2] + ".c" in _src_bases)]
        # -lm and the system libs must come AFTER --end-group: sqlite3.o's FTS5
        # code calls log(), and with -lm before the group the linker has already
        # passed libm when it pulls sqlite's object in.
        retry = ([_linker()] + objs + ["-o", out] + _GC_LINK +
                 ["-Wl,--start-group"] + vendored
                 + ["-Wl,--end-group", "-lm"] + ssl_libs + libs)
        r2 = _run_link(retry)
        if r2.returncode == 0:
            return out

        # The linker names every symbol it could not resolve. Many ports call
        # a sibling helper with NO extern declaration, so extern-only
        # discovery cannot see it (and an all-identifiers sweep measured
        # net-negative). Resolve the NAMED symbols instead: targeted, bounded,
        # and it fixes exactly the gates that would otherwise fail.
        #
        # Iterate: a newly added file has its own unresolved deps, so one pass
        # is not enough. Each round adds only the files the linker just named,
        # which keeps the link line proportional to the real dependency set
        # rather than to a blind transitive sweep.
        base_lib_objs = [os.path.join(REPO, "lib/libdb/sqlite3.o"),
                         os.path.join(REPO, "lib/libhttp/http.o"),
                         os.path.join(REPO, "lib/libyaml/yaml.o"),
                         os.path.join(REPO, "lib/libcrypto/crypto.o"),
                         os.path.join(REPO, "lib/libjson/json.o"),
                         os.path.join(REPO, "lib/libregex/hermes_regex.o")]
        base_lib_objs = [p for p in base_lib_objs if os.path.exists(p)]
        # If the gate's own extra_src already links a vendored library's SOURCE
        # (e.g. lib/libjson/json.c), drop that library's .o from base_lib_objs --
        # both define the same symbols (json_append, ...) -> "multiple
        # definition". The source on the link line wins.
        _src_bases = {os.path.basename(e) for e in g.get("extra_src", [])}
        base_lib_objs = [
            p for p in base_lib_objs
            if not (p.endswith(".o")
                    and os.path.basename(p)[:-2] + ".c" in _src_bases)]
        # Same left-to-right single-pass linker limitation as the retry above:
        # wrap the vendored objects in a group so cross-references between them
        # (and to the system libs) resolve regardless of order on the line.
        base_lib_objs = (["-Wl,--start-group"] + base_lib_objs
                         + ["-Wl,--end-group"])
        # OpenSSL symbols (BIO_*, EVP_*, CRYPTO_*, SHA256_*) come from the
        # system libcrypto, which the main build already links via -lcrypto.
        # inflate*/crc32 come from zlib; registry/log helpers often need
        # -ldl/-lpthread. build/config.mk links all of these for the binary.
        # ssl_libs was computed above (before the first retry). Reuse it here.
        added = []
        rejected = set()
        added_objs = []  # accumulates every object added across retry rounds
        # `extra_src` may already contain a stub file that duplicates a symbol
        # another unit provides (infrastructure_stubs.c re-defines
        # hermes_get_home, which paths.c also defines — and paths.c is in
        # COMMON_SRC). Probe with nothing extra; a duplicate means the stub is
        # the offender and its object must come off the link line.
        probe = _run_link(
            [_linker()] + objs + ["-o", out] + _GC_LINK + base_lib_objs + ["-lm"] + ssl_libs + libs)
        if probe.returncode != 0:
            # The linker names the OFFENDER object on the line that carries
            # "multiple definition of `sym'", and the keeper after "first
            # defined here". Take the .o immediately preceding each
            # "multiple definition" — never the one after "first defined here",
            # and never via the symbol index (which returns whichever file it
            # saw first, not necessarily the offender).
            offenders = set()
            for m in re.finditer(
                    r'([^\s/:]+\.o):\s*in function[^\n]*\n[^\n]*?'
                    r'multiple definition of',
                    probe.stderr or ""):
                offenders.add(os.path.basename(m.group(1)))
            if offenders:
                before = len(objs)
                objs = [o for o in objs
                        if os.path.basename(o) not in offenders]
                if len(objs) != before:
                    probe = _run_link(
                        [_linker()] + objs + ["-o", out] + _GC_LINK + base_lib_objs + ["-lm"] + ssl_libs + libs)
        r2 = probe
        # FULL-OBJECT TIER FIRST: one link with every prebuilt main object
        # (~1-3s, --gc-sections keeps the binary minimal) resolves
        # everything resolvable. The incremental named-symbol rounds below
        # COMPILE their additions and can eat the whole per-gate timeout
        # before ever reaching the tier - the kanban gates (battery_*,
        # browser_supervisor_*, ... deep closures) timed out mid-rounds and
        # never linked. Tier first; rounds become the fallback.
        _syslibs0 = _plat_syslibs(["-lm", "-lssl", "-lcrypto", "-ldl", "-lpthread",
                     "-lz", "-lpcre2-8", "-lasound"])
        import glob as _glob0
        _main_objs0 = sorted(
            _glob0.glob(os.path.join(REPO, "src", "**", "*.o"),
                        recursive=True) +
            _glob0.glob(os.path.join(REPO, "lib", "**", "*.o"),
                        recursive=True))
        _main_objs0 = [o for o in _main_objs0
                       if os.path.basename(o) != "main.o"]
        if _main_objs0:
            _t0 = ([_linker(), "-Wl,--allow-multiple-definition",
                    "-Wl,--start-group"] + _main_objs0
                   + objs + ["-Wl,--end-group", "-o", out]
                   + _GC_LINK + base_lib_objs + _syslibs0 + libs)
            _r0 = _run_link(_t0)
            if _r0.returncode == 0:
                return out
            r2 = _r0  # the tier's error is the best starting state for rounds
        for _round in range(12):
            missing = sorted(set(re.findall(
                r"undefined reference to [`\"']([A-Za-z_][A-Za-z0-9_]*)",
                r2.stderr or "")))
            # A stub file can re-define a symbol another unit already provides
            # (infrastructure_stubs.c vs paths.c/credential_pool_custom.c).
            # Linking it is worse than not linking it, so drop the offender and
            # let the next round try a different owner.
            dupes = set(re.findall(
                r"multiple definition of [`\"']([A-Za-z_][A-Za-z0-9_]*)",
                r2.stderr or ""))
            if dupes:
                for d in dupes:
                    owners = _resolve_missing_symbols([d], g.get("name"))
                    for o in owners:
                        rejected.add(o)
                added = [a for a in added if a not in rejected]
                # added_objs holds the COMPILED objects, named
                # "<tag>.xr.<source>.o". A source in `rejected` whose object is
                # still on the link line keeps the duplicate alive. Extract the
                # <source> part (everything after ".xr.", minus ".o") and match
                # it against the rejected set's basenames.
                rej_bases = {os.path.basename(r) for r in rejected}
                added_objs = [
                    o for o in added_objs
                    if ".xr." not in os.path.basename(o)
                    or os.path.basename(o).split(".xr.", 1)[1][:-2]
                    not in rej_bases
                ]
                # base_lib_objs is linked unconditionally; a vendored .o that
                # duplicates a symbol the harness's own extra_src already
                # provides (json.o vs the gate's json.c) must come off the base
                # set too, or the duplicate persists every round.
                base_lib_objs = [o for o in base_lib_objs
                                 if not (o.endswith(".o") and o in rejected)]
            if not missing:
                break
            extra = [e for e in _resolve_missing_symbols(missing, g.get("name"))
                     if e not in added and e not in rejected]
            if not extra:
                break
            added += extra
            # Compile each newly added .c with the gate's include flags, then
            # link the objects. Passing raw .c paths to cc on a link line
            # compiles them with default flags, so their #includes fail.
            #
            # added_objs ACCUMULATES across rounds: it must hold every object
            # ever added, not just this round's. Resetting it here made the loop
            # oscillate — a symbol resolved in round N became undefined again in
            # round N+1 because its object was dropped from the link line.
            new_objs = []
            ok = True
            for a in extra:
                # A vendored-library symbol resolves to a precompiled .o, not a
                # .c source. Add it directly — _compile_one only handles .c.
                if a.endswith(".o") and os.path.exists(a):
                    new_objs.append(a)
                    continue
                # infrastructure_stubs.c re-defines symbols real modules own.
                # If it is NOT already linked (initial extra_src), compile it
                # shadowed against the current link set. If it IS already
                # linked, the dupe-handler's re-shadow below updates the
                # existing <tag>.stub.o in place -- adding a second copy here
                # would link the stub twice and duplicate every shadowed symbol.
                if a.endswith("infrastructure_stubs.c"):
                    already = any(os.path.basename(o) == "%s.stub.o" % g["name"]
                                  for o in objs + added_objs)
                    if not already:
                        keep = [os.path.join(REPO, e) for e in
                                g.get("extra_src", []) if e.endswith(".c")]
                        keep += [x for x in added if x.endswith(".c")]
                        so = _compile_stub_shadowed(g, keep + [a])
                        if so:
                            new_objs.append(so)
                            continue
                    else:
                        continue
                o = _compile_one(g, a)
                if o:
                    new_objs.append(o)
                else:
                    ok = False
            if not ok:
                break
            added_objs += new_objs
            # Always include the full main-object set: the rounds resolve
            # what the tier genuinely lacks (compiling a missing definer),
            # but dropping the main objects from this link re-loses every
            # symbol the tier had already resolved - the link oscillates
            # until the gate times out. Tier-set + rounds' additions
            # converges in one pass.
            retry2 = ([_linker(), "-Wl,--allow-multiple-definition",
                       "-Wl,--start-group"] + _main_objs0 + objs
                      + added_objs + ["-Wl,--end-group", "-o", out]
                      + _GC_LINK + base_lib_objs + _syslibs0 + libs)
            r2 = _run_link(retry2)
            r = r2  # remember the last attempt for the final error message
            if r2.returncode == 0:
                return out

        # LAST-RESORT tier: the named-symbol rounds converge for shallow
        # closures, but a unit that transitively pulls a whole subsystem
        # (goals -> banner -> CLI config + cron) stalls on depth. The MAIN
        # BUILD's objects sit in-tree (src/**/*.o) and --gc-sections keeps
        # the harness minimal. Canonical objects come FIRST in the group so
        # the harness's own .xr recompiled copies are the ones the dupe
        # pass drops (ld names the LATER definition as the offender).
        if r2.returncode != 0:
            import glob as _glob
            main_objs = sorted(
                _glob.glob(os.path.join(REPO, "src", "**", "*.o"),
                           recursive=True) +
                _glob.glob(os.path.join(REPO, "lib", "**", "*.o"),
                           recursive=True))
            main_objs = [o for o in main_objs
                         if os.path.basename(o) != "main.o"]
            # NOTE: infrastructure_stubs.o IS included on purpose — it still
            # provides the not-yet-ported helpers (skin_get, ansi_parse_hex,
            # credfiles_get_hermes_home, whisperc_*) that display_core.o and
            # friends reference; the main binary links the same way. The
            # dupe pass below drops any object that conflicts with a real
            # definition from another main object.
            # System libs: mirror the main link (build/config.mk) — zlib for
            # the wubuzip/pngdec paths, pcre2 for hermes_regex's pcre2_*
            # entry points, plus the ssl/dl/pthread/m set the vendored libs
            # need.
            syslibs = _plat_syslibs(["-lm", "-lssl", "-lcrypto", "-ldl", "-lpthread",
                       "-lz", "-lpcre2-8", "-lasound"])
            if main_objs:
                tier_objs = list(objs) + added_objs
                for _pass in range(6):
                    retry3 = ([_linker(), "-Wl,--allow-multiple-definition",
                               "-Wl,--start-group"] + main_objs
                              + tier_objs + ["-Wl,--end-group", "-o", out]
                              + _GC_LINK + base_lib_objs
                              + syslibs)
                    r3 = _run_link(retry3)
                    if r3.returncode == 0:
                        return out
                    dupes = re.findall(
                        r"multiple definition of [`\"']"
                        r"([A-Za-z_][A-Za-z0-9_]*)",
                        r3.stderr or "")
                    if not dupes:
                        break
                    offenders = set()
                    for m in re.finditer(
                            r"([^\s/:]+\.o):\s*in function[^\n]*\n[^\n]*?"
                            r"multiple definition of",
                            r3.stderr or ""):
                        offenders.add(os.path.basename(m.group(1)))
                    if not offenders:
                        break
                    tier_objs = [o for o in tier_objs
                                 if os.path.basename(o) not in offenders]
                    r2 = r3
                    r = r3  # the tier's last attempt IS the best error state
        raise RuntimeError("BUILD FAIL (link) for %s:\n%s"
                           % (g["name"], r.stderr))
    return out


_MISSING_SYMBOL_CACHE = {}

# Vendored-library objects: these define symbols (sqlite3_*, http_*, crypto_*)
# that appear as undefined references but live in a compiled .o, not a .c
# source. find_definition() scans .c files and returns None for them, so the
# per-symbol retry would drop them. Map them here so the retry can add the .o.
# Vendored-library objects define symbols (sqlite3_*, http_*, crypto_*,
# regex_*, ...) that appear as undefined references but live in a compiled .o,
# not a .c source. find_definition() scans .c files and returns None for them,
# so the per-symbol retry would drop them. Discover ALL vendored objects under
# lib/ dynamically — a hardcoded list misses libs like libregex whose symbols
# block many gates. Index every symbol each .o defines.
_VENDORED_OBJ_DIRS = ["lib"]
def _nm_tool():
    """nm by full path when PATH does not carry the toolchain.

    A bare "nm" raised FileNotFoundError at import time -- the runner died before running a single
    gate, with the traceback naming subprocess rather than the missing tool.
    """
    found = shutil.which("nm")
    if found:
        return found
    for cand in ("C:/winlibs/mingw64/bin/nm.exe", "C:/msys64/mingw64/bin/nm.exe"):
        if os.path.exists(cand):
            return cand
    return "nm"


_VENDORED_SYMS = {}


def _build_vendored_syms():
    """Symbols exported by the vendored objects.

    `nm` over every vendored .o costs ~20s, and --each-isolated starts a fresh process per gate, so
    it ran 5,931 times. Cache it beside the archive, keyed on the member mtimes.
    """
    import subprocess as _sp
    import glob as _glob_v
    cache = os.path.join(BUILD, "_vendored_syms.json")
    members = sorted(_glob_v.glob(os.path.join(REPO, "lib", "**", "*.o"), recursive=True))
    stamp = {m: os.path.getmtime(m) for m in members}
    try:
        with open(cache, encoding="utf-8") as f:
            blob = json.load(f)
        if blob.get("stamp") == {k: v for k, v in stamp.items()}:
            _VENDORED_SYMS.update(blob["syms"])
            return members
    except (OSError, ValueError, KeyError):
        pass
    seen_dirs = []
    for base in _VENDORED_OBJ_DIRS:
        for root, _, files in os.walk(os.path.join(REPO, base)):
            for fn in files:
                if not fn.endswith(".o"):
                    continue
                p = os.path.join(root, fn)
                r = _sp.run([_nm_tool(), p], capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
                for line in (r.stdout or "").split("\n"):
                    parts = line.split()
                    if len(parts) >= 2 and parts[-2] in ("T", "W", "D", "B", "R"):
                        _VENDORED_SYMS.setdefault(parts[-1], p)
                seen_dirs.append(p)
    try:
        with open(cache, "w", encoding="utf-8") as f:
            json.dump({"stamp": stamp, "syms": _VENDORED_SYMS}, f)
    except OSError:
        pass
    return seen_dirs


_build_vendored_syms()


_ARCHIVE_CACHE = []


def _watercup_object_archive():
    """Every prebuilt object (minus main.o) in one archive, rebuilt when any member is newer."""
    import glob as _glob_a
    if _ARCHIVE_CACHE:
        return _ARCHIVE_CACHE[0]
    path = os.path.join(BUILD, "_all_objects.a")
    objs = sorted(_glob_a.glob(os.path.join(REPO, "src", "**", "*.o"), recursive=True)
                  + _glob_a.glob(os.path.join(REPO, "lib", "**", "*.o"), recursive=True))
    objs = [o for o in objs if os.path.basename(o) != "main.o"]
    newest = max((os.path.getmtime(o) for o in objs), default=0)
    if not (os.path.exists(path) and os.path.getmtime(path) >= newest):
        rsp = os.path.join(BUILD, "_ar_members.rsp")
        with open(rsp, "w") as f:
            f.write("\n".join(o.replace("\\", "/") for o in objs))
        ar = shutil.which("ar") or "C:/winlibs/mingw64/bin/ar.exe"
        subprocess.run([ar, "rcs", path, "@" + rsp], capture_output=True,
                       stdin=subprocess.DEVNULL)
    _ARCHIVE_CACHE.append(path)
    return path


def _resolve_missing_symbols(missing, gate_name):
    """Map linker-reported undefined symbols to the .c files that define them.

    Uses the same one-pass symbol index fix_all_gates builds. Bounded: only the
    symbols the linker actually named, and only files under the size cap, so
    this cannot reintroduce the link-line explosion the all-identifiers sweep
    caused.
    """
    import fix_all_gates as _fag
    out = []
    seen = set()
    # No size cap here: the linker has already PROVEN each symbol is needed, so
    # this is not a blind transitive sweep that could explode the link line.
    # The cap that guards resolve_deps does not apply — it exists to stop a
    # speculative walk, not a targeted one. (auth_store.c is 80 KB and would
    # otherwise be dropped, leaving authstore_load permanently undefined.)
    for sym in missing:
        if sym in _MISSING_SYMBOL_CACHE:
            owner = _MISSING_SYMBOL_CACHE[sym]
        elif sym in _VENDORED_SYMS:
            owner = _VENDORED_SYMS[sym]
            _MISSING_SYMBOL_CACHE[sym] = owner
        else:
            owner = _fag.find_definition_cached(sym)
            _MISSING_SYMBOL_CACHE[sym] = owner
        if not owner:
            continue
        rel = os.path.relpath(owner, REPO)
        if rel in seen:
            continue
        seen.add(rel)
        out.append(os.path.join(REPO, rel))
    return out


def run_harness(out, g):
    args = [out, g["c_fn"]] + [str(x) for x in g["inputs"]]
    r = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60, cwd=REPO)
    if r.returncode != 0:
        raise RuntimeError(
            "HARNESS CRASH for %s (exit %d): %s"
            % (g["name"], r.returncode, (r.stderr or "").strip()[:400]))
    # A cup is the FINALIZE format: exactly 16 hex chars. Traced functions
    # may legitimately print to stdout (echo, debug, prompts) — those lines
    # are noise and must not count as cups. Tighten the parse so a port that
    # prints does not trip the arity check.
    cups = [line.strip() for line in r.stdout.split("\n")
            if line.strip()
            and len(line.strip()) == 16
            and all(c in "0123456789abcdef" for c in line.strip())]
    if len(cups) != len(g["inputs"]):
        raise RuntimeError(
            "HARNESS ARITY for %s: %d cups for %d inputs"
            % (g["name"], len(cups), len(g["inputs"])))
    return cups


# ──────────────────────────────────────────────────────────────────────
# Twin loading + LIVE module import
# ──────────────────────────────────────────────────────────────────────

def load_live_module(py_module):
    """Import a LIVE Python source file by path (not by installed name)."""
    path = os.path.join(REPO, py_module)
    modname = "wc_live_" + py_module.replace("/", "_").replace(".", "_")
    spec = importlib.util.spec_from_file_location(modname, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod   # register before exec (dataclasses need it)
    # Prepend the quarry (cross-OS rules) so the dev version takes precedence
    from watercup_paths import quarry_root
    parent_quarry = str(quarry_root())
    old_path = sys.path[:]
    sys.path.insert(0, parent_quarry)
    try:
        spec.loader.exec_module(mod)
    except Exception as e:
        raise RuntimeError("Failed to import LIVE %s: %s" % (py_module, e))
    finally:
        sys.path[:] = old_path
    return mod


def _harness_cup_label(gate_dir, g):
    """The string the harness passes to WCUP_BEGIN -- the cup's seed."""
    path = os.path.join(gate_dir, g.get("c_src", "harness.c"))
    try:
        with open(path, errors="ignore") as f:
            txt = f.read()
    except OSError:
        return None
    m = re.search(r'WCUP_BEGIN\("([^"]+)"\)', txt)
    return m.group(1) if m else None


def load_twin(g):
    twin_path = os.path.join(GATES, g["name"], "twin.py")
    if not os.path.exists(twin_path):
        raise RuntimeError("missing twin.py for gate %s" % g["name"])
    spec = importlib.util.spec_from_file_location("twin_" + g["name"],
                                                  twin_path)
    twin = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(twin)
    return twin


@contextlib.contextmanager
def _neutral_argv():
    """Hide the runner's own argv from LIVE code that parses it.

    A gate whose py_fn is an entry point (``main``) calls the real function,
    and CLI entry points run ``argparse.parse_args()``. Under the runner that
    argv is the watercup command line, so the first such gate aborted the
    ENTIRE run with
    "error: the following arguments are required: --criteria"
    (cron/scripts/classify_items.py). 20 gates declare py_fn "main".

    Two hazards, both handled here:
      * argv hijack — swap in a bare argv so parse_args() sees no arguments
        rather than the runner's flags;
      * SystemExit — a CLI whose required args are now missing still calls
        sys.exit(2) from inside argparse's error path. Let that kill the
        process and the whole fleet dies on one gate; convert it to an
        ordinary exception so the twin's try/except traces it as a cup.

    SystemExit is caught, not suppressed: it still surfaces as the traced
    "ERROR:SystemExit" cup, so the divergence stays visible in the report.
    """
    saved_argv = sys.argv
    sys.argv = [saved_argv[0]] if saved_argv else [""]

    # ArgumentParser.error() -> self.exit(2, msg), an instance method, so
    # neither sys.exit nor an argparse module attribute is the lever. Patch the
    # class method every parser instance resolves through.
    import argparse as _argparse
    saved_ap_exit = _argparse.ArgumentParser.exit

    def _exit_to_exception(self, status=0, message=None):
        raise SystemExit(status)

    _argparse.ArgumentParser.exit = _exit_to_exception
    try:
        yield
    finally:
        _argparse.ArgumentParser.exit = saved_ap_exit
        sys.argv = saved_argv


def _run_twin_inputs(twin, inputs):
    """Invoke a twin's input walk, absorbing escapes that would kill the fleet.

    ``twin.run`` executes LIVE code. A gate over a CLI entry point reaches
    argparse's exit path, which raises SystemExit — a BaseException, so the
    per-gate ``except Exception`` in every twin.py sails straight past it and
    terminates the whole run on one gate.

    Convert it to a ValueError here, centrally, so all 20 ``py_fn: main`` gates
    are covered without editing each twin. The twin's existing handler then
    traces it as an ordinary ``ERROR:...`` cup and the divergence stays
    visible in the report.
    """
    try:
        return twin.run(inputs)
    except SystemExit as e:
        raise ValueError("SystemExit(%s)" % (e.code,))


# ---------------------------------------------------------------------------
# Entry-point isolation
# ---------------------------------------------------------------------------

# py_fn values whose LIVE target is a process-level program rather than a pure
# function. gateway/run.py binds ports and runs a polling loop;
# agent/transports/hermes_tools_mcp_server.py and tools/mcp_death_supervisor.py
# spawn and supervise child processes; tools/neutts_synth.py spawns audio
# subprocesses; hermes_cli/container_boot.py starts services. Imported
# in-process by the runner these crash or hang the HOST (observed: repeated WSL
# deaths), so they are delegated to _twin_driver.py in a subprocess.
ENTRY_POINT_FNS = frozenset({"main"})

# Gates whose LIVE twin must NEVER be imported at all — not in-process, not
# in a sandboxed subprocess. Both proven offenders (2026-10-20):
#   _cmd_update_impl  — runs the REAL hermes self-update flow: network fetch,
#       *.hermes-update-staging two-phase replace, and web_git._ensure_repo,
#       which git-commits "Initial commit" snapshots into whatever repo the
#       paths resolve to (three rogue commits landed on main this way). Even
#       sandboxed it runs >400s and can only ever produce an ERROR cup.
#   _tool_override_allowed — builds ABSOLUTE paths from the twin's own
#       SLERMES derivation (cwd sandboxing cannot contain it) and mkdirs
#       <twin_...DummySelf...> directories in the repo root.
# Neither can produce a meaningful cup; both only damage. Skip with an
# honest error verdict instead of executing.
UNTRACEABLE_FNS = frozenset({
    # Interactive flows: they wait on a human (device-code sign-in, QR approval) or run a whole
    # backup. Imported in-process they never return and the shard dies silently.
    "_codex_device_code_login", "dingtalk_qr_auth", "create_pre_update_backup",
    "_cmd_update_impl", "_tool_override_allowed",
})

# Whole MODULES that are never traced: hermes_cli/update_cmd.py is the
# self-update orchestration — ANY function in it can run the two-phase
# staging replace, fetch over the network, or shell to git (2026-10-20:
# a second, different update_cmd fn recreated *.hermes-update-staging in
# the repo root hours after _cmd_update_impl was blocked — per-function
# blocking is whack-a-mole; the whole module only ever damages, never
# oracles, because its behavior is machine/network state, not fixtures).
UNTRACEABLE_MODULES = frozenset({
    "hermes_cli/update_cmd.py",
})

# LIVE functions that mutate the filesystem or shell out to git using their
# arguments as PATHS. Traced in-process with the repo as CWD, they create real
# directories named after the fixture inputs — this is what produced
# <repo>/NULL/NULL, <repo>/hello/hello and <repo>/test123/test123, each a
# nested .worktrees/ containing real git worktrees. Run these from a throwaway
# CWD so the damage lands in a temp dir, not the repo.
SIDE_EFFECTING_FNS = frozenset({
    "_init_shadow_repo", "_run_git", "_init_store", "_touch_project",
    "restore", "clear_legacy", "_dirty_split", "worktree_list",
    "_ensure_file_checkpoint", "_maybe_checkpoint_wal",
    # _animate() runs a frames loop against the terminal: tracing it hangs the shard (a live
    # spinner never returns). Never import it in-process.
    "_animate",
})


def _is_entry_point(g):
    return g.get("py_fn") in ENTRY_POINT_FNS


def _is_side_effecting(g):
    return g.get("py_fn") in SIDE_EFFECTING_FNS


def _run_twin_isolated(g, inputs, timeout=None):
    """Run an entry-point gate's twin in a subprocess. Returns (cups, error).

    The child inherits the sandbox env, so operator state still lands in
    build/home_watercup/. Any failure — crash, hang, SystemExit — comes back as
    ``error`` and is reported as a failing gate, never as a dead runner.
    """
    if timeout is None:
        try:
            timeout = float(os.environ.get("WATERCUP_GATE_TIMEOUT", "120"))
        except ValueError:
            timeout = 120.0

    twin_path = os.path.join(GATES, g["name"], "twin.py")
    if not os.path.exists(twin_path):
        return None, "missing twin.py for gate %s" % g["name"]

    driver = os.path.join(HERE, "_twin_driver.py")
    # Inputs go through a file: argv has a length limit and the walks carry
    # JSON blobs with embedded quotes and newlines.
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False,
                                     dir=BUILD) as f:
        json.dump(inputs, f)
        inputs_file = f.name

    try:
        r = subprocess.run([sys.executable, driver, twin_path, inputs_file],
                           capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout,
                           env=os.environ)
        out = (r.stdout or "").strip()
        if not out:
            return None, ("isolated twin produced no output (rc=%s): %s"
                          % (r.returncode, (r.stderr or "").strip()[:300]))
        try:
            payload = json.loads(out.splitlines()[-1])
        except ValueError:
            return None, "isolated twin emitted non-JSON: %s" % out[:200]
        if "error" in payload:
            return None, payload["error"]
        return list(payload.get("cups") or []), None
    except subprocess.TimeoutExpired:
        return None, "TIMEOUT after %.0fs (entry-point gate isolated)" % timeout
    finally:
        with suppress(OSError):
            os.unlink(inputs_file)


# ──────────────────────────────────────────────────────────────────────
# Gate kinds
# ──────────────────────────────────────────────────────────────────────

def run_cup_gate(gd, g, verbose=False):
    """Trace-cup diff: C cups vs LIVE-twin cups over the shared input walk.
    Returns (fail_count, detail_lines)."""
    if (g.get("py_module") in UNTRACEABLE_MODULES
            or g.get("py_fn") in UNTRACEABLE_FNS):
        # Never build, never import this twin: the live fn runs destructive /
        # long flows (self-update, absolute-path mkdirs). Honest error,
        # zero execution.
        return 1, ["  ERROR (untraceable): %s:%s executes destructive live "
                   "flows — excluded from tracing"
                   % (g.get("py_module"), g.get("py_fn"))]
    out = build_harness(gd, g)
    c_cups = run_harness(out, g)
    twin = load_twin(g)
    _label = _harness_cup_label(gd, g)
    if _label:
        # Generated twins bind `trace = _t.trace` on the MODULE at import, so the label has to land
        # on the instance the bound method belongs to.
        _inst = getattr(twin, "_t", None)
        if _inst is not None:
            _inst._cup_label = _label
        else:
            twin._cup_label = _label

    if _is_entry_point(g):
        # Isolated: never import this twin in-process.
        py_cups, err = _run_twin_isolated(g, g["inputs"])
        if err is not None:
            return 1, ["  ERROR (isolated): %s" % err]
    elif _is_side_effecting(g):
        # Side-effecting fns are ALSO never imported in-process: an in-process
        # os.chdir(tempdir) does not contain a twin that builds ABSOLUTE paths
        # (the plugins twin created <twin_...DummySelf...> dirs in the repo
        # root despite the chdir). The isolated subprocess pins BOTH the
        # throwaway CWD and the sandboxed HERMES_HOME, so git/staging damage
        # lands in build/sandbox (gitignored, disposable) instead of the repo.
        py_cups, err = _run_twin_isolated(g, g["inputs"])
        if err is not None:
            return 1, ["  ERROR (isolated): %s" % err]
    else:
        # _neutral_argv keeps a twin's argparse from consuming the runner's
        # own argv; the CWD sandbox keeps a side-effecting fn from creating
        # directories named after its fixture inputs inside the repo.
        with _neutral_argv():
            if _is_side_effecting(g):
                import tempfile
                with tempfile.TemporaryDirectory(prefix="wc_cwd_") as _td:
                    _prev = os.getcwd()
                    try:
                        os.chdir(_td)
                        py_cups = _run_twin_inputs(twin, g["inputs"])
                    finally:
                        os.chdir(_prev)
            else:
                py_cups = _run_twin_inputs(twin, g["inputs"])
        # Normalize: if twin.run returns a single cup string (not a list),
        # replicate it across the input walk.
        if isinstance(py_cups, str):
            py_cups = [py_cups] * len(g["inputs"])
        elif not isinstance(py_cups, list):
            py_cups = list(py_cups)

    if len(c_cups) != len(py_cups):
        return 1, ["MISMATCH count: C=%d Py=%d  <<< harness/twin arity differs"
                   % (len(c_cups), len(py_cups))]

    # A gate whose cups do not move with the input proves nothing: both sides answer one constant
    # (junk fixtures like "hello"/"test123" never reach the branch under test). Reporting five
    # mismatches for it reads as five port defects; say what it is instead.
    if len(g["inputs"]) >= 2 and len(set(c_cups)) == 1 and len(set(py_cups)) == 1:
        return 1, ["  NON-DISCRIMINATING: every input produced one cup (C=%s Py=%s) -- the "
                   "fixtures do not vary the answer, so this gate cannot prove parity"
                   % (c_cups[0], py_cups[0])]

    fails = []
    lines = []
    for i, (inp, cc, pc) in enumerate(zip(g["inputs"], c_cups, py_cups)):
        if cc != pc:
            fails.append(i)
            lines.append("    [%d] %s  C=%s  PY=%s  <<< CUP MISMATCH (FAP)"
                         % (i, str(inp)[:24], cc[:12], pc[:12]))
    if fails:
        lines.append("  -> %d cup mismatches (FAP)" % len(fails))
    else:
        lines.append("  -> all %d cups MATCH LIVE Python" % len(c_cups))

    # Golden-cup pinning: a gate may pin its passing cups so any future
    # behavioral change (either side) is caught even without a twin change.
    golden_path = os.path.join(gd, "golden.json")
    if g.get("pin_golden") and not fails:
        golden = {"inputs": g["inputs"], "cups": c_cups}
        with open(golden_path, "w") as f:
            json.dump(golden, f, indent=1)
        lines.append("  -> golden cups pinned (%d)" % len(c_cups))
    elif os.path.exists(golden_path):
        try:
            golden = json.load(open(golden_path, encoding="utf-8"))
            if golden.get("cups") != c_cups:
                drift = [i for i, (a, b) in
                         enumerate(zip(golden["cups"], c_cups)) if a != b]
                lines.append("    GOLDEN DRIFT at %s  <<< pinned cups changed"
                             % drift[:8])
                fails.extend(drift)
        except (ValueError, KeyError):
            lines.append("    (golden.json unreadable — ignored)")

    if verbose:
        lines.append("    [verbose] C cups: %s" % c_cups)
        lines.append("    [verbose] PY cups: %s" % py_cups)
    return len(set(fails)), lines


def run_unit_gate(gd, g, verbose=False):
    """Unit-style gate: harness.c and twin.py are self-contained assertion
    programs (print RESULT, exit 0 on success). Covers contracts that are
    boolean/invariant rather than a branch-ladder trace (allowlist parity,
    control-char escaping, ...)."""
    lines = []
    out = build_harness(gd, g)
    rh = subprocess.run([out], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if rh.returncode != 0:
        lines.append("  HARNESS FAIL (exit %d):\n%s"
                     % (rh.returncode, rh.stdout.strip()))
        return 1, lines
    twin_path = os.path.join(gd, "twin.py")
    if os.path.exists(twin_path):
        # env=os.environ so the operator-state sandbox (HERMES_HOME) reaches the
        # child; a unit twin importing a live module must not touch the real home.
        rt = subprocess.run([sys.executable, twin_path], capture_output=True,
                            text=True, encoding="utf-8", errors="replace", cwd=gd, env=os.environ)
        if rt.returncode != 0:
            lines.append("  TWIN FAIL (exit %d):\n%s"
                         % (rt.returncode, rt.stdout.strip()))
            return 1, lines
    else:
        lines.append("  (no twin.py — harness-only unit gate)")
    if verbose:
        lines.append("  [verbose] harness output: %s" % rh.stdout.strip()[:200])
    lines.append("  -> harness + LIVE twin both PASS")
    return 0, lines


# ──────────────────────────────────────────────────────────────────────
# Coverage: what part of the port does the cup fleet actually watch?
# ──────────────────────────────────────────────────────────────────────

def coverage_report():
    """Map each gate to its LIVE python module and report subsystem coverage.
    The cup fleet is a deep gate on high-traffic pure surfaces, not a whole
    port proof — the diff oracle (tests/oracle/) covers breadth."""
    by_subsystem = {}
    for name in discover_gates():
        try:
            g = json.load(open(os.path.join(GATES, name, "gate.json"), encoding="utf-8"))
        except (ValueError, OSError):
            continue
        sub = g.get("subsystem", "?")
        by_subsystem.setdefault(sub, []).append(name)
    lines = ["  water-cup coverage by subsystem:"]
    for sub in sorted(by_subsystem):
        lines.append("    %-10s %2d gates: %s"
                     % (sub, len(by_subsystem[sub]),
                        ", ".join(by_subsystem[sub])))
    return lines


# ──────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────

def _run_each_isolated(selected, args):
    """One subprocess per gate, with a wall clock.

    In-process runs are faster but a single interactive gate ends the shard: a device-code login or
    a QR approval waits for a human forever, and a spinner never returns. Isolation also survives a
    harness access violation.
    """
    import subprocess as _sp
    green = 0
    verdicts = {"GREEN": 0, "MISMATCH": 0, "NON-DISCRIMINATING": 0, "BUILD": 0, "CRASH": 0, "OTHER": 0}
    for name in selected:
        cmd = [sys.executable, os.path.abspath(__file__), name]
        try:
            r = _sp.run(cmd, capture_output=True, text=True, encoding="utf-8",
                        errors="replace", timeout=args.timeout, cwd=REPO)
            out = (r.stdout or "") + (r.stderr or "")
        except _sp.TimeoutExpired:
            print("-- %s --\n  TIMEOUT after %ss (gate blocked)" % (name, args.timeout))
            verdicts["CRASH"] += 1
            continue
        print("-- %s --" % name)
        for line in out.splitlines():
            if ("  ->" in line or "ERROR:" in line or "MISMATCH" in line
                    or "NON-DISCRIMINATING" in line or "TIMEOUT" in line):
                print(line)
        if "cups MATCH LIVE Python" in out:
            green += 1; verdicts["GREEN"] += 1
        elif "NON-DISCRIMINATING" in out:
            verdicts["NON-DISCRIMINATING"] += 1
        elif "BUILD FAIL" in out:
            verdicts["BUILD"] += 1
        elif "MISMATCH" in out:
            verdicts["MISMATCH"] += 1
        elif "CRASH" in out or "exit" in out:
            verdicts["CRASH"] += 1
        else:
            verdicts["OTHER"] += 1
    print("\n%d gate(s): %d green, %d mismatch, %d non-discriminating, %d build fail, %d crash/timeout"
          % (len(selected), verdicts["GREEN"], verdicts["MISMATCH"],
             verdicts["NON-DISCRIMINATING"], verdicts["BUILD"], verdicts["CRASH"]))
    return 0


def main():
    ap = argparse.ArgumentParser(
        description="Project-wide water-cup trace-cup parity runner")
    ap.add_argument("gates", nargs="*", default=None,
                    help="gate names (default: all)")
    ap.add_argument("--list", action="store_true", help="list gate names")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="per-gate detail (event streams, outputs)")
    ap.add_argument("--json", action="store_true",
                    help="machine-readable report on stdout")
    ap.add_argument("--coverage", action="store_true",
                    help="append a gated-surface coverage summary")
    ap.add_argument("--force", action="store_true",
                    help="rebuild every harness (ignore freshness)")
    ap.add_argument("--timeout", type=float, default=90.0,
                    help="per-gate wall clock for --each-isolated")
    ap.add_argument("--each-isolated", action="store_true",
                    help="run every gate as its own subprocess (a gate that blocks or segfaults "
                         "costs one gate, not the shard)")
    ap.add_argument("--gate-file", default="",
                    help="read gate names from a file (one per line) instead of naming them")
    ap.add_argument("--shard", default="",
                    help="K/N: run only the K-th of N interleaved slices (drives a "
                         "parallel fleet run; each shard is an independent process)")
    args = ap.parse_args()

    all_gates = discover_gates()
    if args.list:
        print("\n".join(all_gates))
        return 0
    try:
        _check_interpreter_deps()
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        return 2
    selected = args.gates if args.gates else all_gates
    if args.gate_file:
        names = [ln.strip() for ln in open(args.gate_file, encoding="utf-8")
                 if ln.strip() and not ln.startswith("#")]
        selected = [g for g in names if g in all_gates]
    if args.shard:
        try:
            k, n = (int(x) for x in args.shard.split("/"))
        except ValueError:
            print("--shard wants K/N", file=sys.stderr)
            return 2
        if not (1 <= k <= n):
            print("--shard wants 1 <= K <= N", file=sys.stderr)
            return 2
        # Interleaved, not chunked: a crash-heavy prefix must not land in one shard.
        selected = [g for i, g in enumerate(selected) if i % n == k - 1]
    unknown = [g for g in selected if g not in all_gates]
    if unknown:
        print("Unknown gates: %s" % ", ".join(unknown), file=sys.stderr)
        return 2

    if args.each_isolated:
        return _run_each_isolated(selected, args)

    if not args.json:
        print("=== Water-Cup project-wide parity run ===")
        print("    gates: %d\n" % len(selected))

    overall_fail = 0
    report = []
    for name in selected:
        gd = os.path.join(GATES, name)
        entry = {"gate": name, "ok": False, "mismatches": 0, "lines": []}
        g = None
        try:
            g = json.load(open(os.path.join(gd, "gate.json"), encoding="utf-8"))
            entry["subsystem"] = g.get("subsystem", "?")
            if not args.json:
                print("-- %s (subsystem: %s) --" % (name,
                                                    entry["subsystem"]))
            kind = g.get("kind", "cup")
            if kind == "unit":
                fail, lines = run_unit_gate(gd, g, verbose=args.verbose)
            else:
                fail, lines = run_cup_gate(gd, g, verbose=args.verbose)
            entry["ok"] = (fail == 0)
            entry["mismatches"] = fail
            entry["lines"] = lines
        except Exception as e:
            fail = 1
            entry["error"] = str(e)
            # Label the CAUSE without exempting the gate. A harness that
            # cannot link is not a cup divergence: the port's function
            # reaches into a subsystem (auth/crypto/http) the harness does
            # not provide. Distinguishing it keeps the failure count honest
            # while making the real work visible — close the gap by linking
            # the dependency, never by skipping the gate (no N/A here).
            if str(e).startswith("BUILD FAIL"):
                entry["build_fail"] = True
            lines = ["  ERROR: %s" % e]
        except SystemExit as e:
            # Backstop for a SystemExit raised anywhere in the per-gate block
            # (a twin's module-level code, or a path _run_twin_inputs does not
            # cover). `except Exception` cannot catch it — SystemExit derives
            # from BaseException — so without this one gate exits the runner
            # and the remaining gates are never evaluated.
            #
            # SystemExit only, deliberately: KeyboardInterrupt is also a
            # BaseException and must still propagate so Ctrl+C aborts the run.
            fail = 1
            entry["error"] = "SystemExit(%s)" % (e.code,)
            lines = ["  ERROR: gate raised SystemExit(%s)" % (e.code,)]

        # Entry-point gates (py_fn "main") are long-running programs, not
        # pure functions. They are isolated so they cannot take the runner
        # down, and they are still scored: this project has no N/A bucket
        # (the NA machinery was deliberately deleted — rewriting from
        # scratch in C is the point), so an entry point that cannot be
        # traced is REAL_GAP work, not an exemption.
        overall_fail += fail
        if not args.json:
            for line in lines:
                print(line)
            print()
        report.append(entry)

    if args.coverage:
        cov = coverage_report()
        if args.json:
            for r in report:
                if r["gate"] == selected[0]:
                    r["coverage"] = cov
        else:
            print("\n".join(cov))

    if args.json:
        print(json.dumps({
            "total": len(selected),
            "failing": overall_fail,
            "ok": overall_fail == 0,
            "gates": report,
        }, indent=1))
    else:
        print("=" * 60)
        print("TOTAL GATES: %d   FAILING: %d" % (len(selected), overall_fail))
        build_fails = sum(1 for r in report if r.get("build_fail"))
        if build_fails:
            print("  of which BUILD FAIL (harness cannot link — port needs an "
                  "unlinked subsystem): %d" % build_fails)
        print("VERDICT: %s" % (
            "water-cup caught divergence(s)" if overall_fail
            else "all cups match LIVE Python across translations"))

    # results.jsonl — one line per run (consumed by CI dashboards)
    try:
        os.makedirs(os.path.dirname(RESULT_FILE), exist_ok=True)
        with open(RESULT_FILE, "a", encoding="utf-8") as f:
            for r in report:
                f.write(json.dumps({"ok": r["ok"],
                                    "gate": r["gate"],
                                    "mismatches": r["mismatches"]}) + "\n")
    except OSError:
        pass

    return 1 if overall_fail else 0


if __name__ == "__main__":
    sys.exit(main())
