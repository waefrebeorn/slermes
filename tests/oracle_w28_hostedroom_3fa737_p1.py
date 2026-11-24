#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_driver.py (gate w28_hostedroom_3fa737_p1).

The live module is loaded by file path (``tui_gateway/__init__`` drags the TUI
stack) with the quarry on ``sys.path`` so its absolute ``gateway``/``persistence``
imports resolve.  Every seam is out of the picture here: what is called is the
value-shaping layer the worker uses after a transport or store observation --

  * ``room_session_title`` -- the hidden "Group: <room>" transcript title;
  * ``_member_id`` -- the member a task's progress/approval belongs to;
  * ``_session_id`` -- the adapter's session-id coercion and its refusal;
  * ``_truncate_utf8`` / ``_bounded_terminal_result`` -- the 64 KiB text and
    4 KiB error bounds, never splitting a code point;
  * ``_receipt_from`` / ``discarded_receipt`` -- the settled/failed coercion and
    the "reply:<task>:<gen>" settlement fallback;
  * ``_find_terminal_receipt`` -- newest exact assistant row proving a terminal;
  * ``_session_kw`` / ``_fences``.

No store, transport, thread or clock is touched: the inputs are the fixture's.
"""
import importlib.util
import json
import sys
from dataclasses import asdict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_hostedroom_3fa737_p1"


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    path = QUARRY / "tui_gateway" / "hosted_room_driver.py"
    spec = importlib.util.spec_from_file_location("hrd_live_p1", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


live = load_live()
from persistence.gateway.hosted_rooms import HostedTaskIdentity  # noqa: E402

WANTED = {
    "room_session_title", "member_id", "session_id", "truncate_utf8", "bounded_result",
    "receipt_from", "discarded_receipt", "find_terminal_receipt", "session_kw", "fences",
}


def identity(raw=None, task_id="task-1"):
    raw = raw if isinstance(raw, dict) else {}
    return HostedTaskIdentity(
        room_id=str(raw.get("room_id") or "room-1"),
        task_id=str(raw.get("task_id") or task_id),
        thread_id=str(raw.get("thread_id") or "thread-1"),
        turn_id=str(raw.get("turn_id") or "turn-1"),
    )


def task_of(raw):
    raw = raw if isinstance(raw, dict) else {}
    out = dict(raw)
    out["identity"] = identity(raw.get("identity"))
    out.setdefault("execution_generation", 0)
    out.setdefault("cancel_generation", 0)
    out.setdefault("payload", {})
    return out


def room_session_title(req):
    return live.room_session_title(req.get("room_id") or "")


def member_id(req):
    return live._member_id(task_of(req.get("task")))


def session_id(req):
    try:
        value = live._session_id(req.get("session"))
    except ValueError as exc:
        return {"ok": False, "value": None, "error": str(exc)}
    return {"ok": True, "value": value, "error": None}


def truncate_utf8(req):
    text, flag = live._truncate_utf8(req.get("text", ""), max_bytes=int(req["max_bytes"]))
    return {"text": text, "truncated": flag}


def bounded_result(req):
    return live._bounded_terminal_result(req.get("receipt") or {})


def receipt_from(req):
    return asdict(live._receipt_from(req.get("receipt") or {},
                                     identity(task_id=req["task_id"]),
                                     int(req["generation"])))


def discarded_receipt(req):
    receipt = live.discarded_receipt(req.get("result") or {}, task_of(req.get("task")))
    return asdict(receipt) if receipt is not None else None


def find_terminal_receipt(req):
    receipt = live._find_terminal_receipt(req.get("history") or [],
                                          identity(task_id=req["task_id"]),
                                          int(req["generation"]))
    return asdict(receipt) if receipt is not None else None


def session_kw(req):
    return live._session_kw(req.get("profile"), req.get("session_id"))


def fences(req):
    return live._fences(task_of(req.get("task")))


HANDLERS = {
    "room_session_title": room_session_title,
    "member_id": member_id,
    "session_id": session_id,
    "truncate_utf8": truncate_utf8,
    "bounded_result": bounded_result,
    "receipt_from": receipt_from,
    "discarded_receipt": discarded_receipt,
    "find_terminal_receipt": find_terminal_receipt,
    "session_kw": session_kw,
    "fences": fences,
}


def expected(req):
    func = req.get("func")
    handler = HANDLERS.get(func)
    if handler is None:
        raise AssertionError("unknown func %r" % (func,))
    return handler(req)


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
            want = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (req.get("func"), req.get("case"), exc),
                  file=sys.stderr)
            continue
        got = req.get("out")
        if want != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (req.get("func"), req.get("case"), want, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
