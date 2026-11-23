#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_plugin_oauth.py (gate w15_auxiliaryp_293888)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")
_pkg("hermes_cli", QUARRY + "/hermes_cli")

_STATE = {"r": {}}


def _r():
    return _STATE["r"]


auth = types.ModuleType("hermes_cli.auth")
auth.PROVIDER_REGISTRY = {}
sys.modules["hermes_cli.auth"] = auth

rp = types.ModuleType("hermes_cli.runtime_provider")
rp._config_base_url_for_provider = lambda cfg, p: _r().get("config_base_url", "")
rp._get_model_config = lambda: {}
sys.modules["hermes_cli.runtime_provider"] = rp

aux = types.ModuleType("agent.auxiliary_client")
aux._normalize_api_key = lambda x: _r().get("normalized_api_key", "")


def _select_pool_entry(provider, model):
    sel = bool(_r().get("pool_selected", False))
    entry = None
    if sel:
        entry = SimpleNamespace(
            runtime_api_key=_r().get("pool_runtime_key", ""),
            runtime_base_url=_r().get("pool_runtime_base_url", ""))
    return sel, entry


aux._select_pool_entry = _select_pool_entry


def _supplied_client(provider, api_key="", base_url=""):
    if _r().get("profile_client_available", False) and api_key:
        return {"provider": provider, "api_key": api_key, "base_url": base_url}
    return None


aux._api_key_profile_supplied_client = _supplied_client
aux._log_once_debug = lambda *a, **kw: None
aux._LOGGED_UNSUPPORTED_OAUTH_KEYS = set()
aux._normalize_resolved_model = lambda model, provider: _r().get("normalized_model", "")
aux._get_aux_model_for_provider = lambda provider: _r().get("model_candidate", "")
aux._route_client = lambda req, client, model: (client, model)
sys.modules["agent.auxiliary_client"] = aux

spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_plugin_oauth_live", QUARRY + "/agent/auxiliary_plugin_oauth.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_plugin_oauth_live"] = live
spec.loader.exec_module(live)


def compute(r):
    _STATE["r"] = r
    f = r.get("func")
    if f == "configured_endpoint":
        provider = r.get("provider", "")
        if r.get("provider_in_registry", True):
            auth.PROVIDER_REGISTRY = {provider: SimpleNamespace(
                inference_base_url=r.get("profile_inference_base_url", ""))}
        else:
            auth.PROVIDER_REGISTRY = {}
        return live._configured_endpoint(provider)
    if f == "resolve":
        live._configured_endpoint = lambda p: r.get("configured_endpoint", "")
        reqd = r.get("req") or {}
        req = SimpleNamespace(
            provider=reqd.get("provider", ""),
            explicit_api_key=reqd.get("explicit_api_key", ""),
            explicit_base_url=reqd.get("explicit_base_url", ""),
            model=reqd.get("model"))
        client, model = live.resolve_plugin_oauth_client(req)
        return [client, model]
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
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_auxiliaryp_293888: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
