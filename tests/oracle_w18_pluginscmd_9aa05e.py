#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_cmd_catalog.py (w18_pluginscmd_9aa05e)."""
import json
import sys
import types
from types import SimpleNamespace

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
from hermes_cli import plugins_cmd_catalog as live

def compute(r):
    fn = r.get("func")
    if fn == "looks":
        return live.looks_like_catalog_name(r.get("identifier", ""))
    if fn == "pin":
        return live.at_catalog_pin(r.get("sidecar") or {}, r.get("entry_sha", ""))
    if fn == "platforms":
        return sorted(live.normalized_platforms(r.get("platforms", [])))
    if fn == "added":
        return sorted(live._added(r.get("surface", ""),
                                  set(r.get("old_set", [])), set(r.get("new_set", []))))
    if fn == "delta":
        old_map = {k: set(v) for k, v in (r.get("old_map") or {}).items()}
        new_map = {k: set(v) for k, v in (r.get("new_map") or {}).items()}
        return live.surface_delta(old_map, new_map)
    if fn == "delta_lines":
        return live.surface_delta_lines(r.get("delta") or {})
    if fn == "pin_label":
        return live.pin_label(SimpleNamespace(version=r.get("version", ""), sha=r.get("sha", "")))
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
print(f"ORACLE w18_pluginscmd_9aa05e: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
