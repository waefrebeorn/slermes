#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_clarify_binding.py
(gate w25_telegramcl_80d613).

The live module answers: ``_ident`` directly, ``ClarifyPrompt.from_sent`` on a
SimpleNamespace standing in for the Bot API send result, ``tap_matches_prompt``
under ``asyncio.run`` with a fake query whose ``answer()`` is observed, and
``log_resolved`` through a capturing logging handler (the formatted record is
the audit line).
"""
import asyncio
import importlib
import json
import logging
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_telegramcl_80d613"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module(
    "plugins.platforms.telegram.telegram_clarify_binding")  # noqa: E402

WANTED = {"ident", "from_sent", "tap_matches", "audit_line"}


def _ns(value):
    return types.SimpleNamespace(**value) if isinstance(value, dict) else None


def ident(req):
    return live._ident(req.get("value"))


def from_sent(req):
    prompt = live.ClarifyPrompt.from_sent(req.get("session_key"), _ns(req.get("msg")))
    return {
        "session_key": prompt.session_key,
        "chat_id": prompt.chat_id,
        "message_id": prompt.message_id,
        "thread_id": prompt.thread_id,
    }


class _Query:
    def __init__(self, raw, answered):
        self.message = _ns(raw.get("message"))
        self.id = raw.get("id")
        self.from_user = types.SimpleNamespace(id=raw.get("from_id"))
        self._answered = answered

    async def answer(self, text=None):
        self._answered.append(text)


def tap_matches(req):
    raw_prompt = req.get("prompt") or {}
    prompt = live.ClarifyPrompt(
        session_key=raw_prompt.get("session_key", ""),
        chat_id=raw_prompt.get("chat_id"),
        message_id=raw_prompt.get("message_id"),
        thread_id=raw_prompt.get("thread_id"),
    )
    answered = []
    query = _Query(req.get("query") or {}, answered)
    matched = asyncio.run(live.tap_matches_prompt(
        query, "clarify-1", prompt, req.get("denial", "")))
    return {
        "matched": bool(matched),
        "answered": bool(answered),
        "denial": answered[0] if answered else None,
    }


class _Capture(logging.Handler):
    def __init__(self):
        super().__init__()
        self.messages = []

    def emit(self, record):
        self.messages.append(record.getMessage())


def audit_line(req):
    raw_query = {"id": req.get("query_id"),
                 "from_id": req.get("caller_id"),
                 "message": None}
    query = _Query(raw_query, [])
    prompt = live.ClarifyPrompt(session_key="s", chat_id=req.get("chat"),
                                message_id=req.get("message"))
    handler = _Capture()
    saved_level = live.logger.level
    live.logger.addHandler(handler)
    live.logger.setLevel(logging.DEBUG)
    try:
        live.log_resolved(query, req.get("clarify_id"), prompt,
                          req.get("choice", ""), "user-display")
    finally:
        live.logger.removeHandler(handler)
        live.logger.setLevel(saved_level)
    return handler.messages[0] if handler.messages else None


def expected(r):
    f = r.get("func")
    if f == "ident":
        return ident(r)
    if f == "from_sent":
        return from_sent(r)
    if f == "tap_matches":
        return tap_matches(r)
    if f == "audit_line":
        return audit_line(r)
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
