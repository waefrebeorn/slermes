#!/usr/bin/env python3
"""Differential oracle for hermes_cli/post_update.py (gate w8_postupdate_284e24).

Logic under test: run_steps(steps).  Each injected step is either
{"name": ..., "out": <json>} (returns <json>) or
{"name": ..., "raise": <str>} (raises Exception(<str>)).  The LIVE run_steps
collects results in order, replacing a raised step with
{"ok": False, "error": str(exc)}.  Compared with the C results object.
"""
import importlib.util
import json
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


_stub("hermes_cli._launchers", expose_cli=lambda: {"ok": True})
_stub("hermes_cli.default_plugins", step_install_default_plugins=lambda: {"ok": True})
_stub("pm.paths", install_root=lambda: "C:/opt/hermes")

LIVE_NAME = "w8_postupdate_284e24_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/post_update.py")
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
    if r.get("func") != "run_steps_ex":
        continue

    steps = []
    for s in r.get("steps", []):
        if "raise" in s:
            def _raise(msg=s["raise"]):
                raise Exception(msg)
            steps.append((s["name"], _raise))
        else:
            def _ret(value=s["out"]):
                return value
            steps.append((s["name"], _ret))
    py = live.run_steps(steps)

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH run_steps_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_postupdate_284e24: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
