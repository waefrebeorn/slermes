#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/mattermost/adapter.py
(gate w30_adapter_7683aa_p2).

The live adapter is loaded by file path after the host's seam stubs are
installed.  The WebSocket, the REST session, the media cache and the
deduplicator's clock are seams: every case feeds them as fixtures, and what is
compared is exactly what the inbound half of the adapter decides:

  * _apply_channel_gating           - allowlist, require_mention, free channels,
                                      @mention stripping
  * _is_configured_bot_user         - MATTERMOST_BOT_USERS membership
  * _post_message                   - the posts payload (root_id, file_ids, props)
  * _post_preserving_thread         - flat-channel fallback for a bad thread root
  * _handle_ws_event                - admission, gating, thread derivation,
                                      chat_type / user_name / is_bot / msg_type
  * _resolve_root_id                - GET posts/{id} -> thread root
  * send_typing                     - users/{bot}/typing path + payload
  * send_multiple_images            - the empty-batch guard
  * _post_with_file                 - caption/file_ids + SendResult shaping

OS env is a fixture seam: each case carries the MATTERMOST_* values it wants
visible and the oracle clears the rest before calling.
"""
import asyncio
import copy
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_adapter_7683aa_p2"

# A login shell may export HOME=/home/<user> (msys) and no USERPROFILE, which
# makes Windows pathlib refuse to resolve $HOME while the gateway imports.
# The repo's parent IS the user profile; seed it when the host left it blank.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)

for _p in (str(REPO / "tests"), str(QUARRY)):
    if _p not in sys.path:
        sys.path.insert(0, _p)
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_SRC = QUARRY / "plugins" / "platforms" / "mattermost" / "adapter.py"
_spec = importlib.util.spec_from_file_location("mm_adapter_live_p2", str(_SRC))
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

ADAPTER = live.MattermostAdapter

WANTED = {
    "apply_channel_gating", "is_configured_bot_user", "post_payload",
    "broken_root_fallback", "ws_event", "resolved_root_id", "typing_request",
    "multi_image_empty", "post_with_file_result",
}

ENV_KEYS = (
    "MATTERMOST_URL", "MATTERMOST_TOKEN", "MATTERMOST_ALLOWED_CHANNELS",
    "MATTERMOST_REQUIRE_MENTION", "MATTERMOST_FREE_RESPONSE_CHANNELS",
    "MATTERMOST_BOT_USERS", "MATTERMOST_REPLY_MODE", "MATTERMOST_HOME_CHANNEL",
)


def apply_env(req):
    for k in ENV_KEYS:
        os.environ.pop(k, None)
    for k, v in (req.get("env") or {}).items():
        os.environ[k] = str(v)


def bare(req):
    """An instance with no session: every I/O edge is a fixture seam."""
    a = ADAPTER.__new__(ADAPTER)
    a.platform = live.Platform.MATTERMOST
    a._bot_user_id = req.get("bot_user_id", "")
    a._bot_username = req.get("bot_username", "")
    a._reply_mode = req.get("reply_mode", "off")
    a.config = SimpleNamespace(extra=req.get("extra") or {})
    a._dedup = live.MessageDeduplicator()
    for seen in req.get("seen") or []:
        a._dedup.is_duplicate(seen)
    a._last_post_status = None
    a._last_post_error = ""
    return a


def send_result(r):
    return {"success": r.success, "message_id": r.message_id, "error": r.error}


def wire_post(a, req, posts, first_ok=True):
    async def api_post(path, payload):
        posts.append(copy.deepcopy(payload))
        if len(posts) == 1:
            a._last_post_status = req.get("status")
            a._last_post_error = req.get("error", "")
            return {"id": "post-1"} if first_ok else {}
        return {"id": "post-2"}

    async def resolve(post_id):
        if "resolved_root" in req:
            return req["resolved_root"]
        return post_id

    a._api_post = api_post
    a._resolve_root_id = resolve


def post_call(a, req):
    return asyncio.run(a._post_message(
        req.get("chat_id", ""), req.get("message", ""), req.get("reply_to"),
        req.get("metadata"), req.get("file_ids")))


def post_payload(req):
    a = bare(req)
    posts = []
    wire_post(a, req, posts)
    post_call(a, req)
    return posts[0]


def broken_root_fallback(req):
    a = bare(req)
    posts = []
    wire_post(a, req, posts, first_ok=bool(req.get("posted_ok", True)))
    post_call(a, req)
    flat = len(posts) > 1
    return {"flat_fallback": flat, "posts": len(posts),
            "flat_keys": sorted(posts[1].keys()) if flat else None}


def ws_event(req):
    a = bare(req)
    rec = {}

    def build_source(**kw):
        rec["source"] = kw
        return SimpleNamespace(**kw)

    async def handle(ev):
        rec["event"] = ev

    async def download(fids):
        return list(req.get("media_urls") or []), list(req.get("media_types") or [])

    a.build_source = build_source
    a.handle_message = handle
    a._is_sender_authorized = lambda *args, **kw: req.get("authorized", False)
    a._download_attachments = download
    asyncio.run(a._handle_ws_event(req.get("event") or {}))
    if "event" not in rec:
        return {"dispatched": False}
    ev, src = rec["event"], rec["source"]
    return {
        "dispatched": True,
        "text": ev.text,
        "message_type": ev.message_type.value,
        "chat_id": src.get("chat_id"),
        "chat_type": src.get("chat_type"),
        "user_id": src.get("user_id"),
        "user_name": src.get("user_name"),
        "thread_id": src.get("thread_id"),
        "message_id": src.get("message_id"),
        "is_bot": src.get("is_bot"),
        "media_urls": list(ev.media_urls or []),
        "media_types": list(ev.media_types or []),
    }


def resolved_root_id(req):
    a = bare(req)
    data = req.get("api_data")

    async def api_get(path):
        return data if data is not None else {}

    a._api_get = api_get
    return asyncio.run(a._resolve_root_id(req.get("post_id", "")))


def typing_request(req):
    a = bare(req)
    rec = {}

    async def api_post(path, payload):
        rec["path"] = path
        rec["payload"] = copy.deepcopy(payload)
        return {}

    a._api_post = api_post
    asyncio.run(a.send_typing(req.get("chat_id", "")))
    return {"path": rec.get("path"), "payload": rec.get("payload")}


def multi_image_empty(req):
    a = bare(req)
    res = asyncio.run(a.send_multiple_images(req.get("chat_id", ""), req.get("images") or []))
    return send_result(res)


def post_with_file_result(req):
    a = bare(req)
    rec = {}

    async def post_message(chat_id, message, reply_to=None, metadata=None, file_ids=None):
        rec["message"] = message
        rec["file_ids"] = file_ids
        return req.get("data")

    a._post_message = post_message
    res = asyncio.run(a._post_with_file(
        req.get("chat_id", ""), req.get("file_id"), req.get("caption"),
        req.get("reply_to"), req.get("metadata")))
    out = send_result(res)
    out["message"] = rec.get("message")
    out["file_ids"] = rec.get("file_ids")
    return out


def expected(r):
    f = r.get("func")
    if f == "apply_channel_gating":
        return bare(r)._apply_channel_gating(r.get("channel_id", ""), r.get("message_text", ""))
    if f == "is_configured_bot_user":
        return bare(r)._is_configured_bot_user(r.get("user_id"))
    if f == "post_payload":
        return post_payload(r)
    if f == "broken_root_fallback":
        return broken_root_fallback(r)
    if f == "ws_event":
        return ws_event(r)
    if f == "resolved_root_id":
        return resolved_root_id(r)
    if f == "typing_request":
        return typing_request(r)
    if f == "multi_image_empty":
        return multi_image_empty(r)
    if f == "post_with_file_result":
        return post_with_file_result(r)
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
        apply_env(r)
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
