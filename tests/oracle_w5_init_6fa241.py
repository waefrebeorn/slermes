#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/__init__.py (w5_init_6fa241 gate).

The package shim has only a docstring and `from . import jiter_preload as
_jiter_preload`. A parent package + jiter_preload submodule are bootstrapped so
the relative import resolves; the jiter import side effect is irrelevant here.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap for the relative `from . import jiter_preload`.
PKG = "w5_init_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/agent"]
sys.modules[PKG] = pkg
sys.modules[PKG + ".jiter_preload"] = types.ModuleType(PKG + ".jiter_preload")

spec = importlib.util.spec_from_file_location(PKG + ".__init__", Q + "/agent/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".__init__"] = live
spec.loader.exec_module(live)

PUBLIC = [n for n in dir(live) if not n.startswith("__")]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "doc":
        py = live.__doc__
    elif func == "names":
        py = PUBLIC
    elif func == "has_symbol":
        py = r.get("name", "") in PUBLIC
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_init_6fa241: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
