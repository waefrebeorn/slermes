#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_dm_topic_send.py
(gate w24_telegramdm_1f8bc2).

The live module is imported and its four pure functions are called on the JSON
values the harness read.  The Bot API call is the seam: nothing here sends a
message, touches the routing repository or prunes a binding -- only what the
values decide (private chat? topic gone? reroute? which kwargs survive?) is
compared.
"""
import json
import importlib.util
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_telegramdm_1f8bc2"
PYFILE = QUARRY / "plugins/platforms/telegram/telegram_dm_topic_send.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The module itself imports nothing from the package, so loading its file keeps
# the platform package __init__ (which drags in the whole gateway) out of the
# way; the code compared is byte-for-byte the live file.
_spec = importlib.util.spec_from_file_location("live_telegram_dm_topic_send", PYFILE)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"private_chat", "topic_gone", "dead_topic_may_reroute", "dead_topic_retry_kwargs"}


def private_chat(r):
    return live.private_chat(r.get("chat_id"))


def topic_gone(r):
    return live.topic_gone(r.get("chat_id"), r.get("error"))


def may_reroute(r):
    return live.dead_topic_may_reroute(r.get("chat_id"), r.get("metadata"))


def retry_kwargs(r):
    return live.dead_topic_retry_kwargs(r.get("kwargs") or {})


DISPATCH = {
    "private_chat": private_chat,
    "topic_gone": topic_gone,
    "dead_topic_may_reroute": may_reroute,
    "dead_topic_retry_kwargs": retry_kwargs,
}


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
            py = DISPATCH[r["func"]](r)
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
