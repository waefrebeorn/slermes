#!/usr/bin/env python3
"""Differential oracle for acp_adapter/__main__.py (gate w7_main_d117b1).

The live module is loaded under a bootstrapped parent package with its
relative `.entry` stubbed so the module-level `main()` call is observable and
does not actually start the adapter.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w7_main_d117b1_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/acp_adapter"]
sys.modules[PKG] = pkg

CALLS = []


def _entry_main():
    CALLS.append(1)


entry = types.ModuleType(PKG + ".entry")
entry.main = _entry_main
sys.modules[PKG + ".entry"] = entry

spec = importlib.util.spec_from_file_location(PKG + ".__main__", Q + "/acp_adapter/__main__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".__main__"] = live
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
    if func == "main_calls":
        return len(CALLS)
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
print(f"ORACLE w7_main_d117b1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
