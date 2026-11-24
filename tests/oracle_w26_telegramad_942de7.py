#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_addressing.py
(gate w26_telegramad_942de7).

The live module is imported and its text layer is called directly.  The ptb
message object is the seam: `leading_bot_handles` / `foreign_command_target` get
a stub carrying exactly `text` + `entities` (what `addressing_sources` reads for
a plain message), and `is_bot_handle` is the fixture's bot list.  Everything
else -- the UTF-16 offset map, the entity handle, the leading-handle run, the
opening command match and the trigger cleanup -- is the shipped code path.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_telegramad_942de7"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from plugins.platforms.telegram import telegram_addressing as live  # noqa: E402

WANTED = {"utf16_offsets", "entity_handle", "leading_handles",
          "leading_bot_handles", "command", "foreign", "clean"}


def _entities(raw):
    return [SimpleNamespace(type=e.get("type", ""), offset=e.get("offset", -1),
                            length=e.get("length", 0))
            for e in (raw or [])]


def _message(text, raw_entities):
    return SimpleNamespace(text=text, entities=_entities(raw_entities))


def utf16_offsets(req):
    return live._utf16_to_str_offsets(req.get("text") or "")


def entity_handle(req):
    return live._entity_handle(req.get("etype") or "", req.get("span") or "")


def leading_handles(req):
    return list(live._leading_handles(req.get("text") or "", _entities(req.get("entities"))))


def leading_bot_handles(req):
    bots = set(req.get("bots") or [])
    found = live.leading_bot_handles(_message(req.get("text") or "", req.get("entities")),
                                     lambda handle: handle in bots)
    return sorted(found)


def command(req):
    match = live._command_after_address(req.get("text") or "", req.get("bot") or "",
                                        has_entities=bool(req.get("has_entities")))
    if match is None:
        return None
    group2 = match.group(2)
    return {"start1": match.start(1),
            "len1": match.end(1) - match.start(1),
            "end": match.end(),
            "group2": group2.lower() if group2 else None}


def foreign(req):
    return live.foreign_command_target(_message(req.get("text") or "", req.get("entities")),
                                       req.get("bot") or "")


def clean(req):
    return live.clean_bot_trigger_text(req.get("text"), req.get("bot") or "")


FUNCS = {"utf16_offsets": utf16_offsets, "entity_handle": entity_handle,
         "leading_handles": leading_handles, "leading_bot_handles": leading_bot_handles,
         "command": command, "foreign": foreign, "clean": clean}


def expected(req):
    try:
        return FUNCS[req["func"]](req)
    except Exception as exc:  # noqa: BLE001
        return {"error": type(exc).__name__}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        py = expected(req)
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
