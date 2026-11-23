#!/usr/bin/env python3
"""Differential oracle for live agent/onboarding_session.py (w10_onboarding_ec6652).

`toolsets` is stubbed with per-case fixtures so the null-enabled branch of
with_onboarding is exercised without importing the live toolset registry.
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

toolsets = types.ModuleType("toolsets")
toolsets.get_all_toolsets = lambda: []
toolsets.TOOLSET_SESSION_PLATFORMS = frozenset()
sys.modules["toolsets"] = toolsets

SRC = Q + "/agent/onboarding_session.py"
spec = importlib.util.spec_from_file_location("ob_ec6652_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["ob_ec6652_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "builds_onboarding":
        return live.builds_onboarding(types.SimpleNamespace(valid_tool_names=r.get("valid")))
    if f == "keeps_toolsets":
        return live.keeps_toolsets(r.get("enabled"))
    if f == "with_onboarding":
        toolsets.get_all_toolsets = lambda: r.get("all_toolsets", [])
        toolsets.TOOLSET_SESSION_PLATFORMS = frozenset(r.get("session_platforms", []))
        return live.with_onboarding(r.get("enabled"))
    if f == "carry_onboarding_toolset":
        agent = types.SimpleNamespace(enabled_toolsets=r.get("agent_toolsets"))
        return live.carry_onboarding_toolset(r.get("enabled"), agent)
    if f == "append_turn_note":
        agent = types.SimpleNamespace(_gateway_turn_context_notes=r.get("prior", ""))
        live.append_turn_note(agent, r.get("note", ""))
        return getattr(agent, "_gateway_turn_context_notes", "") or ""
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
    try:
        py = compute(r)
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w10_onboarding_ec6652: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
