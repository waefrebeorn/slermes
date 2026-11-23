#!/usr/bin/env python3
"""Differential oracle for agent/oneshot_footprint.py (gate w9_oneshotfoot_1a8e1a).

The live one-shot decision is driven with a seeded frozen policy
(gateway.session_policy.active_policy) and session-env seam
(gateway.session_context.get_session_env).  Both registered as stubs; C gets
the same two inputs through the _ex seam.  No process environment is mutated.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

gateway_pkg = types.ModuleType("gateway")
gateway_pkg.__path__ = [QUARRY + "/gateway"]
sys.modules.setdefault("gateway", gateway_pkg)

sp = types.ModuleType("gateway.session_policy")
sys.modules["gateway.session_policy"] = sp
sc = types.ModuleType("gateway.session_context")
sys.modules["gateway.session_context"] = sc

live = _load("agent.oneshot_footprint", QUARRY + "/agent/oneshot_footprint.py")


def compute(r):
    func = r.get("func")
    if func == "is_single_query":
        ps = r.get("policy_source")   # None -> no owner policy; else str
        ev = r.get("env_value")       # str or None
        sp.active_policy = lambda: (types.SimpleNamespace(source=ps) if ps is not None else None)
        sc.get_session_env = lambda k, d="": (ev if k == "HERMES_SINGLE_QUERY_SESSION" else d)
        return live.is_single_query_session()
    if func == "agent_is_oneshot":
        computed = bool(r.get("computed", False))
        live.is_single_query_session = lambda: computed
        a = types.SimpleNamespace()
        if r.get("cached_present"):
            a._oneshot_session = bool(r.get("cached", False))
        return live.agent_is_oneshot(a)
    if func == "disabled_toolsets":
        return list(live.ONESHOT_DISABLED_TOOLSETS)
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_oneshotfoot_1a8e1a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
