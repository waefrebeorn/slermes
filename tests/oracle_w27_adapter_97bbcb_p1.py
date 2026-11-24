#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/google_chat/adapter.py (gate
w27_adapter_97bbcb_p1).

Gate 1 of 4: inbound envelope shaping.

The live adapter module is imported by package path (its relative ``.cards``
import needs the package context) and every comparison is made against the real
callables, driven through duck-typed stand-ins so no network/socket/file seam
starts:

  * ``_extract_message_payload`` - the three inbound dialects;
  * ``_prepare_inbound`` - self-filter + dedup + space backfill, with a fake
    ``_dedup`` that only knows the fixture's prior claims;
  * ``_thread_body`` / ``_create_kwargs`` / ``_mime_for_message_type`` - the
    outbound body helpers;
  * ``_handle_pubsub_message`` - the CE-type branch table, observed through a
    capturing logger and a ``_submit_on_loop`` spy;
  * ``_build_message_event`` - the Chat message -> MessageEvent shaping, with
    ``_download_attachment`` resolved from the fixture, ``build_source``
    recording its kwargs and the thread-count store a plain dict.
"""
import asyncio
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_97bbcb_p1"

# The quarry's hermes_constants resolves Path.home() at import time; a login
# bash on this host can arrive without USERPROFILE/HOME, so pin them to the
# user tree the repo lives in before the import.
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.google_chat import adapter as live  # noqa: E402

A = live.GoogleChatAdapter

WANTED = {"extract_message_payload", "prepare_inbound", "thread_body", "create_kwargs",
          "mime_for_message_type", "route_pubsub_event", "build_message_event"}


class Quiet:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass

    def exception(self, *a, **k):
        pass


live.logger = Quiet()


class Log(Quiet):
    def __init__(self):
        self.lines = []

    def debug(self, tpl, *a, **k):
        self.lines.append(("debug", tpl, a))

    def info(self, tpl, *a, **k):
        self.lines.append(("info", tpl, a))

    def warning(self, tpl, *a, **k):
        self.lines.append(("warn", tpl, a))


def same(py, c):
    """Structural equality, floats by tolerance, bool is not int."""
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


class Dedup:
    """MessageDeduplicator limited to the fixture's prior claims."""

    def __init__(self, seen):
        self.seen = set(seen or [])
        self.discarded = []

    def is_duplicate(self, msg_id):
        if not msg_id:
            return False
        return msg_id in self.seen

    def discard(self, msg_id):
        self.discarded.append(msg_id)


class Store:
    """_ThreadCountStore with no disk: incr returns the pre-increment value."""

    def __init__(self, counts):
        self.counts = {k: dict(v) for k, v in (counts or {}).items()}
        self.last = None

    def incr(self, chat_id, thread_name):
        c = self.counts.setdefault(chat_id, {})
        prev = c.get(thread_name, 0)
        c[thread_name] = prev + 1
        self.last = prev
        return prev


class RecDict(dict):
    """dict that records the two write shapes the adapter uses."""

    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.ops = []

    def __setitem__(self, k, v):
        self.ops.append(("set", k, v))
        super().__setitem__(k, v)

    def pop(self, k=None, *d):
        super().pop(k, *d)
        self.ops.append(("del", k))


class FakeEvent:
    def __init__(self, req):
        self._thread_count_store = Store(req.get("counts"))
        self._last_sender_by_chat = {}
        self._last_inbound_thread = RecDict(req.get("last_inbound_thread") or {})
        self._auth = req.get("authorized")
        self._atts = req.get("attachments") or []
        self.source_kwargs = None

    def _is_sender_authorized(self, *a, **k):
        return self._auth

    async def _download_attachment(self, att):
        for cand in self._atts:
            if cand.get("mime") == att.get("contentType"):
                return cand.get("path"), cand.get("mime")
        return None, att.get("contentType") or ""

    def build_source(self, **kw):
        self.source_kwargs = kw
        return {"kwargs": kw}


def extract(req):
    out = A._extract_message_payload(req.get("envelope") or {}, req.get("ce_type") or "")
    if out is None:
        return None
    msg, space, fmt = out
    return {"msg": msg, "space": space, "format": fmt}


