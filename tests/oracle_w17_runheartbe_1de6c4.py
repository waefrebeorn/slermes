#!/usr/bin/env python3
"""Differential oracle for gateway/run_heartbeat_restore.py (w17_w17_runheartbe_1de6c4_1de6c4)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

gateway = types.ModuleType("gateway")
gateway.__path__ = []
sys.modules["gateway"] = gateway

_state = {"homes": []}
grun = types.ModuleType("gateway.run")
grun._multiplex_profile_homes = lambda cfg: _state["homes"]
sys.modules["gateway.run"] = grun
gateway.run = grun

SRC = Q + "/gateway/run_heartbeat_restore.py"
spec = importlib.util.spec_from_file_location("w17_runheartbe_1de6c4_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_runheartbe_1de6c4_live"] = live
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
    func = r.get("func")
    if func != "watched":
        continue
    try:
        _state["homes"] = [(str(n), str(h)) for n, h in r.get("multiplex_homes", [])]
        runner = types.SimpleNamespace(
            config=types.SimpleNamespace(multiplex_profiles=r.get("multiplex_profiles", False)))
        py = [str(p) for p in live._watched_homes(runner, r.get("default_home", ""))]
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_runheartbe_1de6c4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
