#!/usr/bin/env python3
"""Differential oracle for the C11 port of agent/legacy_cli.py (w6_legacycli_b0ce00).

Loads the LIVE module by path (parent-package bootstrap via a stub `agent`
package). Calls live.main(argv, run=capture) and compares the resolved call
against the C harness output. argparse rejections become {"error":true}.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap: keep the real agent/__init__ out of the way.
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [Q + "/agent"]
sys.modules["agent"] = agent_pkg

SRC = Q + "/agent/legacy_cli.py"
spec = importlib.util.spec_from_file_location("w6_legacycli_b0ce00_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_legacycli_b0ce00_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "resolve":
        continue
    argv = r.get("argv")
    if not isinstance(argv, list):
        argv = []
    captured = {}

    def fake_run(**kw):
        captured.update(kw)

    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            live.main(argv, run=fake_run)
        if captured:
            py = {"called": True}
            py.update(captured)
        else:
            py = {"called": False}
    except SystemExit:
        py = {"error": True}
    except Exception as exc:
        print("LIVE ERROR case=%s: %r" % (r.get("case"), exc), file=sys.stderr)
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s C=%r PY=%r" % (r.get("case"), got, py), file=sys.stderr)

if cases == 0:
    print("ORACLE w6_legacycli_b0ce00: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_legacycli_b0ce00: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
