#!/usr/bin/env python3
"""Differential oracle for agent/reasoning_carriers.py (gate w10_reasoningc_80bddd).

Imports the LIVE module with stubbed providers / route-hook seams; declared
carrier keys and replay answers are injected per case from the fixture.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")

providers = types.ModuleType("providers")
PROFILE_KEYS = {"opaque": [], "text": []}
def _list_providers():
    n = max(len(PROFILE_KEYS["opaque"]), len(PROFILE_KEYS["text"]))
    out = []
    for i in range(n):
        o = PROFILE_KEYS["opaque"][i] if i < len(PROFILE_KEYS["opaque"]) else ""
        t = PROFILE_KEYS["text"][i] if i < len(PROFILE_KEYS["text"]) else ""
        out.append(SimpleNamespace(message_carrier_opaque=o, message_carrier_text=t))
    return out
providers.list_providers = _list_providers
providers.profile_for_host = lambda url: None
providers.registry_generation = lambda: 0
sys.modules["providers"] = providers

hooks = types.ModuleType("agent.provider_route_hooks")
REPLAY = {"v": {}}
hooks.hook_failed = lambda profile, name, scope: False
hooks.ask_profile = lambda *a, **k: REPLAY["v"]
agent_pkg.provider_route_hooks = hooks
sys.modules["agent.provider_route_hooks"] = hooks

transports = _pkg("agent.transports")
cc = types.ModuleType("agent.transports.chat_completions")
cc._model_consumes_thought_signature = lambda model: False
transports.chat_completions = cc
sys.modules["agent.transports.chat_completions"] = cc

SRC = QUARRY + "/agent/reasoning_carriers.py"
spec = importlib.util.spec_from_file_location("w10_reasoningc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_reasoningc_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "thought_signature":
        return live.thought_signature(r.get("extra"))
    if func == "carrier_record":
        PROFILE_KEYS["opaque"] = list(r.get("opaque_keys") or [])
        PROFILE_KEYS["text"] = list(r.get("text_keys") or [])
        msg = r.get("msg")
        # live field() uses getattr(obj, name): the message must be an object,
        # while extra_content / model_extra values stay plain dicts.
        obj = SimpleNamespace(**dict(msg)) if isinstance(msg, dict) else msg
        return live.carrier_record(obj)
    if func == "strip_function_signature":
        return live._strip_function_signature(r.get("tool_calls") or [])
    if func == "replay_plan":
        plan = live._replay_plan(r.get("answer"), r.get("values") or {})
        if plan is live._REWRITES:
            return "rewrites"
        if plan is None:
            return None
        return [[a, b] for a, b in plan]
    if func == "shape":
        REPLAY["v"] = r.get("replay_result") or {}
        rt = r.get("route") or {}
        profile = object() if rt.get("profile") else None
        route = live._WireRoute(
            model="m", base_url="", gemini=bool(rt.get("gemini")),
            profile=profile, keep_fn_sig=bool(rt.get("keep_fn_sig")),
            keys=tuple(rt.get("keys") or []), scope=None)
        return live._shape(r.get("msg"), route)
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w10_reasoningc_80bddd: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_reasoningc_80bddd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
