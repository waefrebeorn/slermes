#!/usr/bin/env python3
"""Differential oracle for agent/route_verdicts.py (gate w7_routeverdic_40e4a2)."""
import copy
import importlib.util
import json
import sys
from types import SimpleNamespace as NS

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/route_verdicts.py"
spec = importlib.util.spec_from_file_location("w7_routeverdic_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "route_visit":
        return live.route_visit(NS(_route_visit=r.get("visit")))
    if func == "snapshot":
        return live.snapshot_route_verdicts(NS(**r.get("agent", {})))
    if func == "apply":
        agent = NS(**r.get("agent", {}))
        live.apply_route_verdicts(agent, r.get("values", {}))
        return vars(agent)
    if func == "reset":
        agent = NS(**r.get("agent", {}))
        live.reset_route_verdicts(agent)
        return vars(agent)
    if func == "restore_primary":
        rt = copy.deepcopy(r.get("rt", {}))
        agent = NS(**r.get("agent", {}))
        live.restore_primary_route_verdicts(agent, rt)
        return [rt, vars(agent)]
    return None


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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_routeverdic_40e4a2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
