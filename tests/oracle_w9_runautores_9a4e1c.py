#!/usr/bin/env python3
"""Differential oracle for gateway/run_auto_resume.py (_session_route).

Reads the harness JSONL, replays the same `parsed` seam through the LIVE
_session_route and compares the routing dict. Fail-closed.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/gateway/run_auto_resume.py"
spec = importlib.util.spec_from_file_location("rar_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["rar_w9_live"] = live
spec.loader.exec_module(live)

# The live _session_route does `from gateway.run import _parse_session_key`
# inside its body; gateway.run imports the whole (unavailable) dependency env.
# Stub that one symbol with the fixture-supplied parsed dict.
gw = types.ModuleType("gateway"); gw.__path__ = []
grun = types.ModuleType("gateway.run")
_HOLDER = {"parsed": None}
grun._parse_session_key = lambda key: _HOLDER["parsed"]
sys.modules["gateway"] = gw
sys.modules["gateway.run"] = grun

cases = mism = 0
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
    cases += 1
    parsed = r.get("parsed")
    _HOLDER["parsed"] = parsed  # None -> `or {}` inside the live function
    try:
        py = live._session_route(r.get("unit") or {})
    except BaseException as ex:
        py = {"__raise__": type(ex).__name__, "message": str(ex)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH session_route case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_runautores_9a4e1c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
