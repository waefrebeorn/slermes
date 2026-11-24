#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_transport.py (w31_streamcons_1c03c4_p2).

Same seam as gate p1: the live ``StreamTransportMixin`` is imported out of the
quarry and its cadence/policy methods run on a ``__new__`` instance.  The clock
is the one thing these methods reach for, so ``time.monotonic`` is pinned to the
fixture's ``now`` for the duration of each call; the adapter probe is supplied
by a throwaway ``BasePlatformAdapter`` subclass built per case (so
``isinstance`` sees a real adapter and a raising probe is a real raise), and the
ledger hooks the transport calls (``_ceiling_observe``,
``_ceiling_note_unknown``) are no-ops on the instance.

That leaves only the policy: draft cadence, the retry horizon after a server
cooldown, the fresh-final age threshold, transport resolution, and what a first
send that may have landed is allowed to leave behind.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_streamcons_1c03c4_p2"

# The quarry resolves its data dir from the OS home at import time; a bare
# ``bash -lc`` environment has none, so pin it to the repo's own parent.
if not os.environ.get("HOME") and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
    os.environ["HOME"] = str(QUARRY.parent)
os.environ.setdefault("LOCALAPPDATA", str(QUARRY.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.stream_consumer_transport as live  # noqa: E402
from gateway.platforms.base import BasePlatformAdapter  # noqa: E402

MIXIN = live.StreamTransportMixin

WANTED = {
    "should_push_draft", "defer_draft_retry", "should_send_fresh_final",
    "degrade_native_to_buffered_send", "resolve_draft_streaming",
    "resolve_native_streaming", "track_preview_ids_from_result",
    "adapter_prefers_fresh_final", "hold_ambiguous_first_send",
}


def bare():
    return MIXIN.__new__(MIXIN)


def num(req, key, default=0.0):
    v = req.get(key)
    return default if v is None else float(v)


def monotonic_at(now):
    """Pin the clock the transport reads to the fixture's `now`."""
    return mock.patch.object(live.time, "monotonic", lambda: now)


def probe_fn(spec, value):
    """A probe answering per the fixture spec: bool, a raise, or a raw value."""
    def probe(*_args, **_kwargs):
        if spec is True:
            return True
        if spec is False:
            return False
        if spec == "raise":
            raise RuntimeError("probe exploded")
        if spec == "truthy":
            return value
        if spec == "false":
            return False
        return True
    return probe


def fake_adapter(spec, value, accepts_chat_id=True, native_class_flag=False,
                 native_probe=None, has_native_probe=True):
    """A real BasePlatformAdapter subclass: isinstance passes, probes are ours."""
    ns = {
        "SUPPORTS_NATIVE_STREAMING": bool(native_class_flag),
        "__init__": lambda self: None,
        "send": lambda self, *a, **k: None,
        "connect": lambda self, *a, **k: None,
        "disconnect": lambda self, *a, **k: None,
        "get_chat_info": lambda self, *a, **k: {},
    }
    if accepts_chat_id:
        ns["supports_draft_streaming"] = (
            lambda self, chat_id=None, chat_type=None, metadata=None: probe_fn(spec, value)())
    else:
        ns["supports_draft_streaming"] = (
            lambda self, chat_type=None, metadata=None: probe_fn(spec, value)())
    if has_native_probe:
        ns["supports_native_streaming"] = (
            lambda self, chat_type=None, metadata=None: (native_probe or probe_fn(spec, value))())
    cls = type("FakeAdapter", (BasePlatformAdapter,), ns)
    return cls()


def should_push_draft(req):
    b = bare()
    b._accumulated = req.get("accumulated")
    b._draft_retry_until = num(req, "draft_retry_until")
    b._last_edit_time = num(req, "last_edit_time")
    b._current_edit_interval = num(req, "current_edit_interval")
    with monotonic_at(num(req, "now")):
        return bool(b._should_push_draft())


def defer_draft_retry(req):
    b = bare()
    b._draft_retry_until = num(req, "draft_retry_until")
    result = SimpleNamespace(retry_after=req.get("retry_after"))
    with monotonic_at(num(req, "now")):
        deferred = b._defer_draft_retry(result)
    return {"deferred": bool(deferred), "retry_until": b._draft_retry_until}


def should_send_fresh_final(req):
    b = bare()
    b._message_id = req.get("message_id")
    b._message_created_ts = req.get("message_created_ts")
    b.cfg = SimpleNamespace(fresh_final_after_seconds=req.get("fresh_final_after_seconds"))
    with monotonic_at(num(req, "now")):
        return bool(b._should_send_fresh_final())


def degrade_native_to_buffered_send(req):
    b = bare()
    b._use_native_streaming = True
    b._native_stream_opened = True
    b._native_last_pushed_len = 17
    b.cfg = SimpleNamespace(buffer_only=False)
    b._degrade_native_to_buffered_send()
    return {
        "use_native_streaming": bool(b._use_native_streaming),
        "native_stream_opened": bool(b._native_stream_opened),
        "native_last_pushed_len": int(b._native_last_pushed_len),
        "buffer_only": bool(b.cfg.buffer_only),
    }


def resolve_draft_streaming(req):
    b = bare()
    b.chat_id = "chat-1"
    b.metadata = req.get("metadata")
    b.cfg = SimpleNamespace(transport=req.get("transport"), chat_type=req.get("chat_type"))
    if req.get("adapter_is_base"):
        b.adapter = fake_adapter(req.get("probe", "true"), req.get("probe_value"),
                                 accepts_chat_id=req.get("accepts_chat_id", True))
    else:
        b.adapter = SimpleNamespace()
    return bool(b._resolve_draft_streaming())


def resolve_native_streaming(req):
    b = bare()
    b.chat_id = "chat-1"
    b.metadata = req.get("metadata")
    b.cfg = SimpleNamespace(transport="native", chat_type=req.get("chat_type"))
    if req.get("adapter_is_base"):
        b.adapter = fake_adapter(req.get("probe", "true"), req.get("probe_value"),
                                 native_class_flag=req.get("adapter_class_flag"),
                                 has_native_probe=req.get("has_probe", True))
    else:
        b.adapter = SimpleNamespace()
    return bool(b._resolve_native_streaming())


def track_preview_ids_from_result(req):
    b = bare()
    b._preview_message_ids = set()
    b._segment_preview_message_ids = set()
    b._ceiling_observe = lambda *_a, **_k: None
    raw = req.get("result")
    result = SimpleNamespace(**raw) if isinstance(raw, dict) else None
    if result is None:
        return {"preview_ids": []}
    b._track_preview_ids_from_result(result)
    return {"preview_ids": sorted({str(v) for v in b._preview_message_ids})}


def adapter_prefers_fresh_final(req):
    """The hook is a plain function on a plain adapter: no bound-method magic."""
    b = bare()
    b._message_id = req.get("message_id")
    b.metadata = req.get("metadata")
    b.chat_id = "chat-1"
    spec = req.get("hook")
    if spec == "absent":
        b.adapter = SimpleNamespace()
    elif spec == "raise":
        def hook(*_a, **_k):
            raise RuntimeError("hook exploded")
        b.adapter = SimpleNamespace(prefers_fresh_final_streaming=hook)
    elif spec == "truthy":  # truthy but not True: `result is True` must reject it
        b.adapter = SimpleNamespace(
            prefers_fresh_final_streaming=lambda text, metadata=None, chat_id=None: 1)
    elif spec == "false":
        b.adapter = SimpleNamespace(
            prefers_fresh_final_streaming=lambda text, metadata=None, chat_id=None: False)
    elif spec == "meta_only":  # single-platform signature -> TypeError cascade
        b.adapter = SimpleNamespace(
            prefers_fresh_final_streaming=lambda text, metadata=None: True)
    elif spec == "text_only":  # test-double signature -> two TypeErrors
        b.adapter = SimpleNamespace(prefers_fresh_final_streaming=lambda text: True)
    else:
        b.adapter = SimpleNamespace(
            prefers_fresh_final_streaming=lambda text, metadata=None, chat_id=None: True)
    return bool(b._adapter_prefers_fresh_final("hello"))


def hold_ambiguous_first_send(req):
    b = bare()
    b._last_sent_text = ""
    b._edit_supported = True
    b._already_sent = False
    b._delivery_ambiguous = False
    b._preview_uncertain = False
    b._ceiling_note_unknown = lambda *_a, **_k: None
    b.cfg = SimpleNamespace(cursor=req.get("cursor"))
    ok = b._hold_ambiguous_first_send(req.get("text") or "", req.get("raw_text"),
                                      finalize=bool(req.get("finalize")))
    return {
        "ok": bool(ok),
        "last_sent_text": b._last_sent_text,
        "delivery_ambiguous": bool(b._delivery_ambiguous),
        "preview_uncertain": bool(b._preview_uncertain),
        "edit_supported": bool(b._edit_supported),
        "already_sent": bool(b._already_sent),
        "message_id": b._message_id,
        "fallback_prefix": getattr(b, "_fallback_prefix", None),
        "fallback_final_send": getattr(b, "_fallback_final_send", None),
    }


FUNCS = {
    "should_push_draft": should_push_draft,
    "defer_draft_retry": defer_draft_retry,
    "should_send_fresh_final": should_send_fresh_final,
    "degrade_native_to_buffered_send": degrade_native_to_buffered_send,
    "resolve_draft_streaming": resolve_draft_streaming,
    "resolve_native_streaming": resolve_native_streaming,
    "track_preview_ids_from_result": track_preview_ids_from_result,
    "adapter_prefers_fresh_final": adapter_prefers_fresh_final,
    "hold_ambiguous_first_send": hold_ambiguous_first_send,
}


def expected(r):
    fn = FUNCS.get(r.get("func"))
    if fn is None:
        raise AssertionError("unknown func %r" % (r.get("func"),))
    return fn(r)


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
