#!/usr/bin/env python3
"""Differential oracle for the Buzz adapter's inbound shaping leaves
(gate w27_adapter_04abb5_p3), driven against the LIVE
plugins/platforms/buzz/adapter.py.

Ported symbols and their live entry points:

  ws_message        BuzzAdapter._handle_ws_message   (seams: _handle_events,
                                                      _rediscover_and_subscribe)
  imeta_parse       BuzzAdapter._parse_imeta_attachments
  imeta_attachments BuzzAdapter._imeta_attachments
  rejection_note    BuzzAdapter._attachment_rejection_note
  user_name         BuzzAdapter._resolve_user_name   (seam: _run_cli)
  dispatch_shape    BuzzAdapter._dispatch_message    (seams: _localize_inbound_media,
                                                      build_source, handle_message,
                                                      send_reaction)

The compared value is what the live function DECIDED: which frame route it
took and what state it left, which imeta tags it accepted, and the MessageEvent
fields it shaped.
"""
import asyncio
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_04abb5_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_TMPHOME = tempfile.mkdtemp(prefix="w27-buzz-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

from plugins.platforms.buzz import adapter as live  # noqa: E402

WANTED = {"ws_message", "imeta_parse", "imeta_attachments", "rejection_note",
          "user_name", "dispatch_shape"}


def bare():
    a = live.BuzzAdapter.__new__(live.BuzzAdapter)
    a._channel_state = {}
    a._channel_names = {}
    a._restricted_channels = set()
    a._user_names = {}
    a._self_pubkey = ""
    a._self_npub = ""
    a._display_name = ""
    a._membership_since = 0
    a._message_handler = None
    a.cli_path = ""
    return a


async def ws_message(r):
    a = bare()
    a._channel_state = dict(r.get("channel_state") or {})
    a._membership_since = int(r.get("membership_since") or 0)
    subs = dict(r.get("subscriptions") or {})
    dispatched = []
    rediscovered = []

    async def handle_events(channel_id, state, events):
        dispatched.append(channel_id)

    async def redisc(ws, subs2):
        rediscovered.append(sorted(subs2))

    a._handle_events = handle_events
    a._rediscover_and_subscribe = redisc
    message = r.get("message")
    try:
        await a._handle_ws_message(None, subs, message)
    except ConnectionError as exc:
        return {"action": "raise", "detail": str(exc)}
    if dispatched:
        return {"action": "dispatch", "channel": dispatched[0]}
    if rediscovered:
        return {"action": "rediscover", "membership_since": a._membership_since}
    head = message[0] if isinstance(message, list) and message else None
    if head == "NOTICE":
        return {"action": "notice"}
    if head == "CLOSED" and a._restricted_channels:
        detail = message[-1] if len(message) > 2 else "subscription closed"
        sub_id = str(message[1]) if len(message) > 1 else ""
        return {"action": "drop", "detail": str(detail), "channel": sorted(a._restricted_channels)[0],
                "restricted": sorted(a._restricted_channels),
                "state_left": list(a._channel_state), "subs_removed": sub_id not in subs}
    return {"action": "ignore"}


def imeta_parse(r):
    attachments, rejected = live.BuzzAdapter._parse_imeta_attachments(r.get("event") or {})
    return {"attachments": attachments, "rejected": rejected}


def imeta_attachments(r):
    return live.BuzzAdapter._imeta_attachments(r.get("event") or {})


def rejection_note(r):
    return live.BuzzAdapter._attachment_rejection_note(int(r.get("rejected") or 0))


async def user_name(r):
    a = bare()
    a._user_names = dict(r.get("cached") or {})
    code = int(r.get("code") or 0)
    out = r.get("stdout") or ""

    async def run_cli(args, input_text=None):
        return code, out, ""

    a._run_cli = run_cli
    return await a._resolve_user_name(r.get("pubkey") or "")


async def dispatch_shape(r):
    a = bare()
    a._channel_names = dict(r.get("channel_names") or {})
    a._message_handler = object()
    loc = r.get("localized") or {}
    seen = {}

    async def localize(text, message_id, *, user_id="", chat_type=None, chat_id=None):
        return (loc.get("text", ""), list(loc.get("urls") or []),
                list(loc.get("types") or []),
                live.MessageType(loc.get("message_type") or "text"))

    def build_source(**kw):
        seen["chat_name"] = kw.get("chat_name")
        return {"chat_name": kw.get("chat_name")}

    async def handle_message(event):
        seen["event"] = event

    async def send_reaction(chat_id, message_id, emoji):
        return True

    a._localize_inbound_media = localize
    a.build_source = build_source
    a.handle_message = handle_message
    a.send_reaction = send_reaction
    await a._dispatch_message(
        text=r.get("text") or "", chat_id=r.get("chat_id") or "", chat_type="group",
        user_id="u1", user_name="U", message_id="m1", created_at=0,
        media_urls=list(r.get("media_urls") or []), media_types=list(r.get("media_types") or []),
        message_type=live.MessageType(r.get("message_type") or "text"),
    )
    ev = seen["event"]
    return {"text": ev.text, "message_type": ev.message_type.name.lower(),
            "media_urls": list(ev.media_urls), "media_types": list(ev.media_types),
            "media_text_inlined": list(ev.media_text_inlined),
            "chat_name": seen.get("chat_name")}


def expected(r):
    f = r.get("func")
    if f == "ws_message":
        return asyncio.run(ws_message(r))
    if f == "imeta_parse":
        return imeta_parse(r)
    if f == "imeta_attachments":
        return imeta_attachments(r)
    if f == "rejection_note":
        return rejection_note(r)
    if f == "user_name":
        return asyncio.run(user_name(r))
    if f == "dispatch_shape":
        return asyncio.run(dispatch_shape(r))
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
