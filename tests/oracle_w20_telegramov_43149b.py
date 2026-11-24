#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_overflow_edit.py
(w20_telegramov_43149b).

The live coroutine is driven with stubbed Telegram modules: the chunker returns
the fixture's chunks, the adapter records every call it receives and answers
from the fixture's script. Both sides produce the same SendResult dict plus the
same ordered adapter-call log.
"""
import asyncio
import dataclasses
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


@dataclasses.dataclass
class SendResult:
    success: bool = True
    message_id: str = None
    error: str = None
    raw_response: object = None
    retryable: bool = False
    retry_after: float = None
    continuation_message_ids: tuple = ()
    error_kind: str = None


class Scripted:
    """One continuation answer: a message object, None, or a flood SendResult."""

    def __init__(self, spec, message_id=None):
        self.message_id = message_id


CALLS = []


def utf16_len(text):
    return len(text)


for pkg in ("gateway", "gateway.platforms", "plugins", "plugins.platforms",
            "plugins.platforms.telegram"):
    _mk(pkg)
_mk("gateway.platforms.base", SendResult=SendResult, utf16_len=utf16_len)
_mk("plugins.platforms.telegram.adapter",
    _redact_telegram_error_text=lambda exc: str(exc))
_mk("plugins.platforms.telegram.telegram_chunking",
    split_raw_carrying_emphasis=lambda text, *a, **k: (SPLIT["raw_chunks"], SPLIT["chunks"]),
    separate_chunk_indicator_from_fence=lambda text: _rec("separate_chunk_indicator_from_fence", text=text))
_mk("plugins.platforms.telegram.telegram_split",
    quotable_text=lambda text: _rec("quotable_text", text=text),
    _strip_mdv2=lambda text: _rec("_strip_mdv2", text=text))

NAME = "w20_telegramov_43149b_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "telegram", "telegram_overflow_edit.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

SPLIT = {"raw_chunks": [], "chunks": []}


def _rec(call, **kw):
    rec = {"call": call}
    rec.update(kw)
    CALLS.append(rec)
    return kw.get("text")


def norm(v):
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    return v


class Adapter:
    name = "tg"
    MAX_MESSAGE_LENGTH = 4096

    def __init__(self, first_edit, continuations, thread_id):
        self.first_edit = first_edit if isinstance(first_edit, dict) else {}
        self.continuations = continuations if isinstance(continuations, list) else []
        self.thread_id = thread_id
        self.sent = 0

    # chunker helpers
    def format_message(self, chunk):
        return _rec("format_message", text=chunk)

    def truncate_message(self, *a, **k):
        return None

    @staticmethod
    def _maybe_raise(spec):
        if spec.get("outcome") == "raise":
            raise RuntimeError(spec.get("error", ""))

    async def _edit_markdown_or_plain(self, chat_id, message_id, text, plain, log_format):
        _rec("edit_markdown_or_plain", chat_id=chat_id, message_id=message_id, text=text,
             plain_text=plain, log_format=log_format)
        self._maybe_raise(self.first_edit)

    async def _edit_text(self, chat_id, message_id, text):
        _rec("edit_text", chat_id=chat_id, message_id=message_id, text=text)
        self._maybe_raise(self.first_edit)

    async def _record_sent_text(self, chat_id, message_id, text):
        _rec("record_sent_text", chat_id=chat_id, message_id=message_id, text=text)

    def _metadata_thread_id(self, metadata):
        _rec("metadata_thread_id", thread_id=self.thread_id)
        return self.thread_id

    def _thread_kwargs_for_send(self, chat_id, thread_id, metadata, reply_to_message_id=None):
        _rec("thread_kwargs_for_send", chat_id=chat_id, thread_id=thread_id,
             reply_to_message_id=reply_to_message_id)
        return {"reply_to_message_id": reply_to_message_id}

    async def _send_overflow_continuation(self, chat_id, chunk, reply_to_id, thread_kwargs,
                                          thread_id, metadata, finalize):
        _rec("send_overflow_continuation", chat_id=chat_id, chunk=chunk,
             reply_to_id=reply_to_id, thread_id=thread_id, finalize=bool(finalize))
        spec = (self.continuations[self.sent] if self.sent < len(self.continuations)
                else {"kind": "ok", "message_id": ""})
        self.sent += 1
        kind = spec.get("kind", "ok")
        if kind == "none":
            return None
        if kind == "flood":
            return SendResult(success=False, error=spec.get("error"),
                              retry_after=spec.get("retry_after"))
        return Scripted(spec, spec.get("message_id", ""))


def expected(r):
    global SPLIT, CALLS
    CALLS = []
    split = r.get("split") if isinstance(r.get("split"), dict) else {}
    SPLIT = {"raw_chunks": split.get("raw_chunks") or [], "chunks": split.get("chunks") or []}
    adapter = Adapter(r.get("first_edit"), r.get("continuations"),
                      (r.get("adapter") or {}).get("thread_id"))
    coro = live.edit_overflow_split(adapter, r.get("chat_id", ""), r.get("message_id", ""),
                                    r.get("content", ""), finalize=bool(r.get("finalize", False)),
                                    metadata=r.get("metadata"))
    res = asyncio.new_event_loop().run_until_complete(coro)
    out = {
        "result": {"success": bool(res.success), "message_id": res.message_id,
                   "error": res.error, "raw_response": norm(res.raw_response),
                   "retryable": bool(res.retryable), "retry_after": res.retry_after,
                   "continuation_message_ids": norm(list(res.continuation_message_ids)),
                   "error_kind": res.error_kind},
        "calls": norm(CALLS),
    }
    return out


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_telegramov_43149b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
