#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_defaults_env.py (gate w18_configdefa_a05e90)."""
import importlib.util
import json
import os
import sys
import types

for _s in ("stdin", "stdout", "stderr"):
    try:
        getattr(sys, _s).reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
# parent-package bootstrap + sys.modules registration
_pkg = "hermes_cli"
if _pkg not in sys.modules:
    _p = types.ModuleType(_pkg); _p.__path__ = [os.path.join(QUARRY, "hermes_cli")]
    sys.modules[_pkg] = _p
_SRC = os.path.join(QUARRY, "hermes_cli", "config_defaults_env.py")
_spec = importlib.util.spec_from_file_location("hermes_cli.config_defaults_env", _SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["hermes_cli.config_defaults_env"] = live
_spec.loader.exec_module(live)


def compute(r):
    fn = r.get("func")
    if fn == "env":
        return live._env(r.get("description", ""), r.get("prompt", ""), **(r.get("keys") or {}))
    if fn == "category":
        pw = r.get("password_default", True)
        adv = bool(r.get("advanced_default", False))
        make = live._category(r.get("category", ""), pw, adv)
        return make(r.get("description", ""), r.get("prompt", ""), **(r.get("opts") or {}))
    if fn == "base_url":
        return live._base_url(r.get("name", ""), r.get("prompt_name"))
    return "__UNKNOWN__"


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
    py = compute(r)
    if py == "__UNKNOWN__":
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_configdefa_a05e90: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
