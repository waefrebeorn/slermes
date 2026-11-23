#!/usr/bin/env python3
"""Oracle for the trivial Hermes plugins/__init__.py shim (gate w6_init_d99929).

The upstream module is a single comment (`# Hermes plugins package`): it binds
no non-dunder names and declares no docstring.  A parent package is bootstrapped
so the module loads under a package like the real import.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w6_init_d99929_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/plugins"]
sys.modules[PKG] = pkg
spec = importlib.util.spec_from_file_location(PKG + ".__init__", Q + "/plugins/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".__init__"] = live
spec.loader.exec_module(live)

PUBLIC = [n for n in dir(live) if not n.startswith("__")]


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
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_init_d99929: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
