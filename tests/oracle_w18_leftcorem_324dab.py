#!/usr/bin/env python3
"""Differential oracle for hermes_cli/left_core_migration.py (w18_leftcorem_324dab)."""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

# ruamel is an optional runtime dep absent in this image; stub only what
# hermes_yaml.py imports so the parent package can be bootstrapped.
import types
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

import hermes_cli  # parent-package bootstrap
from hermes_cli import left_core_migration as live

def compute(r):
    fn = r.get("func")
    if fn == "first_cause":
        return live._first_cause(r.get("error", ""))
    if fn == "delegation_routes":
        return live._delegation_backend_routes(r.get("delegation") or {})
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
print(f"ORACLE w18_leftcorem_324dab: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
