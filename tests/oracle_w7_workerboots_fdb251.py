#!/usr/bin/env python3
"""Differential oracle for cron/worker_bootstrap.py (gate w7_workerboots_fdb251)."""
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


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


calls = []
_stub("pm")
_stub("pm.environments", activate_dependencies=lambda d: calls.append(str(d)))
_stub("hermes_cli")

LIVE_NAME = "w7_workerboots_fdb251_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/worker_bootstrap.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "worker_bootstrap_ex":
        continue
    root = r.get("project_root", "")
    _stub("hermes_cli._startup_fast", project_root_str=lambda: root)
    marker = r.get("marker")
    if marker is None:
        os.environ.pop(live.WORKER_MARKER, None)
    else:
        os.environ[live.WORKER_MARKER] = marker
    del calls[:]
    live.worker_bootstrap()
    py = ["activate", calls[0]] if calls else ["noop"]

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH worker_bootstrap_ex args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

os.environ.pop(live.WORKER_MARKER, None)
print("ORACLE w7_workerboots_fdb251: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
