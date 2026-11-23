#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_isolation.py (w15_pluginisol_0cd954)."""
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
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

SRC = Q + "/hermes_cli/plugin_isolation.py"
spec = importlib.util.spec_from_file_location("pluginisol_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginisol_live"] = live
spec.loader.exec_module(live)

ENV = "HERMES_PLUGIN_HOST_PROCESS"


def config_for(r):
    plugins = {}
    if "cfg_isolation" in r:
        plugins["isolation"] = r["cfg_isolation"]
    if "launcher" in r:
        plugins["host"] = {"launcher": r["launcher"]}
    return {"plugins": plugins}


def compute(r):
    f = r.get("func")
    config = config_for(r)
    if f == "isolation_mode":
        return live.isolation_mode(config)
    if f == "in_process_import_refusal":
        return live.in_process_import_refusal(
            r.get("what", ""), source=r.get("source", "user"), config=config)
    if f == "host_launcher":
        return live.host_launcher(config)
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
    saved = os.environ.get(ENV)
    if r.get("host_env") is None:
        os.environ.pop(ENV, None)
    else:
        os.environ[ENV] = str(r["host_env"])
    try:
        expected = compute(r)
    finally:
        if saved is None:
            os.environ.pop(ENV, None)
        else:
            os.environ[ENV] = saved
    cases += 1
    got = r.get("out")
    if expected != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w15_pluginisol_0cd954: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
