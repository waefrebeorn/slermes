#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_provider_env.py (gate w18_configprov_6f4b50)."""
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
_pkg = "hermes_cli"
if _pkg not in sys.modules:
    _p = types.ModuleType(_pkg); _p.__path__ = [os.path.join(QUARRY, "hermes_cli")]
    sys.modules[_pkg] = _p
_SRC = os.path.join(QUARRY, "hermes_cli", "config_provider_env.py")
_spec = importlib.util.spec_from_file_location("hermes_cli.config_provider_env", _SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["hermes_cli.config_provider_env"] = live
_spec.loader.exec_module(live)


def compute(r):
    if r.get("func") != "entries_for":
        return "__UNKNOWN__"
    profiles = [types.SimpleNamespace(**p) for p in r.get("profiles", [])]
    return live._entries_for(profiles, set(r.get("skip", [])))


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
        print(f"MISMATCH py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_configprov_6f4b50: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
