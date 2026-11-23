#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_local_route.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, "C:/Users/eman5/hermes-agent")

SRC = "C:/Users/eman5/hermes-agent/gateway/session_local_route.py"
spec = importlib.util.spec_from_file_location("session_local_route_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_local_route_live"] = live
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
    if r.get("func") != "resolve_launch_route":
        continue
    cases += 1
    py = live.resolve_launch_route(r.get("params", {}), r.get("config", {}))
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH resolve_launch_route py={py!r} c={got!r} "
              f"params={r.get('params')!r} config={r.get('config')!r}", file=sys.stderr)
print(f"ORACLE localroute: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
