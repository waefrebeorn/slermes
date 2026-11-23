#!/usr/bin/env python3
"""Differential oracle for w8_voiceturnr_3c1c02 -> agent/voice_turn_route.py.

Late imports are scripted: _get_aux_model_for_provider and expand_direct_api_alias
are registered as modules and their return values come from the fixture, and
_lowest_effort is monkeypatched with the fixture's `lowest` seam.
"""
import importlib.util
import json
import sys
import types

G = "w8_voiceturnr_3c1c02"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
_pkg("hermes_cli")
_mod("agent.route_verdicts", ROUTE_VERDICT_FIELDS=("fallback", "verdict"))
aux_holder = [""]


def _get_aux_model_for_provider(provider, prefer_fast=True):
    return aux_holder[0]


_mod("agent.auxiliary_client", _get_aux_model_for_provider=_get_aux_model_for_provider)
alias_holder = {"provider": "", "base": None}


def _expand(provider, base_url):
    return alias_holder["provider"], alias_holder["base"]


_mod("hermes_cli.runtime_provider_custom", expand_direct_api_alias=_expand)

SRC = Q + "/agent/voice_turn_route.py"
spec = importlib.util.spec_from_file_location("w8_voiceturnr_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_voiceturnr_live"] = live
spec.loader.exec_module(live)


class _Agent:
    def __init__(self, provider, model):
        self.provider = provider
        self.model = model


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


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
        if func == "route_target":
            aux_holder[0] = r.get("aux_model") or ""
            alias_holder["provider"] = r.get("alias_provider") or ""
            alias_holder["base"] = r.get("alias_base")
            agent = _Agent(r.get("agent_provider") or "", r.get("agent_model") or "")
            py = live._route_target(agent, r.get("cfg") or {})
        elif func == "voice_reasoning":
            live._lowest_effort = lambda agent: (r.get("lowest") or None)
            py = live._voice_reasoning(None, r.get("effort"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
