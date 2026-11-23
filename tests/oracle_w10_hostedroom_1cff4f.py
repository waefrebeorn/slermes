#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_room_lead.py (gate w10_hostedroom_1cff4f)."""
import importlib.util
import json
import sys
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/gateway/hosted_room_lead.py"
NAME = "hosted_room_lead_live_w10_hostedroom"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "parse_assignment":
        members = [SimpleNamespace(handle=m["handle"], member_id=m["member_id"])
                   for m in (r.get("members") or [])]
        room = SimpleNamespace(members=members)
        return list(live.parse_assignment(r.get("text", ""), room))
    if func == "assignment":
        events = [SimpleNamespace(kind=e.get("kind", ""),
                                  payload=e.get("payload") or {},
                                  id=e.get("id")) for e in (r.get("events") or [])]
        got = live.assignment(events, r.get("discussion_event_id", ""))
        return got.id if got is not None else None
    if func == "is_routing_task":
        task = SimpleNamespace(payload=r.get("payload") or {})
        return bool(live.is_routing_task(task))
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
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w10_hostedroom_1cff4f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
