#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_prefix_gate.py
(gate w25_delegateto_a1fd91).

The live module is imported and its gate is handed stand-ins for the agent
children: an object exposing exactly the attributes the fixture names (anything
unnamed reads as None, like ``getattr(child, attr, None)``).  So the route-key
tuple, the ``is True`` prompt-caching identity, the first-response test and the
leader/follower decisions all run live; the thread, the clock and the wait are
the fixed inputs the port drops.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_delegateto_a1fd91"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import delegate_tool_prefix_gate as live  # noqa: E402

WANTED = {"route_key", "caches_prompts", "first_response_started",
          "for_children", "follows", "constants"}


class Child:
    """Stands in for a child agent: named attributes only, None otherwise."""

    def __init__(self, data):
        self._data = data if isinstance(data, dict) else {}

    def __getattr__(self, name):
        return self._data.get(name)


def child_of(data):
    return Child(data)


def route_key(req):
    return list(live._route_key(child_of(req.get("child"))))


def caches_prompts(req):
    return live._caches_prompts(child_of(req.get("child")))


def first_response_started(req):
    return live._first_response_started(child_of(req.get("child")))


def for_children(req):
    raw = req.get("children")
    entries = raw if isinstance(raw, list) else []
    children = [(i, "task", child_of(entry)) for i, entry in enumerate(entries)]
    gate = live.PrefixCacheGate.for_children(children, req.get("max_wait_s"))
    return {"gate": gate is not None}


def follows(req):
    leader = child_of(req.get("leader"))
    gate = live.PrefixCacheGate(leader, 30.0)
    child = leader if req.get("same") else child_of(req.get("child"))
    return {"follows": gate.follows(child)}


def constants():
    return {
        "first_chunk_activity": live._FIRST_CHUNK_ACTIVITY,
        "poll_seconds": live._POLL_S,
    }


def expected(req):
    func = req.get("func")
    if func == "route_key":
        return route_key(req)
    if func == "caches_prompts":
        return caches_prompts(req)
    if func == "first_response_started":
        return first_response_started(req)
    if func == "for_children":
        return for_children(req)
    if func == "follows":
        return follows(req)
    if func == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
