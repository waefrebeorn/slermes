#!/usr/bin/env python3
"""Differential oracle for tools/connectors/live.py (gate w24_live_416722).

The live registry functions are called for real -- ``_profile_key``, ``_key``,
``current``, ``get``, ``get_by_op_id``, ``find_target``, ``open``, ``close`` and
``OperationAlreadyOpen``'s message -- against a registry built from the fixture.
Each case rebuilds ``live._open`` from the fixture's operation list, so the module
state is input, not leftover state.

Seams (substituted, because they touch the filesystem/process):
  * ``live.hermes_home_key`` / ``live.get_process_hermes_home`` -> the fixture's
    ``process_key`` / ``active_profile_key`` (home resolution is resolve()+normcase
    on disk; the C port receives the resolved key instead);
  * the operations themselves are ``ConnectionOperation.__new__`` skeletons with
    ``settled_at``/``targets`` set, so no lock, wake event or deadline floats in.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_live_416722"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.connectors import live  # noqa: E402

WANTED = {
    "effective_profile_key", "key_pair", "select", "open_result", "close_result",
    "conflict_message", "constants",
}


def bare_op(spec):
    op = live.ConnectionOperation.__new__(live.ConnectionOperation)
    op.session_key = spec.get("session_key") or ""
    op.profile_key = spec.get("profile_key") or ""
    op.op_id = spec.get("op_id") or ""
    op.settled_at = 1.0 if spec.get("settled") else None
    op.targets = [
        SimpleNamespace(name=t.get("name"), kind=t.get("kind"),
                        alias=t.get("alias"), repair_id=t.get("repair_id"))
        for t in (spec.get("targets") or [])
    ]
    return op


def install(req):
    """Registry + home seams from one fixture case."""
    live._open.clear()
    ops = []
    for spec in req.get("ops") or []:
        op = bare_op(spec)
        ops.append(op)
        live._open[(op.profile_key, op.session_key)] = op
    process_key = req.get("process_key") or ""
    active_key = req.get("active_profile_key") or process_key
    live.get_process_hermes_home = lambda: process_key
    live.hermes_home_key = lambda path=None: path if path is not None else active_key
    return ops


def select(req):
    ops = install(req)
    which = req.get("op")
    home = req.get("profile_home")
    if which == "current":
        found = live.current(req.get("session_key") or "", profile_home=home)
    elif which == "get":
        found = live.get(req.get("session_key") or "", req.get("op_id") or "",
                         profile_home=home)
    elif which == "get_by_op_id":
        found = live.get_by_op_id(req.get("op_id") or "", profile_home=home)
    elif which == "find_target":
        found = live.find_target(req.get("name") or "", alias=req.get("alias"),
                                 repair_id=req.get("repair_id"), profile_home=home)
    else:
        raise AssertionError("unknown select op %r" % (which,))
    if found is None:
        return None
    return next((i for i, op in enumerate(ops) if op is found), None)


def open_result(req):
    install(req)
    spec = {k: req.get(k) for k in ("session_key", "op_id") if req.get(k) is not None}
    op = bare_op(spec)
    raised = stored = False
    message = None
    try:
        live.open(op)
        stored = True
    except live.OperationAlreadyOpen as exc:
        raised = True
        message = str(exc)
    return {"raised": raised, "message": message, "stored": stored,
            "op_ids": [o.op_id for o in live._open.values()]}


def close_result(req):
    ops = install(req)
    key = (req.get("profile_key") or "", req.get("session_key") or "")
    if req.get("via_registry", True) and key in live._open:
        target = live._open[key]
    else:
        target = bare_op({"session_key": key[1], "profile_key": key[0]})
    was_present = key in live._open
    live.close(target)
    return {"removed": was_present and key not in live._open,
            "op_ids": [o.op_id for o in live._open.values()]}


def expected(r):
    f = r.get("func")
    if f == "effective_profile_key":
        install(r)
        return live._profile_key(r.get("profile_home"))
    if f == "key_pair":
        install(r)
        return list(live._key(r.get("session_key") or "", r.get("profile_home")))
    if f == "select":
        return select(r)
    if f == "open_result":
        return open_result(r)
    if f == "close_result":
        return close_result(r)
    if f == "conflict_message":
        op = bare_op({"session_key": r.get("session_key"), "op_id": r.get("op_id")})
        return str(live.OperationAlreadyOpen(op))
    if f == "constants":
        return {"op_id_length": len(
            live.ConnectionOperation.__dataclass_fields__["op_id"].default_factory())}
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
