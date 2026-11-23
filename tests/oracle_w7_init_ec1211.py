#!/usr/bin/env python3
"""Differential oracle for tui_gateway/__init__.py (gate w7_init_ec1211).

The live file is an empty package initializer. It is still loaded by path under
its real package name and registered in sys.modules (parent-package bootstrap),
then its observable public surface is compared against the C port.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tui_gateway/__init__.py"
spec = importlib.util.spec_from_file_location("tui_gateway", SRC)
live = importlib.util.module_from_spec(spec)
live.__path__ = [Q + "/tui_gateway"]
sys.modules["tui_gateway"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "public_names":
        return sorted(n for n in vars(live) if not n.startswith("_"))
    if f == "has_attr":
        name = r.get("name")
        return bool(isinstance(name, str) and name and not name.startswith("_") and hasattr(live, name))
    if f == "module_doc":
        return live.__doc__
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w7_init_ec1211: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
