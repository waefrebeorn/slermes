#!/usr/bin/env python3
"""Differential oracle for the Buzz adapter's CLI plumbing / send-shaping
leaves (gate w27_adapter_04abb5_p2), driven against the LIVE
plugins/platforms/buzz/adapter.py.

Ported symbols and their live entry points:

  member_candidates     BuzzAdapter._channel_member_pubkeys   (seam: _cli_json)
  profile_name          BuzzAdapter._profile_display_name    (seam: _cli_json)
  mention_pubkeys       BuzzAdapter._mention_pubkeys_for     (seam: member/profile lookups)
  send_ladder           BuzzAdapter._run_message_send        (seam: _run_cli, scripted)
  send_result           BuzzAdapter._send_result             (pure)
  chat_info             BuzzAdapter.get_chat_info            (seam: _cli_json)
  websocket_url         BuzzAdapter._websocket_url           (pure)
  channel_subscription  BuzzAdapter._send_channel_subscription (seam: _send_req)
  subscriptions         BuzzAdapter._subscribe_websocket     (seam: _send_req)

Every seam is the subprocess/relay boundary: the call sequences it would make
are exactly what the C side rebuilds from the same JSON inputs.
"""
import asyncio
import json
import os
import sys
import tempfile
from collections import OrderedDict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_04abb5_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_TMPHOME = tempfile.mkdtemp(prefix="w27-buzz-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

from plugins.platforms.buzz import adapter as live  # noqa: E402

WANTED = {
    "member_candidates", "profile_name", "mention_pubkeys", "send_ladder",
    "send_result", "chat_info", "websocket_url", "channel_subscription",
    "subscriptions",
}


def bare():
    """An adapter with no transport, no CLI and no channels."""
    a = live.BuzzAdapter.__new__(live.BuzzAdapter)
    a._member_cache = {}
    a._profile_name_cache = {}
    a._channel_state = {}
    a._channel_names = {}
    a._restricted_channels = set()
    a._self_pubkey = ""
    a._self_npub = ""
    a._display_name = ""
    a._user_names = {}
    a.cli_path = ""
    return a


async def member_candidates(r):
    a = bare()
    members = r.get("members") or []
    messages = r.get("messages") or []

    async def cli(args, default):
        if list(args[:2]) == ["channels", "members"]:
            return list(members)
        return list(messages)

    a._cli_json = cli
    return await a._channel_member_pubkeys(str(r.get("chat_id") or ""))


async def profile_name(r):
    a = bare()
    profiles = r.get("profiles") or []

    async def cli(args, default):
        return list(profiles)

    a._cli_json = cli
    return await a._profile_display_name(str(r.get("pubkey") or ""))


async def mention_pubkeys(r):
    a = bare()
    a._self_pubkey = r.get("self_pubkey") or ""
    members = r.get("members") or []
    names = {}
    for m in members:
        names.setdefault(m.get("pubkey"), m.get("name") or "")
    order = [m.get("pubkey") for m in members]

    async def cm(chat_id):
        return list(order)

    async def pdn(pk):
        return names.get(pk, "")

    a._channel_member_pubkeys = cm
    a._profile_display_name = pdn
    return await a._mention_pubkeys_for(str(r.get("chat_id") or ""), r.get("content") or "")


async def send_ladder(r):
    a = bare()
    a._self_pubkey = r.get("self_pubkey") or ""
    script = list(r.get("script") or [])
    calls = []
    inputs = []
    state = {"i": 0}

    async def run_cli(args, input_text=None):
        calls.append(list(args))
        inputs.append("" if input_text is None else input_text)
        if state["i"] >= len(script):
            raise RuntimeError("SCRIPT_EXHAUSTED")
        entry = script[state["i"]]
        state["i"] += 1
        return int(entry[0]), entry[1] or "", entry[2] or ""

    a._run_cli = run_cli
    try:
        code, out, err = await a._run_message_send(
            list(r.get("args") or []), r.get("content") or "", list(r.get("mention_pubkeys") or []))
    except RuntimeError:
        return {"calls": calls, "inputs": inputs, "error": "SCRIPT_EXHAUSTED"}
    return {"calls": calls, "inputs": inputs, "rc": code, "out": out, "err": err}


def send_result(r):
    a = bare()
    redact = r.get("redact")
    res = a._send_result(str(r.get("chat_id") or ""), int(r.get("code") or 0),
                         r.get("stdout") or "", r.get("stderr") or "",
                         redact_path=(Path(redact) if redact else None))
    return {"success": res.success, "message_id": res.message_id,
            "error": res.error, "retryable": res.retryable}


async def chat_info(r):
    a = bare()
    a._channel_names = dict(r.get("names") or {})
    a._channel_state = dict(r.get("state") or {})
    a.cli_path = r.get("cli_path") or ""
    reply = r.get("channel_reply") or {}

    async def cli(args, default):
        return reply

    a._cli_json = cli
    return await a.get_chat_info(str(r.get("chat_id") or ""))


def websocket_url(r):
    a = bare()
    a.relay_url = r.get("relay_url") or ""
    try:
        return {"ok": a._websocket_url()}
    except ValueError as exc:
        return {"error": str(exc)}


async def channel_subscription(r):
    a = bare()
    ch = str(r.get("channel_id") or "")
    a._channel_state = {ch: dict(r.get("state") or {})}
    captured = {}

    async def send_req(ws, sub_id, filt):
        captured.clear()
        captured.update(filt)

    a._send_req = send_req
    await a._send_channel_subscription(None, "s1", ch)
    return captured


async def subscriptions(r):
    a = bare()
    a._channel_state = OrderedDict((k, dict(v)) for k, v in (r.get("channel_state") or {}).items())
    a._restricted_channels = set(r.get("restricted") or [])
    a._self_pubkey = r.get("self_pubkey") or ""
    a._membership_since = int(r.get("membership_since") or 0)
    sends = []

    async def send_req(ws, sub_id, filt):
        sends.append([sub_id, filt])

    a._send_req = send_req
    subs = await a._subscribe_websocket(None)
    return {"subs": [[k, v] for k, v in subs.items()], "sends": sends}


def expected(r):
    f = r.get("func")
    if f == "member_candidates":
        return asyncio.run(member_candidates(r))
    if f == "profile_name":
        return asyncio.run(profile_name(r))
    if f == "mention_pubkeys":
        return asyncio.run(mention_pubkeys(r))
    if f == "send_ladder":
        return asyncio.run(send_ladder(r))
    if f == "send_result":
        return send_result(r)
    if f == "chat_info":
        return asyncio.run(chat_info(r))
    if f == "websocket_url":
        return websocket_url(r)
    if f == "channel_subscription":
        return asyncio.run(channel_subscription(r))
    if f == "subscriptions":
        return asyncio.run(subscriptions(r))
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
