#!/usr/bin/env python3
"""Differential oracle for agent/memory_manager_tools.py pure helpers
(gate w13_memorymana_3efc6f). The live module has only stdlib imports; the
lazy `toolsets.resolve_toolset` call is stubbed from the injected `resolved`
map so the live function under test is what computes every expected value.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

TS = {"map": {}}
ts = types.ModuleType("toolsets")


def resolve_toolset(name):
    if name not in TS["map"]:
        raise KeyError(name)          # models the live resolve failure path
    return TS["map"][name]


ts.resolve_toolset = resolve_toolset
sys.modules["toolsets"] = ts

SRC = "C:/Users/eman5/hermes-agent/agent/memory_manager_tools.py"
spec = importlib.util.spec_from_file_location("mmt_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["mmt_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "normalize":
        return live.normalize_tool_schema(r.get("schema"))
    if f == "tool_name":
        return live._tool_name(r.get("tool"))
    if f in ("enabled", "exposed"):
        TS["map"] = dict(r.get("resolved") or {})
        present = bool(r.get("present", False))
        if f == "exposed":
            tools = r.get("tools") or []
            present = any(live._tool_name(t) == "memory" for t in tools)
        return live.memory_provider_tools_enabled(
            r.get("enabled"), r.get("disabled"), memory_tool_present=present)
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
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_memorymana_3efc6f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
