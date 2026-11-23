#!/usr/bin/env python3
"""Differential oracle for the C11 port of agent/turn_usage.py (w6_turnusage_47b7c5).

Loads the LIVE module by path with a parent-package bootstrap and stubs its
import-time collaborators. Int64 token counts are carried as decimal strings
in the fixtures; both sides are normalised to Python ints before comparison.
"""
import importlib.util
import json
import sys
import types
from dataclasses import dataclass

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


apkg = _mod("agent")
apkg.__path__ = [Q + "/agent"]
_mod("agent.image_token_cost", calibrate_from_usage=lambda *a, **k: None)
_mod("agent.usage_anchor", capture_usage_anchor=lambda *a, **k: None,
     set_usage_anchor=lambda *a, **k: None)
_mod("agent.usage_pricing", estimate_usage_cost=lambda *a, **k: None,
     normalize_usage=lambda *a, **k: None,
     with_served_service_tier=lambda *a, **k: None)

SRC = Q + "/agent/turn_usage.py"
spec = importlib.util.spec_from_file_location("w6_turnusage_47b7c5_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_turnusage_47b7c5_live"] = live
spec.loader.exec_module(live)

FIELDS = ("session_prompt_tokens", "session_completion_tokens", "session_total_tokens",
          "session_input_tokens", "session_output_tokens", "session_cache_read_tokens",
          "session_cache_write_tokens", "session_reasoning_tokens")
UFIELDS = ("input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
           "reasoning_tokens")


class Agent:
    pass


class Usage:
    pass


@dataclass
class TUsage:
    cache_write_by_ttl: object
    cache_write_tokens: int


def norm(out, func):
    if func == "add":
        return {k: int(v) for k, v in out.items()}
    if func == "write_tier":
        return [[ttl, int(tokens)] for ttl, tokens in out]
    return out


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    got = norm(r.get("out"), func)
    try:
        if func == "add":
            agent = Agent()
            for f in FIELDS:
                setattr(agent, f, int(r.get(f, "0")))
            usage = Usage()
            for f in UFIELDS:
                setattr(usage, f, int(r.get(f, "0")))
            live.add_session_usage(
                agent, usage,
                prompt_tokens=int(r.get("prompt_tokens", "0")),
                completion_tokens=int(r.get("completion_tokens", "0")),
                total_tokens=int(r.get("total_tokens", "0")))
            py = {f: getattr(agent, f) for f in FIELDS}
        elif func == "write_tier":
            pairs = [tuple(p) for p in (r.get("ttl") or [])]
            u = TUsage(tuple((ttl, int(tok)) for ttl, tok in pairs),
                       int(r.get("cache_write_tokens", "0")))
            tier = r.get("tier")
            res = live.with_write_tier(u, tier)
            py = [[ttl, int(tok)] for ttl, tok in res.cache_write_by_ttl]
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s case=%s: %r" % (func, r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (func, r.get("case"), got, py),
              file=sys.stderr)

if cases == 0:
    print("ORACLE w6_turnusage_47b7c5: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_turnusage_47b7c5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
