#!/usr/bin/env python3
"""Differential oracle for hermes_agent/__init__.py (gate w8_init_7b876f).

The upstream module is a single docstring: it binds no non-dunder names. A
parent package is bootstrapped so the module loads under a package, exactly
like the real import.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w8_init_7b876f_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/hermes_agent"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".__init__", Q + "/hermes_agent/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".__init__"] = live
spec.loader.exec_module(live)

PUBLIC = sorted(n for n in dir(live) if not n.startswith("__"))


def compute(r):
    func = r.get("func")
    if func == "doc":
        return live.__doc__
    if func == "names":
        return PUBLIC
    if func == "has_symbol":
        return r.get("name", "") in PUBLIC
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w8_init_7b876f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
