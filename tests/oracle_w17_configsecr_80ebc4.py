#!/usr/bin/env python3
"""Differential oracle for gate w17_configsecr_80ebc4 (hermes_cli/config_secret_refs.py)."""
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


def _custom_endpoint_key_env(identity):
    slug = re.sub(r"[^A-Z0-9]+", "_", str(identity or "").upper()).strip("_")
    return f"HERMES_CUSTOM_{slug}_API_KEY" if slug else "HERMES_CUSTOM_API_KEY"


hcli = _pkg("hermes_cli")
_mod("hermes_cli.config", custom_endpoint_key_env=_custom_endpoint_key_env)

NAME = "w17csr_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/config_secret_refs.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_configsecr_80ebc4"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    if func == "env_slug":
        return live._env_slug(inp.get("text", ""))
    if func == "model_api_key_env":
        return live.model_api_key_env()
    if func == "aux_api_key_env":
        return live.aux_api_key_env(inp.get("task", ""))
    if func == "digest":
        return live._digest(inp.get("text", ""))
    if func == "provider_name_key_env":
        return live.provider_name_key_env(inp.get("name", ""))
    if func == "is_env_reference":
        return bool(live.is_env_reference(inp.get("value", "")))
    if func == "canonical_endpoint":
        return live._canonical_endpoint(inp.get("base_url", ""))
    if func == "endpoint_identity":
        return live.endpoint_identity(inp.get("base_url", ""))
    if func == "endpoint_api_key_env":
        return live.endpoint_api_key_env(inp.get("base_url", ""))
    if func == "dashboard_endpoint_key_env":
        return live.dashboard_endpoint_key_env(inp.get("entry") or {}, inp.get("endpoint_id", ""))
    if func == "block_base_url":
        return live._block_base_url(inp.get("block") or {})
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
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
