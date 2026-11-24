#!/usr/bin/env python3
"""Differential oracle for hermes_cli/main_provider_setup.py (w18_mainprovid_56bced)."""
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m
_r = _mod("ruamel"); _r.__path__ = []
_ry = _mod("ruamel.yaml"); _ry.__path__ = []
class _YAML:
    def __init__(self, *a, **k): pass
class _YAMLError(Exception): pass
class _VersionedResolver: pass
_ry.YAML = _YAML
_ry.YAMLError = _YAMLError
_mod("ruamel.yaml.error", YAMLError=_YAMLError)
_mod("ruamel.yaml.resolver", VersionedResolver=_VersionedResolver)

import hermes_cli
from hermes_cli import main_provider_setup as live

def compute(r):
    fn = r.get("func")
    if fn == "short_url":
        return live._short_url(r.get("url", ""))
    if fn == "effort":
        return live._aux_effort_word(r.get("cfg") or {})
    if fn == "format_aux":
        return live._format_aux_current(r.get("cfg"))
    if fn == "delegation_cfg":
        return live._delegation_cfg_as_task(r.get("cfg") or {})
    if fn == "auto_name":
        return live._auto_provider_name(r.get("url", ""))
    if fn == "api_key_value":
        return live._custom_provider_api_key_config_value(r.get("info") or {}, r.get("resolved", ""))
    if fn == "base_url_value":
        return live._custom_provider_base_url_config_value(r.get("info") or {}, r.get("resolved", ""))
    if fn == "stepfun":
        return live._infer_stepfun_region(r.get("url", ""))
    return None

total = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    total += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_mainprovid_56bced: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
