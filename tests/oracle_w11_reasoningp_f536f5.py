#!/usr/bin/env python3
"""Differential oracle for agent/reasoning_promotion.py (gate w11_reasoningp_f536f5).

The live route's four external seams are stubbed and injected per case:
base_url_host_matches, get_custom_provider_model_capability, _entries_for_route
and is_local_endpoint.  custom_provider_slug is stubbed with its real upstream
implementation (verbatim) so the live module's identity logic is exercised.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

HOST_MATCH = {"v": False}
CONFIGURED = {"v": None}
ENTRIES = {"v": []}
IS_LOCAL = {"v": False}


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


utils = _pkg("utils")
utils.base_url_host_matches = lambda url, domain: HOST_MATCH["v"]

hc = _pkg("hermes_cli")
config_mod = types.ModuleType("hermes_cli.config")
config_mod._entries_for_route = lambda base_url, custom_providers, scope: ENTRIES["v"]
config_mod.get_custom_provider_model_capability = lambda **kw: CONFIGURED["v"]
hc.config = config_mod
sys.modules["hermes_cli.config"] = config_mod

providers_mod = types.ModuleType("hermes_cli.providers")


def _custom_provider_slug(display_name="", provider_key=""):
    identity = str(provider_key or "").strip() or str(display_name or "").strip()
    normalized = identity.lower().replace(" ", "-")
    return normalized if normalized.startswith("custom:") else f"custom:{normalized}"


providers_mod.custom_provider_slug = _custom_provider_slug
hc.providers = providers_mod
sys.modules["hermes_cli.providers"] = providers_mod

agent_pkg = _pkg("agent")
mm = types.ModuleType("agent.model_metadata")
mm.is_local_endpoint = lambda url: IS_LOCAL["v"]
agent_pkg.model_metadata = mm
sys.modules["agent.model_metadata"] = mm

SRC = QUARRY + "/agent/reasoning_promotion.py"
spec = importlib.util.spec_from_file_location("w11_reasoningp_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_reasoningp_live"] = live
spec.loader.exec_module(live)


def compute(r):
    HOST_MATCH["v"] = bool(r.get("openrouter_host_match", False))
    CONFIGURED["v"] = r.get("configured", None)
    ENTRIES["v"] = r.get("entries", []) or []
    IS_LOCAL["v"] = bool(r.get("is_local", False))
    agent = SimpleNamespace(
        base_url=r.get("base_url", ""), provider=r.get("provider", ""),
        api_mode=r.get("api_mode", ""), model=r.get("model", ""),
        requested_provider=r.get("requested_provider", ""),
        _custom_providers=r.get("custom_providers", None))
    return live.answer_in_reasoning_capability(agent)


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
    print("ORACLE w11_reasoningp_f536f5: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_reasoningp_f536f5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
