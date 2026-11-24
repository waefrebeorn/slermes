#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/teams/adapter.py
(gate w29_adapter_1c52a4_p2).

The live module is imported BY FILE PATH.  The message/card shaping decisions
are compared by *running* the live code where it exists, and by driving the live
inbound/outbound paths where the decision is inline:

  * ``TeamsAdapter._activity_mentions_bot`` -- mention entities vs the <at> tag;
  * ``_approval_body`` -- TextBlock kwargs, with TextBlock/t stubbed (SDK + i18n
    are seams; the fixture supplies the resolved catalog strings);
  * ``_on_message`` media-kind precedence (the precedence table is the live one);
  * ``TeamsAdapter._cache_attachment`` -- driven with fake attachment bytes and a
    fake media cache to observe the body/card skip and the download-info
    filename the live code derives;
  * ``TeamsAdapter.send`` -- driven against a fake App to observe whether the
    reply_to predicate routes to App.reply or App.send.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_1c52a4_p2"

# hermes_constants resolves the Hermes home through Path.home(); MSYS bash does
# not always export USERPROFILE/HOME to the native interpreter, so anchor it at
# the repository's parent.  Nothing under test reads HOME.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "hermes_teams_adapter_live_p2", QUARRY / "plugins/platforms/teams/adapter.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

A = live.TeamsAdapter

WANTED = {"mentions_bot", "approval_body", "media_type", "attachment_skip",
          "document_filename", "threaded_reply"}


def _entity(raw):
    """A duck-typed activity entity: only ``type`` and ``mentioned.id`` are read."""
    if not isinstance(raw, dict):
        return None
    mentioned = raw.get("mentioned")
    if isinstance(mentioned, dict):
        mentioned = types.SimpleNamespace(**mentioned)
    return types.SimpleNamespace(type=raw.get("type"), mentioned=mentioned)


def mentions_bot(r):
    entities = [e for e in (_entity(x) for x in (r.get("entities") or [])) if e is not None]
    activity = types.SimpleNamespace(entities=entities)
    return bool(A._activity_mentions_bot(activity, set(r.get("bot_ids") or []),
                                        r.get("text") or ""))


def approval_body(r):
    class _TextBlock:
        def __init__(self, **kwargs):
            self.__dict__.update(kwargs)

    texts = {
        "gateway.exec_approval.header": r.get("header", ""),
        "gateway.exec_approval.reason_label": r.get("reason_label", ""),
    }
    saved = (live.TextBlock, live.t)
    live.TextBlock = _TextBlock
    live.t = lambda key, *a, **kw: texts.get(key, key)
    try:
        body = live._approval_body(r.get("cmd"), r.get("desc"),
                                   always=bool(r.get("always")))
    finally:
        live.TextBlock, live.t = saved
    return [dict(vars(block)) for block in body]


def media_type(r):
    kinds = r.get("kinds") or []
    mt = next((t for kind, t in live._MEDIA_KIND_PRECEDENCE if kind in kinds),
              live.MessageType.TEXT)
    return mt.value


class _Cached:
    def __init__(self, path, media_type, kind):
        self.path = path
        self.media_type = media_type
        self.kind = kind


def _drive_cache(r):
    """Run the live _cache_attachment over a fake attachment; report what it did."""
    calls = {"fetch": 0, "image": 0}
    captured = {}
    adapter = A.__new__(A)

    async def fetch(url, timeout=30.0):
        calls["fetch"] += 1
        return b"payload"

    adapter._fetch_attachment_bytes = fetch
    att = types.SimpleNamespace(content_url=r.get("content_url"),
                                content_type=r.get("content_type"),
                                name=r.get("name") or "",
                                content=r.get("content"))

    async def cache_bytes(data, filename=None, mime_type=""):
        captured["filename"] = filename
        return _Cached("/tmp/cached", mime_type or "application/octet-stream", "document")

    async def cache_image(url):
        calls["image"] += 1
        return "/tmp/image"

    saved = (live.cache_media_bytes_async, live.cache_image_from_url)
    live.cache_media_bytes_async = cache_bytes
    live.cache_image_from_url = cache_image
    try:
        asyncio.run(adapter._cache_attachment(att))
    finally:
        live.cache_media_bytes_async, live.cache_image_from_url = saved
    return calls, captured


def attachment_skip(r):
    calls, captured = _drive_cache(r)
    return not (calls["fetch"] or calls["image"] or "filename" in captured)


def document_filename(r):
    _, captured = _drive_cache(r)
    return captured.get("filename")


def threaded_reply(r):
    class _Result:
        id = "resp-1"

    class _App:
        def __init__(self):
            self.replied = 0
            self.sent = 0

        async def reply(self, chat_id, reply_to, chunk):
            self.replied += 1
            return _Result()

        async def send(self, chat_id, chunk):
            self.sent += 1
            return _Result()

    adapter = A.__new__(A)
    app = _App()
    adapter._app = app
    adapter.truncate_message = lambda text: [text]
    adapter.format_message = lambda text: text
    adapter._remember_sent = lambda result: None
    asyncio.run(adapter.send("chat-id", "hello", reply_to=r.get("reply_to")))
    return app.replied == 1


HANDLERS = {
    "mentions_bot": mentions_bot,
    "approval_body": approval_body,
    "media_type": media_type,
    "attachment_skip": attachment_skip,
    "document_filename": document_filename,
    "threaded_reply": threaded_reply,
}


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
            py = HANDLERS[r["func"]](r)
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
