#!/usr/bin/env python3
"""Differential oracle for the outbound half of plugins/platforms/signal/adapter.py
(gate w29_adapter_d36d4e_p3).

The live ``SignalAdapter`` is imported BY FILE PATH; the class methods are
called directly and the async methods are driven with the RPC and the clock
replaced by fixture seams:

  * ``_with_target``                    groupId / recipient routing key
  * ``_validate_send_result``           per-recipient SUCCESS/type/failure
  * ``_utf16_offsets`` / ``_styles_for_chunk`` / ``_split_signal_formatted_message``
  * ``_track_sent_timestamp``           recent LRU + TTL + sent cache
  * ``send_typing``                     consecutive-failure backoff (clock seam)
  * ``_extract_reaction_target`` / ``send_reaction`` / ``remove_reaction``
  * ``send_image``                      reason → error text (media-path seam)
  * ``get_chat_info``                   group static / contact name fallback

The ``time`` module inside the live adapter is replaced by a fixed clock so the
backoff and LRU arithmetic is compared exactly, not approximately.
"""
import asyncio
import hashlib
import importlib.util
import json
import logging
import os
import sys
import types
from collections import OrderedDict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_d36d4e_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

_spec = importlib.util.spec_from_file_location(
    "sig_adapter_p3", QUARRY / "plugins" / "platforms" / "signal" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)
logging.disable(logging.CRITICAL)   # the stubbed download path logs a warning
A = live.SignalAdapter
REAL_TIME = live.time

WANTED = {
    "with_target", "validate_send_result", "utf16_offsets", "styles_for_chunk",
    "split_signal_message", "track_sent_timestamp", "typing_skip_seconds",
    "extract_reaction_target", "reaction_params", "image_error_text", "chat_info",
}


class Clock:
    """Fixed clock seam: the same 'now' for every monotonic() read."""

    def __init__(self, now):
        self.now = float(now)

    def monotonic(self):
        return self.now

    def time(self):
        return 0.0

    def sleep(self, _seconds):
        return None


def num_keys(mapping):
    out = OrderedDict()
    for k, v in (mapping or {}).items():
        try:
            out[int(k)] = v
        except (TypeError, ValueError):
            out[k] = v
    return out


def bare(account="acct-a"):
    ad = A.__new__(A)
    ad.http_url = "http://127.0.0.1:8080"
    ad.account = account
    ad._account_normalized = (account or "").strip()
    ad.gateway_runner = None
    ad.dm_allow_from = {"*"}
    ad.group_allow_from = set()
    ad._recipient_uuid_by_number = {}
    ad._recipient_number_by_uuid = {}
    ad._typing_tasks = {}
    ad._typing_failures = {}
    ad._typing_skip_until = {}
    ad._recent_sent_timestamps = OrderedDict()
    ad._sent_message_timestamps = OrderedDict()
    ad._max_sent_message_timestamps = 500
    ad._recent_sent_ttl_seconds = 300.0
    ad._max_recent_timestamps = 512
    return ad


def with_target(r):
    ad = bare(r.get("account"))
    return asyncio.run(ad._with_target({"account": ad.account}, r.get("chat_id"), resolve=False))


def validate_send_result(r):
    ad = bare(r.get("account"))
    ok, err = ad._validate_send_result(r.get("result"))
    return {"success": ok, "error": err}


def utf16_offsets(r):
    return A._utf16_offsets(r.get("text"))


def styles_for_chunk(r):
    return A._styles_for_chunk(r.get("styles") or [], r.get("start", 0), r.get("end", 0))


def split_signal_message(r):
    return [[t, st] for t, st in
            A._split_signal_formatted_message(r.get("text"), r.get("styles") or [], r.get("max_length"))]


def track_sent_timestamp(r):
    ad = bare(r.get("account"))
    ad._recent_sent_timestamps = num_keys(r.get("recent"))
    ad._recent_sent_ttl_seconds = float(r.get("ttl", 300.0))
    ad._max_recent_timestamps = int(r.get("max_recent", 512))
    ad._sent_message_timestamps = OrderedDict((str(k), None) for k in (r.get("sent_cache") or ()))
    ad._max_sent_message_timestamps = int(r.get("max_sent", 500))
    live.time = Clock(r.get("now", 0.0))
    try:
        ad._track_sent_timestamp(r.get("result"))
    finally:
        live.time = REAL_TIME
    return {"recent": [[str(k), v] for k, v in ad._recent_sent_timestamps.items()],
            "sent_cache": list(ad._sent_message_timestamps)}


def typing_skip_seconds(r):
    ad = bare(r.get("account"))
    chat = "group:g1"

    async def _rpc(*a, **k):
        return None

    ad._rpc = _rpc
    ad._typing_failures = {chat: int(r.get("prior_fails", 0))}
    ad._typing_skip_until = {}
    live.time = Clock(r.get("now", 1000.0))
    try:
        asyncio.run(ad.send_typing(chat))
        until = ad._typing_skip_until.get(chat)
        return None if until is None else until - live.time.monotonic()
    finally:
        live.time = REAL_TIME


def extract_reaction_target(r):
    ad = bare(r.get("account"))
    event = types.SimpleNamespace(raw_message=r.get("raw_message"))
    target = ad._extract_reaction_target(event)
    if not target:
        return None
    return [target[0], target[1]]


def reaction_params(r):
    ad = bare(r.get("account"))
    captured = {}

    async def _rpc(method, params, *a, **k):
        captured.clear()
        captured.update(params)
        return {}

    ad._rpc = _rpc
    if r.get("remove"):
        asyncio.run(ad.remove_reaction(r.get("chat_id"), r.get("author"), r.get("timestamp")))
    else:
        asyncio.run(ad.send_reaction(r.get("chat_id"), r.get("emoji"), r.get("author"), r.get("timestamp")))
    return dict(captured)


def image_error_text(r):
    ad = bare(r.get("account"))
    reason, detail = r.get("reason"), r.get("detail")

    async def _rpcm(*a, **k):
        return None

    async def _resolve(_url):
        return (None, reason, detail)

    ad._rpc = _rpcm
    ad._resolve_image_path = _resolve
    result = asyncio.run(ad.send_image("group:g1", "file:///tmp/x.png"))
    return result.error


def chat_info(r):
    ad = bare(r.get("account"))
    contact = r.get("contact")

    async def _rpc(*a, **k):
        return contact

    ad._rpc = _rpc
    return asyncio.run(ad.get_chat_info(r.get("chat_id")))


DISPATCH = {
    "with_target": with_target,
    "validate_send_result": validate_send_result,
    "utf16_offsets": utf16_offsets,
    "styles_for_chunk": styles_for_chunk,
    "split_signal_message": split_signal_message,
    "track_sent_timestamp": track_sent_timestamp,
    "typing_skip_seconds": typing_skip_seconds,
    "extract_reaction_target": extract_reaction_target,
    "reaction_params": reaction_params,
    "image_error_text": image_error_text,
    "chat_info": chat_info,
}


def digest(v):
    return hashlib.sha1(json.dumps(v, sort_keys=True).encode("utf-8")).hexdigest()[:10]


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            py = {"error": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r/%s c=%r/%s"
                  % (r.get("func"), r.get("case"), py, digest(py), got, digest(got)),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
