#!/usr/bin/env python3
"""Differential oracle for agent/rate_limit_credits.py (gate w10_ratelimit_e741d9).

Imports the LIVE module with stubbed utils / hermes_cli.config seams; host
matching, the credits-notices config and the cached flag are injected per case.
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

utils = types.ModuleType("utils")
HOST_MATCH = {"v": False}
utils.is_truthy_value = lambda v: bool(v)
utils.base_url_host_matches = lambda url, host: HOST_MATCH["v"]
sys.modules["utils"] = utils

hcli = _pkg("hermes_cli")
cfg = types.ModuleType("hermes_cli.config")
CFG = {}
cfg.load_config = lambda: CFG
hcli.config = cfg
sys.modules["hermes_cli.config"] = cfg

SRC = QUARRY + "/agent/rate_limit_credits.py"
spec = importlib.util.spec_from_file_location("w10_ratelimit_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_ratelimit_live"] = live
spec.loader.exec_module(live)

MIXIN = live.RateLimitCreditsMixin


def compute(r):
    func = r.get("func")
    if func == "is_nous_agent":
        HOST_MATCH["v"] = bool(r.get("nous_host_match", False))
        agent = SimpleNamespace(provider=r.get("provider"), base_url=r.get("base_url"))
        return live._is_nous_agent(agent)
    if func == "pct":
        return live._pct(r.get("fraction"))
    if func == "credits_spent_micros":
        start = int(r["session_start"]) if r.get("session_start") is not None else None
        rem = int(r["remaining"]) if r.get("remaining") is not None else None
        state = SimpleNamespace(remaining_micros=rem) if rem is not None else None
        agent = SimpleNamespace(_credits_session_start_micros=start, _credits_state=state)
        v = MIXIN.get_credits_spent_micros(agent)
        return None if v is None else str(v)
    if func == "notices_enabled":
        cv = r.get("config_value")
        CFG.clear()
        if cv is not None:
            CFG.update({"display": {"credits_notices": cv}})
        agent = SimpleNamespace(_credits_notices_enabled_cache=r.get("cache"))
        return MIXIN._credits_notices_enabled(agent)
    if func == "openrouter_cache_status":
        agent = SimpleNamespace(_or_cache_hits=int(r.get("count_before", 0)))
        status = r.get("status")
        response = SimpleNamespace(headers={"x-openrouter-cache-status": status} if status else {})
        MIXIN._check_openrouter_cache_status(agent, response)
        hit = bool(status) and str(status).upper() == "HIT"
        return {"hit": hit, "count": str(agent._or_cache_hits)}
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
    print("ORACLE w10_ratelimit_e741d9: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w10_ratelimit_e741d9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
