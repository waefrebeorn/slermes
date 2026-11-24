#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_batching.py
(gate w26_telegramba_f62536).

The live module is imported and its pure merge decisions are called on stand-in
events: a SimpleNamespace carries ``media_text_inlined`` / ``media_urls`` and the
private ``_telegram_item_notes`` attribute, which is exactly what the merge
reads.  No adapter, no batch dict, no debounce task, no download happens -- and
``merge_caption`` is the fixed join the C port implements.
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
GATE = "w26_telegramba_f62536"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import telegram_batching as live  # noqa: E402
from plugins.platforms.telegram import telegram_media_group as media  # noqa: E402

WANTED = {"pad_inline_flags", "command_first", "merged_text", "caption_is_command",
          "constants"}

# The one caption join: (a + "\n\n" + b) if a else b.
MERGE = lambda a, b: (a + "\n\n" + b) if a else b  # noqa: E731


def pad_inline_flags(req):
    event = SimpleNamespace(media_text_inlined=req.get("media_text_inlined"),
                            media_urls=req.get("media_urls"))
    return live.pad_inline_flags(event)


def command_first(req):
    return live.command_first(req.get("captions") or "", req.get("incoming"), MERGE)


def merged_text(req):
    event = SimpleNamespace()
    notes = [n for n in (req.get("notes") or [])]
    if notes:
        setattr(event, media.ITEM_NOTES_ATTR, notes)
    return live.merged_text(req.get("text"), event, req.get("incoming"), MERGE)


def caption_is_command(req):
    return live.caption_is_command(req.get("caption"))


def constants():
    return {"caption_text_attr": media.CAPTION_TEXT_ATTR,
            "item_notes_attr": media.ITEM_NOTES_ATTR}


def expected(r):
    f = r.get("func")
    if f == "pad_inline_flags":
        return pad_inline_flags(r)
    if f == "command_first":
        return command_first(r)
    if f == "merged_text":
        return merged_text(r)
    if f == "caption_is_command":
        return caption_is_command(r)
    if f == "constants":
        return constants()
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