def prepare(req):
    env = req.get("envelope")
    env = env if isinstance(env, dict) else {}
    fake = A.__new__(A)
    fake._dedup = Dedup(req.get("dedup_seen"))
    ex = A._extract_message_payload(env, "")
    if ex is None:
        return {"outcome": "drop_unknown"}
    msg, space, _fmt = ex
    if (msg.get("sender") or {}).get("type") == "BOT":
        assert fake._prepare_inbound(env) is None
        return {"outcome": "drop_bot"}
    name = msg.get("name") or ""
    if name and fake._dedup.is_duplicate(name):
        assert fake._prepare_inbound(env) is None
        return {"outcome": "drop_dup"}
    got = fake._prepare_inbound(env)
    if got is None:
        raise AssertionError("live dropped an envelope the oracle expected through")
    m2, e2 = got
    return {"outcome": "ok", "msg": m2, "envelope": e2}


class FakePubSubMsg:
    def __init__(self, envelope, ce_type):
        self.data = json.dumps(envelope or {}).encode("utf-8")
        self.attributes = {"ce-type": ce_type or ""}
        self.acked = False
        self.nacked = False

    def ack(self):
        self.acked = True

    def nack(self):
        self.nacked = True


def route(req):
    log = Log()
    live.logger = log
    try:
        fake = A.__new__(A)
        fake._shutting_down = False
        fake._debug_raw = False
        fake._bot_user_id = req.get("bot_user_id") or None
        fake._dedup = Dedup([])
        submitted = []
        fake._submit_on_loop = lambda coro: (submitted.append(coro), coro.close())
        fake._save_cached_bot_id = lambda bot: None
        msg = FakePubSubMsg(req.get("envelope"), req.get("ce_type"))
        A._handle_pubsub_message(fake, msg)
    finally:
        live.logger = Quiet()

    action, space_name = "message", None
    for kind, tpl, args in log.lines:
        text = tpl % args if args and "%" in tpl else tpl
        if "ADDED_TO_SPACE" in text:
            action = "membership_added"
            space_name = None if (not args or args[0] == "?") else args[0]
        elif "REMOVED_FROM_SPACE" in text:
            action = "membership_removed"
            space_name = None if (not args or args[0] == "?") else args[0]
        elif "Card/widget event ack'd" in text:
            action = "card_ack"
            space_name = None
    return {"action": action, "space_name": space_name, "bot_user_id": fake._bot_user_id}


def build_event(req):
    fake = FakeEvent(req)
    ev = asyncio.run(A._build_message_event(fake, req.get("msg") or {}, req.get("envelope") or {}))
    if ev is None:
        return None
    cache = None
    if fake._last_inbound_thread.ops:
        op = fake._last_inbound_thread.ops[-1]
        cache = {"op": "set", "thread": op[2]} if op[0] == "set" else {"op": "del"}
    last_sender = None
    if fake._last_sender_by_chat:
        (sp, em), = fake._last_sender_by_chat.items()
        last_sender = {"space": sp, "email": em}
    src = fake.source_kwargs or {}
    return {
        "chat_type": src.get("chat_type"),
        "text": ev.text,
        "message_type": ev.message_type.value,
        "session_thread_id": src.get("thread_id"),
        "prev_thread_count": 0 if fake._thread_count_store.last is None else fake._thread_count_store.last,
        "thread_cache": cache,
        "last_sender": last_sender,
        "media_urls": ev.media_urls,
        "media_types": ev.media_types,
        "source": {
            "chat_id": src.get("chat_id"),
            "chat_name": src.get("chat_name"),
            "chat_type": src.get("chat_type"),
            "user_id": src.get("user_id"),
            "user_name": src.get("user_name"),
            "thread_id": src.get("thread_id"),
            "user_id_alt": src.get("user_id_alt"),
            "message_id": src.get("message_id"),
        },
    }


def expected(r):
    f = r.get("func")
    if f == "extract_message_payload":
        return extract(r)
    if f == "prepare_inbound":
        return prepare(r)
    if f == "thread_body":
        return live._thread_body(r.get("text") or "", r.get("thread_id"))
    if f == "create_kwargs":
        return live._create_kwargs(r.get("chat_id"), r.get("body") or {})
    if f == "mime_for_message_type":
        return live._mime_for_message_type(r.get("mime") or "").value
    if f == "route_pubsub_event":
        return route(r)
    if f == "build_message_event":
        return build_event(r)
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
