#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p8 (mrkdwn conversion, block merge, unfurls, constructors)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p8"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

AD = live.SlackAdapter
WANTED = {
    "format_message", "append_block_text", "normalize_changed_message",
    "append_link_unfurls", "media_message_type", "session_thread_ts", "button",
    "interaction_fields", "section_text", "thread_cache_key", "thread_root_message",
    "render_message_text",
}


def bare():
    return AD.__new__(AD)


def expected(r):
    f = r.get("func")
    if f == "format_message":
        return bare().format_message(r.get("content", ""))
    if f == "append_block_text":
        return AD._append_block_text(r.get("text", ""), r.get("blocks"), r.get("bot_uid", ""))
    if f == "normalize_changed_message":
        inst = bare()
        inst._processed_message_ts = {ts: 1.0 for ts in (r.get("processed") or [])}
        return inst._normalize_changed_message(r.get("event") or {})
    if f == "append_link_unfurls":
        return AD._append_link_unfurls(r.get("text", ""), r.get("attachments") or [])
    if f == "media_message_type":
        return AD._media_message_type(r.get("media_types") or []).name
    if f == "session_thread_ts":
        inst = bare()
        inst.config = SimpleNamespace(extra={"reply_in_thread": r.get("reply_in_thread", True),
                                             "dm_top_level_threads_as_sessions": r.get(
                                                 "dm_top_level", True)})
        return inst._session_thread_ts(r.get("event") or {}, r.get("ts", ""),
                                       bool(r.get("is_dm")), r.get("assistant_meta") or {})
    if f == "button":
        return AD._button(r.get("text", ""), r.get("action_id", ""), r.get("value", ""),
                          style=r.get("style", ""), emoji=bool(r.get("emoji")))
    if f == "interaction_fields":
        return list(AD._interaction_fields(r.get("body") or {}, r.get("action") or {}))
    if f == "section_text":
        msg = r.get("message") or {}
        if "limit" in r:
            return AD._section_text(msg, r["limit"])
        return AD._section_text(msg)
    if f == "thread_cache_key":
        return AD._thread_cache_key(r.get("channel_id", ""), r.get("thread_ts", ""),
                                    r.get("team_id", ""))
    if f == "thread_root_message":
        return AD._thread_root_message(r.get("messages") or [], r.get("thread_ts", ""))
    if f == "render_message_text":
        return AD._render_message_text(r.get("msg") or {}, r.get("bot_uid", ""),
                                       r.get("own_ids") or ())
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
