#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_ids.py
(gate w24_telegramid_756e5b).

The live module is imported and called directly -- it has no I/O, no adapter and
no credentials, so every function under test is the upstream function itself:
``normalize_telegram_chat_id``, ``looks_like_telegram_username``,
``parse_telegram_username_target``, ``parse_send_target`` and
``parse_telegram_message_id``.  Only ``parse_send_target``'s argument type is a
seam (the send_message contract passes ``str``), so non-string refs are skipped.
"""
import json
import os
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_telegramid_756e5b"

# The MSYS login shell that feeds this oracle drops the Windows home env, so
# Path.home() would raise while the telegram package imports.
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.telegram import telegram_ids as live  # noqa: E402

WANTED = {
    "normalize_chat_id", "looks_like_username", "parse_username_target",
    "parse_send_target", "parse_message_id", "constants",
}


def as_json(value):
    """Tuples -> lists, so a Python return value compares to the C JSON."""
    if isinstance(value, tuple):
        return [as_json(v) for v in value]
    if isinstance(value, list):
        return [as_json(v) for v in value]
    return value


def bounds(pattern):
    match = re.search(r"\{(\d+),(\d+)\}", pattern)
    return (int(match.group(1)), int(match.group(2))) if match else (None, None)


def constants():
    low, high = bounds(live._TELEGRAM_USERNAME_RE.pattern)
    return {
        "username_min": low,
        "username_max": high,
        "username_pattern": live._TELEGRAM_USERNAME_RE.pattern,
        "send_target_pattern": live._SEND_TARGET_RE.pattern,
        "named_dm_topic_pattern": live._NAMED_DM_TOPIC_TARGET_RE.pattern,
    }


def expected(r):
    f = r.get("func")
    if f == "normalize_chat_id":
        return as_json(live.normalize_telegram_chat_id(r.get("v")))
    if f == "looks_like_username":
        return bool(live.looks_like_telegram_username(r.get("v")))
    if f == "parse_username_target":
        return as_json(live.parse_telegram_username_target(r.get("v")))
    if f == "parse_send_target":
        ref = r.get("ref")
        if not isinstance(ref, str):
            raise AssertionError("fixture ref must be str (contract: target_ref)")
        return as_json(live.parse_send_target(ref))
    if f == "parse_message_id":
        return as_json(live.parse_telegram_message_id(r.get("v")))
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
