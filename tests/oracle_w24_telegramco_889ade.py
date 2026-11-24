#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_context.py
(gate w24_telegramco_889ade).

The live module functions are called for real, on a REAL ``TelegramAdapter``
instance made with ``__new__`` (no PTB bot, no network, no event loop):

  * ``mentions_other_participants`` / ``starts_with_other_participant_mention``
  * ``_quoted_reply`` / ``group_reply_expected`` / ``group_trigger_text``
  * ``group_identity_prompt``

So the entity walking, ``_entity_span`` (Telegram's UTF-16 offsets),
``_is_reply_to_bot``, ``_message_mentions_bot``, ``_message_matches_mention_patterns``,
``_is_group_chat`` and ``clean_bot_trigger_text`` are the upstream implementations.

Seam honesty: the C port receives three per-message adapter verdicts as booleans
(``reply_to_bot``, ``mentions_bot``, ``matches_patterns``).  For every
``group_reply_expected`` case this oracle recomputes them with the live adapter
and fails the case if the fixture's declaration drifted, so a fixture can never
silently take a different branch in C than in Python.
"""
import json
import os
import re
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_telegramco_889ade"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram.adapter import TelegramAdapter  # noqa: E402
from plugins.platforms.telegram import telegram_context as live  # noqa: E402

WANTED = {
    "mentions_other", "starts_with_other_mention", "quoted_reply",
    "group_reply_expected", "group_trigger_text", "group_identity_prompt", "constants",
}


class Ent:
    def __init__(self, spec):
        self.type = spec.get("type")
        self.offset = spec.get("offset")
        self.length = spec.get("length")
        uid = spec.get("user_id")
        self.user = SimpleNamespace(id=uid) if uid is not None else None


class Reply:
    def __init__(self, spec):
        self.message_id = spec.get("message_id")
        uid = spec.get("from_user_id")
        self.from_user = SimpleNamespace(id=uid) if uid is not None else None
        self.sender_chat = SimpleNamespace(id=1) if spec.get("sender_chat") else None
        self.forum_topic_created = SimpleNamespace(id=1) if spec.get("forum_topic_created") else None


class Msg:
    def __init__(self, spec):
        self.text = spec.get("text")
        self.caption = spec.get("caption")
        self.entities = [Ent(e) for e in spec.get("entities") or []]
        self.caption_entities = [Ent(e) for e in spec.get("caption_entities") or []]
        self.reply_to_message = Reply(spec["reply"]) if spec.get("reply") else None
        self.is_topic_message = bool(spec.get("is_topic_message"))
        self.message_thread_id = spec.get("message_thread_id")
        self.chat = SimpleNamespace(type=spec.get("chat_type") or "private")
        self.voice = bool(spec.get("voice"))
        self.video_note = bool(spec.get("video_note"))
        self.audio = bool(spec.get("audio"))
        self.rich_message = None
        # declared adapter verdicts (verified against the live adapter below)
        self.reply_to_bot = bool(spec.get("reply_to_bot"))
        self.mentions_bot = bool(spec.get("mentions_bot"))
        self.matches_patterns = bool(spec.get("matches_patterns"))


def make_adapter(spec, msg=None):
    a = TelegramAdapter.__new__(TelegramAdapter)
    if spec.get("bot"):
        a._bot = SimpleNamespace(id=spec.get("bot_id"), username=spec.get("username"))
    else:
        a._bot = None
    a._bot_username_observed = spec.get("username") or None
    a._mention_patterns = [re.compile(r"(?s).")] if msg is not None and msg.matches_patterns else []
    return a


def verify_seams(a, m):
    drift = []
    if bool(m.reply_to_bot) != a._is_reply_to_bot(m):
        drift.append("reply_to_bot=%r live=%r" % (m.reply_to_bot, a._is_reply_to_bot(m)))
    if bool(m.mentions_bot) != a._message_mentions_bot(m):
        drift.append("mentions_bot=%r live=%r" % (m.mentions_bot, a._message_mentions_bot(m)))
    if bool(m.matches_patterns) != a._message_matches_mention_patterns(m):
        drift.append("matches_patterns=%r live=%r"
                     % (m.matches_patterns, a._message_matches_mention_patterns(m)))
    if drift:
        raise AssertionError("seam flag drift: " + "; ".join(drift))


def expected(r):
    f = r.get("func")
    spec = r.get("msg") or {}
    m = Msg(spec)
    a = make_adapter(r.get("adapter") or {}, m)

    if f == "mentions_other":
        return bool(live.mentions_other_participants(a, m))
    if f == "starts_with_other_mention":
        return bool(live.starts_with_other_participant_mention(a, m))
    if f == "quoted_reply":
        replied = live._quoted_reply(m)
        return None if replied is None else replied.message_id
    if f == "group_reply_expected":
        verify_seams(a, m)
        return live.group_reply_expected(a, m, r.get("chat_type"))
    if f == "group_trigger_text":
        return live.group_trigger_text(a, m, r.get("text"))
    if f == "group_identity_prompt":
        return live.group_identity_prompt(a, m, r.get("channel_prompt"))
    if f == "constants":
        probe = make_adapter({"bot": True, "bot_id": 1, "username": "PROBE"},
                             Msg({"chat_type": "supergroup"}))
        sentence = live.group_identity_prompt(probe, Msg({"chat_type": "supergroup"}), None)
        return {
            "identity_template": sentence.replace("@PROBE", "@{username}"),
            "reply_verdicts": [True, False, None],
        }
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
