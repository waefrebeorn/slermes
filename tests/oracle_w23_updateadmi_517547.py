#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/update_admission.py
(gate w23_updateadmi_517547).

The live module is imported, and the three value-deciding helpers are called
on plain inputs:

  * ``_field_names``          -- the log line, over a stand-in for Update whose
                                 ``to_dict()`` answers the fixture's dict;
  * ``_topic_lane``           -- over a stand-in Message namespace;
  * ``PerChatUpdateProcessor._lane_keys`` -- on an uninitialised processor (no
    event loop, no tails: only ``_adopt`` is consulted), with the adapter's
    topic-adoption probe faked to answer the fixture's value;
  * ``build_update_processor`` -- over a stand-in adapter, so the real
    ``coerce_port`` fallback ladder answers the limit.

python-telegram-bot is not installed here and nothing under test touches a real
PTB object, so ``telegram``/``telegram.ext`` are dependency stubs publishing the
same shapes (Update, Application, the two handler bases).  That is the only
seam: the Application, its tasks and the receipt file never start.
"""
import importlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_updateadmi_517547"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


def _install_telegram_stub():
    telegram = types.ModuleType("telegram")

    class Update:  # noqa: D401 - shape only
        """Stand-in for telegram.Update."""

    telegram.Update = Update

    ext = types.ModuleType("telegram.ext")

    class SimpleUpdateProcessor:
        def __init__(self, max_concurrent_updates: int = 1) -> None:
            self.max_concurrent_updates = max_concurrent_updates

    class Application:
        pass

    class ApplicationHandlerStop(Exception):
        pass

    class ConversationHandler:
        entry_points = ()
        fallbacks = ()
        states = {}

    ext.SimpleUpdateProcessor = SimpleUpdateProcessor
    ext.Application = Application
    ext.ApplicationHandlerStop = ApplicationHandlerStop
    ext.ConversationHandler = ConversationHandler
    telegram.ext = ext
    sys.modules["telegram"] = telegram
    sys.modules["telegram.ext"] = ext


_install_telegram_stub()
live = importlib.import_module("plugins.platforms.telegram.update_admission")

WANTED = {"field_names", "topic_lane", "lane_keys", "update_limit", "defaults"}
_MISSING = object()


def field_names(req):
    data = req.get("data")
    update = types.SimpleNamespace(to_dict=lambda: data if isinstance(data, dict) else {})
    return live._field_names(update)


def as_message(value):
    """A Message stand-in: its attributes are the fixture's keys."""
    if isinstance(value, dict):
        return types.SimpleNamespace(**value)
    return value


def topic_lane(req):
    return live._topic_lane(as_message(req.get("message")))


def lane_keys(req):
    proc = live.PerChatUpdateProcessor.__new__(live.PerChatUpdateProcessor)
    adopted = req.get("adopt")
    proc._adopt = (lambda _message: adopted) if adopted is not None else None
    chat = types.SimpleNamespace(id=req.get("chat_id"))
    return [list(key) for key in proc._lane_keys(chat, as_message(req.get("message")))]


class _Adapter:
    def __init__(self, raw):
        self.name = "telegram"
        self.config = types.SimpleNamespace(extra={})
        if raw is not _MISSING:
            self.config.extra["max_concurrent_updates"] = raw
        self._admit_topic_follow = None


def update_limit(req):
    raw = req["raw"] if "raw" in req else _MISSING
    return live.build_update_processor(_Adapter(raw)).max_concurrent_updates


def defaults(_req):
    return {
        "max_concurrent_updates": live.DEFAULT_MAX_CONCURRENT_UPDATES,
        "seen_cap": live._SEEN_CAP,
        "receipt_ttl_seconds": live.RECEIPT_TTL_SECONDS,
    }


def expected(r):
    f = r.get("func")
    if f == "field_names":
        return field_names(r)
    if f == "topic_lane":
        return topic_lane(r)
    if f == "lane_keys":
        return lane_keys(r)
    if f == "update_limit":
        return update_limit(r)
    if f == "defaults":
        return defaults(r)
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
            print("MISMATCH", r.get("func"), "case=", r.get("case"), "live-error=", repr(exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH", r.get("func"), "case=", r.get("case"),
                  "py=", repr(py), "c=", repr(got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
