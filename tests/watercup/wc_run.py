#!/usr/bin/env python3
"""tests/watercup/wc_run.py — PROJECT-WIDE water-cup parity runner.

This is the generic, registry-driven engine that makes the water-cup apply
"project wide" rather than as hand-built one-off demos. Each gate lives in
tests/watercup/gates/<name>/ and declares itself via gate.json:

    {
      "name": "usage_pricing_format_cost_label",
      "subsystem": "agent",
      "c_src": "harness.c",          # traced C copy of the ported fn
      "c_fn": "format_cost_label",   # argv-dispatch name inside harness.c
      "py_module": "agent/usage_pricing.py",
      "py_fn": "format_cost_label",  # LIVE Python twin (imported, traced)
      "inputs": ["0.0", "5.0", ...], # correlated input walk (shared C/Py)
      "enc": {"double": "%.17g"}     # canonical value encodings (cross-lang)
    }

The runner:
  1. builds <gate>/harness.c (compiled with watercup.h) -> build/<name>
  2. runs it over gate["inputs"] -> one cup hash per input
  3. imports LIVE <py_module> and runs <py_fn> under the water-cup tracer
     (the twin.py in the gate dir wraps it to match the C branch ladder)
  4. diffs the two cup sequences; any mismatch => FAP for that gate.

The design is language-agnostic: the cup is a byte stream of B/L/R/r/V events
folded with FNV-1a. Any source translated to C11 — Python, TypeScript, Rust —
carves the same shape when the logic is identical. A stub that returns a constant
cannot reproduce the real trace, so the cup never matches.

Run:
    python3 wc_run.py                # all gates
    python3 wc_run.py usage_pricing_format_cost_label   # one gate
    python3 wc_run.py --list         # list gate names

Exit code is fail-closed: non-zero if ANY gate has a cup mismatch.
"""
import os, sys, json, subprocess, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(HERE)                 # .../slermes
DEV = os.path.dirname(SLERMES)                 # .../hermes-agent-dev
GATES = os.path.join(HERE, "gates")
BUILD = os.path.join(SLERMES, "tests", "oracle", "build", "watercup")
sys.path.insert(0, HERE)
from watercup_py import Cup

# Canonical value encodings shared C<->Py (mirror watercup.h WCUP_VALSTR).
def enc_double(v):  return "%.17g" % float(v)
def enc_bool(v):    return "True" if v else "False"
def enc_str(v):     return "" if v is None else str(v)


def build_harness(gate_dir, g):
    src = os.path.join(gate_dir, g["c_src"])
    out = os.path.join(BUILD, g["name"])
    os.makedirs(BUILD, exist_ok=True)
    r = subprocess.run(["cc", "-I", HERE, src, "-o", out],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("BUILD FAIL for %s:\n%s" % (g["name"], r.stderr))
    return out


def run_harness(out, g):
    args = [out, g["c_fn"]] + [str(x) for x in g["inputs"]]
    r = subprocess.run(args, capture_output=True, text=True)
    cups = [l.strip() for l in r.stdout.split("\n") if l.strip()]
    return cups


def load_live_module(py_module):
    """Import a LIVE Python source file by path (not by installed name)."""
    import importlib.util as ilu
    path = os.path.join(DEV, py_module)
    modname = "wc_live_" + py_module.replace("/", "_").replace(".", "_")
    spec = ilu.spec_from_file_location(modname, path)
    mod = ilu.module_from_spec(spec)
    sys.modules[modname] = mod   # register before exec (dataclasses need it)
    try:
        spec.loader.exec_module(mod)
    except Exception as e:
        # some modules need sibling imports; surface a clear error
        raise RuntimeError("Failed to import LIVE %s: %s" % (py_module, e))
    return mod


def run_twin(g):
    """The gate's twin.py exposes run(inputs) -> list of cup hashes, importing
    the LIVE module and tracing it with the SAME branch ladder as the C harness."""
    twin_path = os.path.join(GATES, g["name"], "twin.py")
    if not os.path.exists(twin_path):
        raise RuntimeError("missing twin.py for gate %s" % g["name"])
    import importlib.util as ilu
    spec = ilu.spec_from_file_location("twin_" + g["name"], twin_path)
    twin = ilu.module_from_spec(spec)
    spec.loader.exec_module(twin)
    return twin.run(g["inputs"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gate", nargs="?", default=None)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    gate_names = sorted(d for d in os.listdir(GATES)
                        if os.path.isfile(os.path.join(GATES, d, "gate.json")))
    if args.list:
        for n in gate_names:
            print(n)
        return 0
    if args.gate:
        gate_names = [args.gate]

    print("=== Water-Cup project-wide parity run ===")
    print("    gates: %d\n" % len(gate_names))

    overall_fail = 0
    for name in gate_names:
        gd = os.path.join(GATES, name)
        g = json.load(open(os.path.join(gd, "gate.json")))
        print("-- %s (subsystem: %s) --" % (name, g.get("subsystem", "?")))
        try:
            out = build_harness(gd, g)
            c_cups = run_harness(out, g)
            py_cups = run_twin(g)
        except Exception as e:
            print("  ERROR: %s" % e)
            overall_fail += 1
            continue
        if len(c_cups) != len(py_cups):
            print("  MISMATCH count: C=%d Py=%d  <<< harness/twin arity differs"
                  % (len(c_cups), len(py_cups)))
            overall_fail += 1
            continue
        mism = 0
        for i, (inp, cc, pc) in enumerate(zip(g["inputs"], c_cups, py_cups)):
            if cc != pc:
                mism += 1
                print("    [%d] %s  C=%s  PY=%s  <<< CUP MISMATCH (FAP)"
                      % (i, str(inp)[:24], cc[:12], pc[:12]))
        if mism:
            print("  -> %d cup mismatches (FAP)\n" % mism)
            overall_fail += 1
        else:
            print("  -> all %d cups MATCH LIVE Python\n" % len(c_cups))

    print("=" * 60)
    print("TOTAL GATES: %d   FAILING: %d" % (len(gate_names), overall_fail))
    print("VERDICT: %s" % ("water-cup caught divergence(s)" if overall_fail
                           else "all cups match LIVE Python across translations"))
    return 1 if overall_fail else 0


if __name__ == "__main__":
    sys.exit(main())
