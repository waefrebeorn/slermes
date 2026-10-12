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
import shutil
import subprocess
import sys
import tempfile
from contextlib import suppress

os.environ["WATERCUP_NO_BROWSER"] = "1"

HERE = os.path.dirname(os.path.abspath(__file__))
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(HERE, "build")
# Repo root is two levels up: tests/watercup -> tests -> slermes.
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(REPO))  # parent quarry for LIVE module imports

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
    if shutil.which("ccache"):
        return ["ccache", "cc"]
    return ["cc"]


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

    cflags = g.get("cflags", [])
    cflags = [os.path.join(REPO, c) if not c.startswith("-") else c
              for c in cflags]
    # Resolve combined -Ipath flags to absolute (REPO-relative) so they
    # work regardless of the invoking CWD. E.g. "-Ilib/libhive" → "-I<REPO>/lib/libhive"
    cflags = [c[:2] + os.path.join(REPO, c[2:])
              if c.startswith("-I") and len(c) > 2 and not os.path.isabs(c[2:])
              else c for c in cflags]
    # Gate cflags carry a bare "-I" with no path, so vendored headers
    # (lib/libhttp/http.h and siblings) never resolve and the harness fails
    # to compile. Add the project's standard include roots once, here.
    std_inc = []
    for root in ("include", "lib", "src",
                 "lib/libhttp", "lib/libjson", "lib/libcredentialfiles",
                 "lib/libcrypto", "lib/libtextwrap", "lib/libglob",
                 "lib/libhive", "lib/libdb"):
        p = os.path.join(REPO, root)
        if os.path.isdir(p):
            std_inc += ["-I", p]
    cflags = std_inc + cflags
    extra = [os.path.join(REPO, e) for e in g.get("extra_src", [])]
    libs = g.get("ldflags", [])

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
    sources += [(e, "%s.x%d.%s.o" % (tag, i, os.path.basename(e)))
                for i, e in enumerate(extra)]

    compile_fail = None
    for s, obj_name in sources:
        obj = os.path.join(BUILD, obj_name)
        objs.append(obj)
        if not force and os.path.exists(obj) and not _newer_than(s, os.path.getmtime(obj)):
            continue  # object fresh
        ccmd = _compiler() + ["-I", HERE] + cflags + ["-c", s, "-o", obj]
        r = subprocess.run(ccmd, capture_output=True, text=True)
        if r.returncode != 0:
            compile_fail = "BUILD FAIL (compile %s) for %s:\n%s" % (
                os.path.basename(s), g["name"], r.stderr)
            break

    if compile_fail:
        raise RuntimeError(compile_fail)

    lcmd = ["cc"] + objs + ["-o", out, "-lm"] + libs
    r = subprocess.run(lcmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("BUILD FAIL (link) for %s:\n%s" % (g["name"], r.stderr))
    return out


def run_harness(out, g):
    args = [out, g["c_fn"]] + [str(x) for x in g["inputs"]]
    r = subprocess.run(args, capture_output=True, text=True, timeout=60, cwd=REPO)
    if r.returncode != 0:
        raise RuntimeError(
            "HARNESS CRASH for %s (exit %d): %s"
            % (g["name"], r.returncode, (r.stderr or "").strip()[:400]))
    cups = [line.strip() for line in r.stdout.split("\n") if line.strip()]
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
    # Prepend parent quarry to sys.path so dev version takes precedence
    parent_quarry = os.path.dirname(REPO)
    old_path = sys.path[:]
    sys.path.insert(0, parent_quarry)
    try:
        spec.loader.exec_module(mod)
    except Exception as e:
        raise RuntimeError("Failed to import LIVE %s: %s" % (py_module, e))
    finally:
        sys.path[:] = old_path
    return mod


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
                           capture_output=True, text=True, timeout=timeout,
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
    out = build_harness(gd, g)
    c_cups = run_harness(out, g)
    twin = load_twin(g)
    if _is_entry_point(g):
        # Isolated: never import this twin in-process.
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
            golden = json.load(open(golden_path))
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
    rh = subprocess.run([out], capture_output=True, text=True)
    if rh.returncode != 0:
        lines.append("  HARNESS FAIL (exit %d):\n%s"
                     % (rh.returncode, rh.stdout.strip()))
        return 1, lines
    twin_path = os.path.join(gd, "twin.py")
    if os.path.exists(twin_path):
        # env=os.environ so the operator-state sandbox (HERMES_HOME) reaches the
        # child; a unit twin importing a live module must not touch the real home.
        rt = subprocess.run([sys.executable, twin_path], capture_output=True,
                            text=True, cwd=gd, env=os.environ)
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
            g = json.load(open(os.path.join(GATES, name, "gate.json")))
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
    unknown = [g for g in selected if g not in all_gates]
    if unknown:
        print("Unknown gates: %s" % ", ".join(unknown), file=sys.stderr)
        return 2

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
            g = json.load(open(os.path.join(gd, "gate.json")))
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
        with open(RESULT_FILE, "a") as f:
            for r in report:
                f.write(json.dumps({"ok": r["ok"],
                                    "gate": r["gate"],
                                    "mismatches": r["mismatches"]}) + "\n")
    except OSError:
        pass

    return 1 if overall_fail else 0


if __name__ == "__main__":
    sys.exit(main())
