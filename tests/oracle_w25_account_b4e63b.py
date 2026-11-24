#!/usr/bin/env python3
"""Differential oracle for tools/connectors/account.py (gate w25_account_b4e63b).

``live.find_target`` is the seam: it is replaced by a lookup into the case's
``open`` map, answering one shared ``_Op`` object per operation id so the live
``is not`` identity walk in ``_matching_open_operation`` is exercised as written.
``plan`` is the live ``Target`` dataclass built exactly as ``find_or_start_operation``
builds it (``Target(name, "connector", action, alias=..., repair_id=...)``), and
the constants are read off the module plus the plan's call site in its source.
"""
import inspect
import json
import re
import sys
import uuid
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_account_b4e63b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import tools.connectors.account as live  # noqa: E402
from tools.connectors import live as registry  # noqa: E402
from tools.connectors.operation import Target  # noqa: E402

WANTED = {"matching_open_operation", "plan", "constants"}
SOURCE = inspect.getsource(live)


class _Op:
    """An open ConnectionOperation, reduced to its identity."""

    def __init__(self, op_id):
        self.op_id = op_id


def _find_target_factory(open_map):
    cache = {}

    def find_target(name, *, alias=None, repair_id=None, profile_home=None):
        op_id = open_map.get(name) if isinstance(open_map, dict) else None
        if op_id is None:
            return None
        if op_id not in cache:
            cache[op_id] = _Op(op_id)
        return cache[op_id]

    return find_target


def matching_open_operation(req):
    registry.find_target = _find_target_factory(req.get("open"))
    names = req.get("names")
    names = names if isinstance(names, list) else (list(names) if isinstance(names, str) else [])
    try:
        found = live._matching_open_operation(names, alias=req.get("alias"),
                                              repair_id=req.get("repair_id"),
                                              profile_home=req.get("profile_home"))
    except ValueError as exc:
        return {"error": "ValueError", "message": str(exc)}
    return {"found": None if found is None else found.op_id}


def plan(req):
    names = req.get("names")
    names = names if isinstance(names, list) else []
    targets = [Target(name, "connector", req.get("action"), alias=req.get("alias"),
                      repair_id=req.get("repair_id")) for name in names]
    return {
        "targets": [{"name": t.name, "kind": t.kind, "action": t.action,
                     "alias": t.alias, "repair_id": t.repair_id} for t in targets],
        "session_key_prefix": _session_key_prefix(),
    }


def _session_key_prefix():
    match = re.search(r'session_key=f"([^"{]*)\{', SOURCE)
    return match.group(1) if match else None


def constants():
    return {
        "prepare_wait_seconds": float(live._PREPARE_WAIT_SECONDS),
        "thread_name": re.search(r'name="([^"]+)"', SOURCE).group(1),
        "session_key_prefix": _session_key_prefix(),
        "session_key_hex_len": len(uuid.uuid4().hex),
        "target_kind": re.search(r'Target\(name, "([^"]+)"', SOURCE).group(1),
    }


def expected(r):
    f = r.get("func")
    if f == "matching_open_operation":
        return matching_open_operation(r)
    if f == "plan":
        return plan(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
