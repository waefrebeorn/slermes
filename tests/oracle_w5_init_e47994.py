#!/usr/bin/env python3
"""Differential oracle for tools/__init__.py (gate w5_init_e47994).

The namespace module has a single delegating callable and a side-effect-free
import contract. A fake parent package + terminal_tool stub supplies the
backend result via sys.modules; the module itself is loaded from the quarry.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

PKG = "w5init_e47994_tools"
pkg = types.ModuleType(PKG)
pkg.__path__ = []
sys.modules[PKG] = pkg

holder = {"value": False}
tt = types.ModuleType(PKG + ".terminal_tool")
tt.check_terminal_requirements = lambda: holder["value"]
sys.modules[PKG + ".terminal_tool"] = tt
# spec_from_file_location with submodule_search_locations makes the live module a
# package whose __package__ is itself, so `.terminal_tool` resolves one level
# deeper than it looks. Register both names and blank __path__ below.
sys.modules[PKG + ".__init__.terminal_tool"] = tt

SRC = Q + "/tools/__init__.py"
spec = importlib.util.spec_from_file_location(
    PKG + ".__init__", SRC, submodule_search_locations=[])
live = importlib.util.module_from_spec(spec)
sys.modules[PKG + ".__init__"] = live
spec.loader.exec_module(live)
live.__path__ = []

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
    try:
        if func == "check":
            holder["value"] = bool(r.get("delegated", False))
            py = live.check_file_requirements()
        elif func == "all":
            py = list(live.__all__)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_init_e47994: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
