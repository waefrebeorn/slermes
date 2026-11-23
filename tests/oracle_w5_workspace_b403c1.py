#!/usr/bin/env python3
"""Differential oracle for pm/workspace.py pure helpers (w5_workspace_b403c1).

Import-time dependencies (pm.paths, pm.package, pm.plugin_declarations,
pm.environment) are stubbed with parent-package bootstrap; `.resolve()` is a
fixture seam so the hash is host-independent.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import PureWindowsPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _InstallError(Exception):
    pass


_stub("pm", __path__=[Q + "/pm"])
_stub("pm.paths", repo_root=lambda: Q)
_stub("pm.package", InstallError=_InstallError)
_stub("pm.plugin_declarations", read_python_declaration=lambda p: None,
      manifest_version_error=lambda m, n: None)
_stub("pm.environment", ResolutionConflict=_InstallError, classify_uv_failure=lambda *a: None)
for _sub in ("paths", "package", "plugin_declarations", "environment"):
    setattr(sys.modules["pm"], _sub, sys.modules["pm." + _sub])

LIVE_NAME = "w5_workspace_b403c1_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/pm/workspace.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


class _ResolvedPath:
    def __init__(self, text):
        self._text = text
        self.name = PureWindowsPath(text).name

    def resolve(self):
        return self

    def __str__(self):
        return self._text


def compute(r):
    f = r.get("func")
    if f == "member_ignored":
        return live._member_ignored(None, r.get("names") or [])
    if f == "without_release_age":
        return live._without_release_age(r.get("text", ""))
    if f == "conjuncts":
        return sorted(live._conjuncts(r.get("marker", "")))
    if f == "member_key":
        return live._member_key(_ResolvedPath(r.get("path", "")))
    if f == "solve_only_for":
        policy = {"environments": r.get("environments")}
        live._solve_only_for(policy, r.get("target") or {})
        return {"environments": policy["environments"]}
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
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH workspace %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_workspace_b403c1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
