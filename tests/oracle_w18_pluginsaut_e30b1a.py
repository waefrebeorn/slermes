#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_auto_update.py (w18_pluginsaut_e30b1a)."""
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
from hermes_cli import plugins_auto_update as live

def compute(r):
    fn = r.get("func")
    if fn == "held":
        return live.held_with_ref(r.get("record"))
    if fn == "summary":
        return live.summary_lines(r.get("outcome") or {})
    if fn == "slash":
        outcome = r.get("outcome") or {}
        state = r.get("state") or {}
        live.update_all_unattended = lambda: outcome
        live.auto_update_state = lambda: state
        live.set_auto_update = lambda enabled: state
        return live.plugins_slash_action(r.get("parts", []))
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
print(f"ORACLE w18_pluginsaut_e30b1a: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
