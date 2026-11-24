#!/usr/bin/env python3
"""Differential oracle for hermes_cli/main_platform_setup.py (w18_mainplatfo_97fb7b)."""
import builtins
import contextlib
import io
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
from hermes_cli import main_platform_setup as live

_real_input = builtins.input

def _feed(value):
    if value is None:
        def raise_eof(prompt=""):
            raise EOFError()
        return raise_eof
    def fake(prompt=""):
        return value
    return fake

def compute(r):
    fn = r.get("func")
    if fn == "yes_no":
        builtins.input = _feed(r.get("response"))
        try:
            return live._yes_no("")
        finally:
            builtins.input = _real_input
    if fn == "whatsapp_mode":
        builtins.input = _feed(r.get("choice"))
        saved = []
        try:
            return live._whatsapp_choose_mode(
                lambda key: r.get("current_mode"),
                lambda key, val: saved.append((key, val)))
        finally:
            builtins.input = _real_input
    if fn == "paired":
        return live._whatsapp_has_paired_identity_live(r.get("creds"))
    return None

# The live helper reads creds.json from a dir; inject the parsed object by
# replacing json.loads + Path.read_text for exactly this call.
def _paired(creds):
    if not isinstance(creds, dict) or not isinstance(creds.get("me"), dict):
        return False
    return isinstance(creds["me"].get("id"), str) and bool(creds["me"]["id"])

def _whatsapp_has_paired_identity_live(creds):
    # Faithful re-execution of the live predicate with the file read seamed:
    # patch Path.read_text to hand back json.dumps(creds).
    import pathlib
    orig = pathlib.Path.read_text
    text = json.dumps(creds)
    pathlib.Path.read_text = lambda self, *a, **k: text
    try:
        return live._whatsapp_has_paired_identity(pathlib.Path("seam"))
    finally:
        pathlib.Path.read_text = orig

live._whatsapp_has_paired_identity_live = _whatsapp_has_paired_identity_live

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
    with contextlib.redirect_stdout(io.StringIO()):
        py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_mainplatfo_97fb7b: {total} cases, {mism} mismatches")
sys.exit(1 if (mism or total == 0) else 0)
