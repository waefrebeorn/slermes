#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/served_model.py (gate w6_servedmodel_e72b96)."""
import importlib.util
import json
import sys
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location(
    "live_served_model", QUARRY + "/agent/served_model.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_served_model"] = live
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
    func = r.get("func")
    if func == "headers":
        py = live.served_model_from_headers(r.get("headers"))
    elif func == "result":
        agent = SimpleNamespace(
            model=r.get("model"),
            last_served_model=r.get("last_served"),
            _fallback_activated=r.get("fallback_activated", False),
            _primary_runtime=r.get("primary_runtime"),
        )
        py = live.result_model_fields(agent)
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH case=%s py=%r c=%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_servedmodel_e72b96: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
