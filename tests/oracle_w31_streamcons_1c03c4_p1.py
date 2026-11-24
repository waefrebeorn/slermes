#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_transport.py (w31_streamcons_1c03c4_p1).

The live ``StreamTransportMixin`` is imported straight out of the quarry and its
pure preview/framing methods are called on an instance made with ``__new__``: no
adapter, no run loop, no socket.  That is the seam -- the methods that talk to
the adapter (``send_stream_frame``, ``edit_message``, ``send_draft``,
``delete_message``) never run, and what is compared is exactly what the
value-level bookkeeping decides: which preview ids a fresh final replaces,
whether an editable preview is on screen, which id an edit retargets to, and the
two resets (native close, fallback mode).

Set-shaped results are returned as sorted lists of ``str`` on both sides: the
upstream values are Python sets that may hold int ids, and the port normalises
ids the same way (``str()``) before sorting.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_streamcons_1c03c4_p1"

# The quarry resolves its data dir from the OS home at import time; a bare
# ``bash -lc`` environment has none, so pin it to the repo's own parent.
if not os.environ.get("HOME") and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
    os.environ["HOME"] = str(QUARRY.parent)
os.environ.setdefault("LOCALAPPDATA", str(QUARRY.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.stream_consumer_transport as live  # noqa: E402

MIXIN = live.StreamTransportMixin

WANTED = {
    "constants", "draft_metadata", "stale_preview_ids", "has_real_preview",
    "adopt_message_id", "close_native_state", "enter_fallback_mode", "track_preview_id",
}


def bare():
    """An instance with no adapter and no run loop: only pure bookkeeping runs."""
    return MIXIN.__new__(MIXIN)


def idset(values):
    return sorted({str(v) for v in values})


def constants(_req):
    return {
        "max_edit_backoff_secs": float(live._MAX_EDIT_BACKOFF_SECS),
        "min_new_msg_chars": float(MIXIN._MIN_NEW_MSG_CHARS),
    }


def draft_metadata(req):
    b = bare()
    b.metadata = req.get("metadata")
    b._initial_reply_to_id = req.get("reply_to")
    return b._draft_metadata()


def stale_preview_ids(req):
    b = bare()
    b._preview_message_ids = set(req.get("preview_ids") or [])
    b._segment_preview_message_ids = set(req.get("segment_preview_ids") or [])
    b._message_id = req.get("message_id")
    return idset(b._stale_preview_ids(segment_only=bool(req.get("segment_only"))))


def has_real_preview(req):
    b = bare()
    b._message_id = req.get("message_id")
    return bool(b._has_real_preview())


def adopt_message_id(req):
    b = bare()
    b._message_id = None
    b._message_created_ts = None
    b._adopt_message_id(req.get("message_id"))
    return {
        "message_id": None if b._message_id is None else str(b._message_id),
        "created": b._message_created_ts is not None,
    }


def close_native_state(req):
    b = bare()
    b._native_stream_opened = True
    b._native_last_pushed_len = 42
    b._already_sent = bool(req.get("already_sent"))
    b._close_native_state()
    return {
        "native_stream_opened": bool(b._native_stream_opened),
        "native_last_pushed_len": int(b._native_last_pushed_len),
        "already_sent": bool(b._already_sent),
    }


def enter_fallback_mode(req):
    b = bare()
    b._enter_fallback_mode(req.get("prefix"))
    return {
        "fallback_prefix": b._fallback_prefix,
        "fallback_final_send": bool(b._fallback_final_send),
        "edit_supported": bool(b._edit_supported),
        "already_sent": bool(b._already_sent),
    }


def track_preview_id(req):
    b = bare()
    b._preview_message_ids = set(req.get("preview_ids") or [])
    b._segment_preview_message_ids = set(req.get("segment_preview_ids") or [])
    b._track_preview_id(req.get("message_id"))
    return {
        "preview_ids": idset(b._preview_message_ids),
        "segment_preview_ids": idset(b._segment_preview_message_ids),
    }


def expected(r):
    f = r.get("func")
    fn = {
        "constants": constants,
        "draft_metadata": draft_metadata,
        "stale_preview_ids": stale_preview_ids,
        "has_real_preview": has_real_preview,
        "adopt_message_id": adopt_message_id,
        "close_native_state": close_native_state,
        "enter_fallback_mode": enter_fallback_mode,
        "track_preview_id": track_preview_id,
    }.get(f)
    if fn is None:
        raise AssertionError("unknown func %r" % (f,))
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
