#!/usr/bin/env python3
"""Differential oracle for hermes_cli/main_install_repair.py (w18_maininstal_b04279)."""
import json
import sys
import types
from pathlib import Path

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
from hermes_cli import main_install_repair as live

def compute(r):
    fn = r.get("func")
    if fn == "npm_path":
        return live._is_windows_npm_path(r.get("npm_path", ""))
    if fn == "stamp":
        return live._quarantine_stamp_ms(Path(r.get("name", "")))
    if fn == "branch":
        b = r.get("branch")
        args = types.SimpleNamespace(branch=b) if b is not None else object()
        return live._resolve_update_branch(args)
    if fn == "filter":
        kept, removed = live._filter_pending_shim_renames(
            r.get("entries", []), [Path(s) for s in r.get("shims", [])])
        return {"kept": kept, "removed": removed}
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
print(f"ORACLE w18_maininstal_b04279: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
