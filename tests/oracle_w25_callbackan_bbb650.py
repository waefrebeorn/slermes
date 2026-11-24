#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/callback_answer.py
(gate w25_callbackan_bbb650).

PTB is not installed here, so the live ``is_stale_query_error`` takes its
wording-only branch: the oracle reports the real importability as the case's
``badrequest_importable``, builds the exact exception the case names and lets the
live function decide.  ``answer_callback`` / ``answer_or_note`` / ``refuse_tap``
are awaited on fake query/adapter objects that record every ``answer(...)`` /
``_send_control_message(...)`` call, so what is compared is the call the live
coroutine made and what it returned.

``refuse_tap`` pulls its toast from ``gateway.approval_actor.refusal_text`` (an
i18n seam); the case carries that text so the port can slice it without the
localization layer.
"""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_callbackan_bbb650"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# The gateway i18n layer resolves its home at call time; the repo itself is a
# valid home and keeps the oracle from depending on the user's profile.
os.environ.setdefault("HERMES_HOME", str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "callback_answer_live", QUARRY / "plugins/platforms/telegram/callback_answer.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

try:  # PTB is optional; the live function narrows only when it is importable.
    from telegram.error import BadRequest as _BadRequest
    HAS_BADREQUEST = True
except ImportError:  # noqa: BLE001
    _BadRequest = None
    HAS_BADREQUEST = False


@contextlib.contextmanager
def _ptb_state(importable):
    """Force the import state the case asks for, on both sides.

    `is_stale_query_error` imports `telegram.error` inside the call, so the
    branch it takes is a function of sys.modules at call time -- not of what
    happened to be installed when this oracle started. A None entry in
    sys.modules makes the import raise ImportError, which is exactly the
    "PTB missing" arm the C models as badrequest_importable=false.
    """
    if importable and HAS_BADREQUEST:
        yield
        return
    saved = {k: v for k, v in sys.modules.items()
             if k == "telegram" or k.startswith("telegram.")}
    for key in saved:
        del sys.modules[key]
    sys.modules["telegram"] = None
    try:
        yield
    finally:
        sys.modules.pop("telegram", None)
        sys.modules.update(saved)

WANTED = {"is_stale_query_error", "answer_callback", "answer_or_note", "refuse_tap", "constants"}


def _make_exc(fault):
    message = fault.get("message", "")
    if HAS_BADREQUEST and fault.get("exc_type") == "BadRequest":
        return _BadRequest(message)
    return Exception(message)


class _Query:
    def __init__(self, fault=None, message=None):
        self.calls = []
        self.fault = fault
        self.message = message

    async def answer(self, **kwargs):
        self.calls.append(dict(kwargs))
        if self.fault is not None:
            raise _make_exc(self.fault)


class _Message:
    def __init__(self, data):
        self.chat_id = data.get("chat_id")
        self.message_thread_id = data.get("message_thread_id")
        self.is_topic_message = data.get("is_topic_message")


class _Adapter:
    def __init__(self, fail=False):
        self.sent = None
        self.fail = fail

    async def _send_control_message(self, chat_id, text, *, parse_mode=None, thread_id=None, metadata=None):
        if self.fail:
            raise RuntimeError("adapter send failed")
        self.sent = {"chat_id": chat_id, "text": text, "parse_mode": parse_mode,
                     "thread_id": thread_id, "metadata": metadata}


def is_stale_query_error(req):
    with _ptb_state(bool(req.get("badrequest_importable"))):
        exc = _make_exc({"message": req.get("message", ""), "exc_type": req.get("exc_type")})
        return bool(live.is_stale_query_error(exc))


def answer_callback(req):
    query = _Query(req.get("fault"))
    try:
        result = asyncio.run(live.answer_callback(query, req.get("text"),
                                                  show_alert=bool(req.get("show_alert"))))
    except Exception:  # noqa: BLE001
        return {"calls": query.calls, "result": None, "raised": True}
    return {"calls": query.calls, "result": bool(result), "raised": False}


def answer_or_note(req):
    data = {k: req.get(k) for k in ("chat_id", "message_thread_id", "is_topic_message")}
    message = None if data["chat_id"] is None and "chat_id" not in req else _Message(data)
    if req.get("no_message"):
        message = None
    query = _Query(req.get("fault"), message=message)
    adapter = _Adapter(fail=bool(req.get("send_fault")))
    try:
        asyncio.run(live.answer_or_note(adapter, query, req.get("text", "")))
    except Exception:  # noqa: BLE001
        return {"calls": query.calls, "sent": adapter.sent, "raised": True}
    return {"calls": query.calls, "sent": adapter.sent, "raised": False}


def refuse_tap(req):
    query = _Query()
    result = asyncio.run(live.refuse_tap(query, req.get("requester")))
    return {"calls": query.calls, "result": bool(result)}


def constants():
    return {
        "toast_limit": int(live.TOAST_LIMIT),
        "stale_query_phrases": list(live._STALE_QUERY_PHRASES),
    }


def expected(r):
    f = r.get("func")
    if f == "is_stale_query_error":
        return is_stale_query_error(r)
    if f == "answer_callback":
        return answer_callback(r)
    if f == "answer_or_note":
        return answer_or_note(r)
    if f == "refuse_tap":
        return refuse_tap(r)
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
