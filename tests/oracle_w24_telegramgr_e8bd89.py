#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_group_observe.py
(gate w24_telegramgr_e8bd89).

The live module is imported from the Python quarry and its two public helpers are
called directly.  ``apply_group_observe_attribution`` is called with a real
``MessageEvent`` and a stand-in adapter whose six seam methods answer from the
request object -- the adapter, the raw Telegram message and the shared-source
builder are the seams; what is compared is the decision they carry:

  * ``apply_group_observe_attribution`` -> {applied, shared_session, text,
    channel_prompt, source_user_id} as left on the returned event;
  * ``observed_media_note`` over a cached media handle;
  * ``shared_sender_suffix`` over an actor-bearing event;
  * the ``_OBSERVED_MEDIA_TOOL`` literal table.
"""
import dataclasses
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_telegramgr_e8bd89"

# Import-time code in the live package resolves the user's home; a bare MSYS
# shell exports HOME but no USERPROFILE, which ``Path.home()`` refuses. Fill it
# in from the checkout's parent so the import is not a function of the shell.
if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.platforms.event import MessageEvent, MessageType  # noqa: E402
from gateway.session import Platform, SessionSource  # noqa: E402
from plugins.platforms.telegram import telegram_group_observe as live  # noqa: E402

WANTED = {"attribution_plan", "observed_media_note", "shared_sender_suffix", "constants"}


class FakeAdapter:
    """The six adapter seam methods, answered from the request object."""

    def __init__(self, req):
        self.req = req

    def _telegram_observe_unmentioned_group_messages(self):
        return bool(self.req.get("observe_enabled"))

    def _is_group_chat(self, raw_message):
        return bool(self.req.get("is_group"))

    def _telegram_observe_allowed_chats(self):
        allowed = self.req.get("allowed")
        return set(allowed) if isinstance(allowed, list) else allowed

    def _chat_id_str(self, raw_message):
        return self.req.get("chat_id")

    def _telegram_group_observe_channel_prompt(self):
        return self.req.get("observe_prompt")

    def _telegram_group_observe_attributed_text(self, event):
        return self.req.get("attributed_text")

    def _telegram_group_observe_shared_source(self, source):
        return dataclasses.replace(
            source,
            user_id=self.req.get("shared_source_user_id"),
            shared_session=bool(self.req.get("shared_source_shared_session")))


def attribution_plan(req):
    source = SessionSource(
        platform=Platform.TELEGRAM,
        chat_id=req.get("chat_id") or "",
        user_id=req.get("source_user_id"),
        shared_session=bool(req.get("source_shared_session")))
    event = MessageEvent(
        text=req.get("text"),
        source=source,
        user_id=req.get("user_id"),
        raw_message={"chat": {"type": "group"}} if req.get("has_raw") else None,
        message_type=(MessageType.COMMAND if req.get("message_type") == "command"
                      else MessageType.TEXT),
        channel_prompt=req.get("channel_prompt"))
    out = live.apply_group_observe_attribution(FakeAdapter(req), event)
    return {"applied": out is not event,
            "shared_session": bool(out.source.shared_session),
            "text": out.text,
            "channel_prompt": out.channel_prompt,
            "source_user_id": out.source.user_id}


def observed_media_note(req):
    note = req.get("context_note")
    cached = SimpleNamespace(kind=req.get("kind"), context_note=lambda: note)
    return live.observed_media_note(cached)


def shared_sender_suffix(req):
    event = SimpleNamespace(user_id=req.get("user_id"),
                            source=SimpleNamespace(user_id=req.get("source_user_id")))
    return live.shared_sender_suffix(event)


def constants(_req):
    return dict(live._OBSERVED_MEDIA_TOOL)


def expected(r):
    f = r.get("func")
    if f == "attribution_plan":
        return attribution_plan(r)
    if f == "observed_media_note":
        return observed_media_note(r)
    if f == "shared_sender_suffix":
        return shared_sender_suffix(r)
    if f == "constants":
        return constants(r)
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
