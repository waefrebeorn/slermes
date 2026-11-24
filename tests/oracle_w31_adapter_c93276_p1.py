#!/usr/bin/env python3
"""Differential oracle for the INBOUND half of plugins/platforms/simplex/adapter.py
(gate w31_adapter_c93276_p1).

The live adapter module is imported BY FILE PATH: its package __init__ pulls the
gateway stack (fastapi/pydantic), and this host's application-control policy
refuses pydantic's compiled extension.  The seam stubs cover that import chain.

Four things the module touches are seams and are injected per case:

  env/authz  -- ``_is_sender_authorized`` is rebound to membership of the case's
                ``authorized_ids``; that is the only thing the production authz
                answer contributes to ``_may_receive_file``.
  build_source -- rebound to a recorder, so the compared record carries exactly
                the arguments the real ``SessionSource`` would be built from.
  event sink -- ``_enqueue_text_event`` / ``handle_message`` are rebound to
                recorders: "batch" vs "handle" is the decision under test.
  clock      -- the truth substitutes ``datetime.now()`` only when the timestamp
                string is unparseable; the port returns null there and the
                oracle mirrors that (a wall clock is not a value).

Ported symbols and their live entry points:

  parse_comma_list   _parse_comma_list(value)
  redact_id          _redact_id(value)
  path_suffix        Path(path).suffix.lower() as _handle_chat_item uses it
  is_image_ext       _is_image_ext(ext)
  is_audio_ext       _is_audio_ext(ext)
  mime_for_ext       _mime_for_ext(ext)
  display_name       _display_name(obj, profile_key, default)
  may_receive_file   SimplexAdapter._may_receive_file(case)
  ts_iso             datetime.fromisoformat(ts).isoformat()
  chat_item          SimplexAdapter._handle_chat_item(case["item"])
"""
import asyncio
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_adapter_c93276_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w31-simplex-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

_SPEC = importlib.util.spec_from_file_location(
    "w31_simplex_adapter_live", QUARRY / "plugins" / "platforms" / "simplex" / "adapter.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

WANTED = {
    "parse_comma_list", "redact_id", "path_suffix", "is_image_ext", "is_audio_ext",
    "mime_for_ext", "display_name", "may_receive_file", "ts_iso", "chat_item",
}

ADAPTER = live.SimplexAdapter


class Recorder:
    def __init__(self, authorized):
        self.authorized = set(authorized or [])
        self.sent = []
        self.batched = []
        self.handled = []
        self.source = None


def bare(case):
    """An adapter with every seam replaced by the case's values."""
    rec = Recorder(case.get("authorized_ids"))
    adapter = ADAPTER.__new__(ADAPTER)
    adapter.group_allow_from = set(case.get("group_allow_from") or [])
    adapter._pending_file_transfers = {}
    adapter._is_sender_authorized = lambda sid, _kind, _chat: sid in rec.authorized

    def build_source(**kw):
        rec.source = kw
        return dict(kw)

    async def fire_and_forget(cmd):
        rec.sent.append(cmd)

    def enqueue(event):
        rec.batched.append(event)

    async def handle(event):
        rec.handled.append(event)

    adapter.build_source = build_source
    adapter._send_fire_and_forget = fire_and_forget
    adapter._enqueue_text_event = enqueue
    adapter.handle_message = handle
    return adapter, rec


def ts_iso(ts):
    from datetime import datetime
    try:
        return datetime.fromisoformat(ts).isoformat()
    except (ValueError, TypeError):
        return None


def chat_item(case):
    adapter, rec = bare(case)
    asyncio.run(adapter._handle_chat_item(case.get("item") or {}))
    out = {
        "action": None, "text": None, "message_type": None,
        "media_urls": None, "media_types": None, "chat_id": None, "chat_name": None,
        "chat_type": None, "user_id": None, "user_name": None, "file_id": None,
        "command": None, "ts": None,
    }
    if rec.sent:
        out["action"] = "defer"
        out["command"] = rec.sent[0]
        pending = case.get("item") or {}
        fid = (((pending.get("chatItem") or {}).get("file")) or {}).get("fileId")
        out["file_id"] = fid
        return out
    events = rec.batched + rec.handled
    if not events:
        out["action"] = "drop"
        return out
    event = events[0]
    src = rec.source or {}
    out.update({
        "action": "batch" if rec.batched else "handle",
        "text": event.text,
        "message_type": event.message_type.value,
        "media_urls": list(event.media_urls),
        "media_types": list(event.media_types),
        "chat_id": src.get("chat_id"),
        "chat_name": src.get("chat_name"),
        "chat_type": src.get("chat_type"),
        "user_id": src.get("user_id"),
        "user_name": src.get("user_name"),
    })
    # The truth only substitutes "now" for an unparseable ts; the port returns
    # null there, so the compared value is the parse, not the wall clock.
    meta = (((case.get("item") or {}).get("chatItem") or {}).get("meta") or {})
    ts_str = meta.get("itemTs") or meta.get("createdAt", "")
    out["ts"] = ts_iso(ts_str) if ts_str else None
    return out


def expected(r):
    f = r.get("func")
    if f == "parse_comma_list":
        return live._parse_comma_list(r.get("value") or "")
    if f == "redact_id":
        return live._redact_id(r.get("value"))
    if f == "path_suffix":
        return Path(r.get("path") or "").suffix.lower()
    if f == "is_image_ext":
        return live._is_image_ext(r.get("ext") or "")
    if f == "is_audio_ext":
        return live._is_audio_ext(r.get("ext") or "")
    if f == "mime_for_ext":
        return live._mime_for_ext(r.get("ext") or "")
    if f == "display_name":
        if "default" in r:
            return live._display_name(r.get("obj") or {}, r.get("profile_key") or "profile",
                                      r.get("default"))
        return live._display_name(r.get("obj") or {}, r.get("profile_key") or "profile")
    if f == "may_receive_file":
        adapter, _rec = bare(r)
        return adapter._may_receive_file(r.get("item") or {})
    if f == "ts_iso":
        return ts_iso(r.get("ts") or "")
    if f == "chat_item":
        return chat_item(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
