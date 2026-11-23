#!/usr/bin/env python3
"""Differential oracle for agent/browser_page_backend.py (gate w13_browserpag_472fc0).

The live module is an abstract interface (no computational body); the C port
mirrors the class contract that Python introspection reports. The live module is
imported via spec_from_file_location with a stubbed agent.provider_base.
"""
import abc
import importlib.util
import inspect
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# parent-package bootstrap + minimal ProviderBase stub for the live ABC
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
pb = types.ModuleType("agent.provider_base")
pb.ProviderBase = type("ProviderBase", (abc.ABC,), {})
sys.modules["agent"] = agent_pkg
sys.modules["agent.provider_base"] = pb
agent_pkg.provider_base = pb

SRC = "C:/Users/eman5/hermes-agent/agent/browser_page_backend.py"
spec = importlib.util.spec_from_file_location("bpp_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["bpp_live"] = live
spec.loader.exec_module(live)

ABC = live.BrowserPageBackend


def shape(name):
    fn = getattr(ABC, name, None)
    if fn is None:
        return None
    sig = inspect.signature(fn)
    params, kwonly = [], []
    for p in sig.parameters.values():
        if p.name == "self":
            continue
        if p.kind == inspect.Parameter.KEYWORD_ONLY:
            kwonly.append(p.name)
        elif p.kind in (inspect.Parameter.POSITIONAL_ONLY,
                        inspect.Parameter.POSITIONAL_OR_KEYWORD):
            params.append(p.name)
    return {"params": params, "kwonly": kwonly,
            "abstract": name in ABC.__abstractmethods__}


def compute(r):
    f = r.get("func")
    if f == "abstract_methods":
        return sorted(ABC.__abstractmethods__)
    if f == "is_abstract":
        return inspect.isabstract(ABC)
    if f == "method_shape":
        return shape(r.get("method", ""))
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
print(f"ORACLE w13_browserpag_472fc0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
