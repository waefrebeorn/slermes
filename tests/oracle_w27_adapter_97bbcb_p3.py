#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/google_chat/adapter.py (gate
w27_adapter_97bbcb_p3).

Gate 3 of 4: outbound shaping.

The bodies are produced by the live methods, observed at the Chat client
boundary: a recording stand-in for ``chat_api`` captures the exact
``messages.patch``/``messages.create`` kwargs (and the ``media.upload`` body)
the adapter would send.  Seams are only ever stubbed at their edge:

  * ``_chunk_text`` / ``_resolve_thread_id`` / ``_note_rate_limit`` - direct;
  * ``_patch_message`` - updateMask + patch body as handed to the API;
  * ``send_card`` - the create body behind a card;
  * ``_send_file`` - the attachment body (os.path.exists and MediaFileUpload
    are the only things faked, plus the user-OAuth client acquisition);
  * ``send_typing`` / ``stop_typing`` / ``_consume_typing_card_with_text`` -
    the guard veredicts, observed through what the call actually did;
  * ``get_chat_info`` - the shape of the space lookup (and its error fallback);
  * ``media_filename`` / ``upload_basename`` - the inline filename/ext
    expressions in ``_download_attachment`` / ``_send_file``, which have no
    callable of their own (transcribed literally).
"""
import asyncio
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_97bbcb_p3"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.google_chat import adapter as live  # noqa: E402

A = live.GoogleChatAdapter

WANTED = {"chunk_text", "resolve_thread_id", "patch_shape", "send_card_shape", "attachment_body",
          "media_filename", "upload_basename", "note_rate_limit", "typing_send_action",
          "typing_stop_action", "typing_consume_action", "chat_info_shape"}


class Quiet:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


live.logger = Quiet()


def same(py, c):
    if isinstance(py, bool) or isinstance(c, bool):
        return isinstance(py, bool) and isinstance(c, bool) and py == c
    if isinstance(py, float) or isinstance(c, float):
        if not isinstance(py, (int, float)) or not isinstance(c, (int, float)):
            return False
        return abs(float(py) - float(c)) <= 1e-9 * max(1.0, abs(float(py)), abs(float(c)))
    if isinstance(py, dict) and isinstance(c, dict):
        if set(py) != set(c):
            return False
        return all(same(py[k], c[k]) for k in py)
    if isinstance(py, list) and isinstance(c, list):
        return len(py) == len(c) and all(same(x, y) for x, y in zip(py, c))
    return py == c


class Store:
    def __init__(self, counts=None):
        self.counts = {k: dict(v) for k, v in (counts or {}).items()}

    def incr(self, chat_id, thread_name):
        c = self.counts.setdefault(chat_id, {})
        prev = c.get(thread_name, 0)
        c[thread_name] = prev + 1
        return prev


class Req:
    def __init__(self, resp):
        self._resp = resp

    def execute(self, **kw):
        return self._resp


class Recorder:
    def __init__(self):
        self.calls = []
        self.upload_ref = None
        self.get_resp = {}
        self.get_error = None

    def record(self, kind, kw, resp):
        self.calls.append((kind, kw))
        return Req(resp)

    def last(self, kind):
        for k, kw in reversed(self.calls):
            if k == kind:
                return kw
        return None


class Messages:
    def __init__(self, rec):
        self.rec = rec

    def patch(self, **kw):
        return self.rec.record("patch", kw, {"name": kw.get("name")})

    def create(self, **kw):
        return self.rec.record("create", kw, {"name": "spaces/A/messages/99"})


class Media:
    def __init__(self, rec):
        self.rec = rec

    def upload(self, **kw):
        return self.rec.record("upload", kw, {"attachmentDataRef": self.rec.upload_ref})


class Spaces:
    def __init__(self, rec):
        self.rec = rec

    def messages(self):
        return Messages(self.rec)

    def get(self, **kw):
        if self.rec.get_error is not None:
            raise self.rec.get_error
        return self.rec.record("get", kw, self.rec.get_resp)


class ChatApi:
    def __init__(self, rec):
        self.rec = rec

    def spaces(self):
        return Spaces(self.rec)

    def media(self):
        return Media(self.rec)


class HttpErr(Exception):
    def __init__(self, status):
        super().__init__("http %s" % status)

        class _Resp:
            pass

        r = _Resp()
        r.status = status
        r.reason = "err"
        self.resp = r


def fake_adapter(rec=None, **attrs):
    rec = rec or Recorder()
    o = A.__new__(A)
    o._chat_api = ChatApi(rec)
    o._new_authed_http = lambda: None
    o._thread_count_store = Store()
    o._last_inbound_thread = {}
    o._last_sender_by_chat = {}
    o._typing_messages = {}
    o._orphan_typing_messages = {}
    o._typing_card_inflight = {}
    o._rate_limit_hits = {}
    o.config = SimpleNamespace(typing_status_text="thinking")
    for k, v in attrs.items():
        setattr(o, k, v)
    return o, rec


def chunk_text(req):
    return A._chunk_text(A.__new__(A), req.get("text") or "")


def resolve_thread_id(req):
    o, _ = fake_adapter(_last_inbound_thread=dict(req.get("last_inbound_thread") or {}))
    return o._resolve_thread_id(req.get("reply_to"), req.get("metadata"), chat_id=req.get("chat_id"))


def patch_shape(req):
    o, rec = fake_adapter()
    asyncio.run(A._patch_message(o, "spaces/A/messages/1", req.get("body") or {}))
    kw = rec.last("patch") or {}
    return {"update_mask": kw.get("updateMask"), "patch_body": kw.get("body")}


def send_card_shape(req):
    o, rec = fake_adapter(_last_inbound_thread=dict(req.get("last_inbound_thread") or {}))
    asyncio.run(A.send_card(o, req.get("chat_id"), req.get("card") or {}, metadata=req.get("metadata")))
    kw = rec.last("create") or {}
    body = kw.get("body")
    return {"body": body, "thread_id": (body.get("thread") or {}).get("name")}


def attachment_body(req):
    o, rec = fake_adapter()
    rec.upload_ref = req.get("attachment_ref")

    async def acquire(email):
        return ChatApi(rec), "a@b.c"

    async def consume(chat_id, text):
        return None

    o._acquire_user_chat_api = acquire
    o._consume_typing_card_with_text = consume
    o.warning_text = lambda text, caption="": text
    orig_os, orig_media = live.os, live.MediaFileUpload
    live.os = SimpleNamespace(path=SimpleNamespace(exists=lambda p: True, basename=os.path.basename))
    live.MediaFileUpload = lambda *a, **k: None
    try:
        asyncio.run(A._send_file(o, req.get("chat_id"), "/tmp/upload.bin", req.get("caption"), None,
                                 thread_id=req.get("thread_id")))
    finally:
        live.os, live.MediaFileUpload = orig_os, orig_media
    kw = rec.last("create") or {}
    return {"body": kw.get("body"), "create_kwargs": kw}


def media_filename(req):
    name = req.get("name") or ""
    filename = name.split("/")[-1] if name else "attachment"
    ext = "." + filename.rsplit(".", 1)[-1].lower() if "." in filename else ""
    return {"filename": filename, "ext": ext}


def upload_basename(req):
    override = req.get("override") or None
    return override or os.path.basename(req.get("path") or "") or "upload.bin"


def note_rate_limit(req):
    o, _ = fake_adapter(_rate_limit_hits={"spaces/A": req.get("hits") or 0} if req.get("hits") else {})
    count = o._note_rate_limit("spaces/A")
    return {"count": count, "warn": count >= live._RATE_LIMIT_WARN_THRESHOLD}


SENTINEL = "<consumed>"


def _state_chat(state):
    if state == "card":
        return {"spaces/A": "spaces/A/messages/5"}
    if state == "sentinel":
        return {"spaces/A": SENTINEL}
    return {}


def typing_send_action(req):
    state = req.get("state")
    o, _ = fake_adapter(_typing_messages=_state_chat(state))
    if state == "inflight":
        ev = asyncio.Event()
        ev.set()
        o._typing_card_inflight = {"spaces/A": ev}
    created = []

    async def create(chat_id, body):
        created.append(body)
        return live.SendResult(success=True, message_id="spaces/A/messages/7")

    o._create_message = create
    asyncio.run(A.send_typing(o, "spaces/A"))
    if created:
        return "create"
    return "wait" if state == "inflight" else "already"


class PopDict(dict):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.popped = []

    def pop(self, k=None, *d):
        self.popped.append(k)
        return super().pop(k, *d)


def typing_stop_action(req):
    state = req.get("state")
    o, _ = fake_adapter(_typing_messages=PopDict(_state_chat(state)))
    asyncio.run(A.stop_typing(o, "spaces/A"))
    return "pop" if o._typing_messages.popped else "noop"


def typing_consume_action(req):
    state = req.get("state")
    err = req.get("error_status")
    messages = _state_chat(state)
    if state == "inflight":
        messages = {"spaces/A": "spaces/A/messages/6"}
    o, _ = fake_adapter(_typing_messages=messages)

    async def patch(name, body):
        if err is not None:
            raise HttpErr(err)
        return live.SendResult(success=True, message_id=name)

    o._patch_message = patch
    try:
        res = asyncio.run(A._consume_typing_card_with_text(o, "spaces/A", "text"))
    except Exception:  # noqa: BLE001 - HttpError is Exception in this env
        return "raise"
    return "none" if res is None else "patched"


def chat_info_shape(req):
    o, rec = fake_adapter()
    if req.get("info") is None:
        rec.get_error = HttpErr(404)
    else:
        rec.get_resp = req.get("info")
    return asyncio.run(A.get_chat_info(o, req.get("chat_id")))


def expected(r):
    f = r.get("func")
    if f == "chunk_text":
        return chunk_text(r)
    if f == "resolve_thread_id":
        return resolve_thread_id(r)
    if f == "patch_shape":
        return patch_shape(r)
    if f == "send_card_shape":
        return send_card_shape(r)
    if f == "attachment_body":
        return attachment_body(r)
    if f == "media_filename":
        return media_filename(r)
    if f == "upload_basename":
        return upload_basename(r)
    if f == "note_rate_limit":
        return note_rate_limit(r)
    if f == "typing_send_action":
        return typing_send_action(r)
    if f == "typing_stop_action":
        return typing_stop_action(r)
    if f == "typing_consume_action":
        return typing_consume_action(r)
    if f == "chat_info_shape":
        return chat_info_shape(r)
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
        if not same(py, r.get("out")):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, r.get("out")), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
