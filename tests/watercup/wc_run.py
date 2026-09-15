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
import hashlib
import importlib.util
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(HERE, "build")
# Repo root is two levels up: tests/watercup -> tests -> slermes.
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(REPO))  # parent quarry for LIVE module imports
from watercup_py import Cup  # noqa: E402

RESULT_FILE = os.path.join(HERE, "build", "results.jsonl")


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
    extra = [os.path.join(REPO, e) for e in g.get("extra_src", [])]
    libs = g.get("ldflags", [])
    cmd = ["cc", "-I", HERE] + cflags + [src] + extra + ["-o", out, "-lm"] + libs
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("BUILD FAIL for %s:\n%s" % (g["name"], r.stderr))
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


# ──────────────────────────────────────────────────────────────────────
# Gate kinds
# ──────────────────────────────────────────────────────────────────────

def run_cup_gate(gd, g, verbose=False):
    """Trace-cup diff: C cups vs LIVE-twin cups over the shared input walk.
    Returns (fail_count, detail_lines)."""
    out = build_harness(gd, g)
    c_cups = run_harness(out, g)
    twin = load_twin(g)
    py_cups_raw = twin.run(g["inputs"])
    # Normalize: if twin.run returns a single cup string (not a list), replicate it
    if isinstance(py_cups_raw, str):
        py_cups = [py_cups_raw] * len(g["inputs"])
    elif isinstance(py_cups_raw, list):
        py_cups = py_cups_raw
    else:
        py_cups = list(py_cups_raw)

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
        rt = subprocess.run([sys.executable, twin_path], capture_output=True,
                            text=True, cwd=gd)
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
            lines = ["  ERROR: %s" % e]
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
