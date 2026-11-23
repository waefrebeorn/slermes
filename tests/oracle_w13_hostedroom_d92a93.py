#!/usr/bin/env python3
"""Differential oracle for gate w13_hostedroom_d92a93 (gateway/hosted_room_router.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_hostedroom_d92a93"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace

SRC = QUARRY + "/gateway/hosted_room_router.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "routed_event_id":
        return live.routed_event_id(r.get("id"))
    if func == "continue_count":
        events = [NS(kind=e.get("kind"), payload=e.get("payload") or {}) for e in (r.get("events") or [])]
        return live.continue_count(events, r.get("id"))
    if func == "routed_payload":
        return live.routed_payload(discussion_event_id=r.get("discussion_event_id"),
                                   thread_id=r.get("thread_id"), member_ids=r.get("member_ids"))
    if func == "one_line":
        return live._one_line(r.get("text"))
    if func == "parse_router_answer":
        return live.parse_router_answer(r.get("text"), r.get("handles") or {})
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
