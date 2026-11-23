#!/usr/bin/env python3
"""Differential oracle for pm/plugin_declarations.py (w8_plugindecla_451f92 gate)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

if "hermes_cli" not in sys.modules:
    hc = types.ModuleType("hermes_cli"); hc.__path__ = [Q + "/hermes_cli"]; sys.modules["hermes_cli"] = hc
pm = types.ModuleType("hermes_cli.plugins_manifest")
pm.SUPPORTED_MANIFEST_VERSION = 2
STATE = {"reason": None}
pm.requires_hermes_error = lambda manifest: STATE["reason"]
sys.modules["hermes_cli.plugins_manifest"] = pm
setattr(sys.modules["hermes_cli"], "plugins_manifest", pm)

LIVE_NAME = "w8_plugindecla_451f92_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/pm/plugin_declarations.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def declaration_external(manifest):
    d = object.__new__(live.PythonDeclaration)
    object.__setattr__(d, "manifest", manifest)
    return d.external


def compute(r):
    f = r.get("func")
    if f == "manifest_version_error":
        STATE["reason"] = r.get("requires_error")
        return live.manifest_version_error(r.get("manifest", {}) or {}, r.get("name", ""))
    if f == "external":
        return declaration_external(r.get("manifest", {}) or {})
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH plugindecla %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_plugindecla_451f92: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
