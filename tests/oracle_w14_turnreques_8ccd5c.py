#!/usr/bin/env python3
"""Differential oracle for gate w14_turnreques_8ccd5c (agent/turn_request_assembly.py)."""
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_turnreques_8ccd5c"
sys.path.insert(0, QUARRY)

NS = types.SimpleNamespace


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


_seed("agent", __path__=[QUARRY + "/agent"])
_seed("agent.message_sanitization", _sanitize_messages_surrogates=lambda messages: None)
_seed("agent.usage_anchor", anchored_context_tokens=lambda *a, **k: None)
_seed("agent.prompt_caching",
      build_prompt_cache_plan=lambda *a, **k: NS(messages=[], tools=[]),
      effective_cache_ttl=lambda *a, **k: 0)
_seed("agent.turn_context", build_api_messages=lambda *a, **k: ([], ""),
      _agent_stale_thinking_on_wire=lambda a: False)
_seed("agent.turn_empty_response", add_empty_response_retry_hint=lambda *a, **k: "")

SRC = QUARRY + "/agent/turn_request_assembly.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    completions = NS()
    if r.get("has_rebase"):
        completions.rebase_prepared_request = (
            lambda pending, messages: copy.deepcopy(r.get("rebase_out")))
    if r.get("has_prepare"):
        completions.prepare = lambda messages: copy.deepcopy(r.get("prepare_out"))
    agent = NS(client=NS(chat=NS(completions=completions)))
    prepared, messages, pending = live._prepare_moa_request(
        agent, r.get("api_messages"), r.get("pending"))
    return [prepared, messages, pending]


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
