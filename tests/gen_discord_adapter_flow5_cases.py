#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow5.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}}}.
io keys are a subset of the C/oracle seam registry. At least 6 cases per
ported function. `fc: true` marks a deliberate fail-closed case.
"""
import json
import os
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow5.jsonl"
OUT.parent.mkdir(parents=True, exist_ok=True)

cases = []


def add(func, args=None, io=None, state=None, fc=False, tag=""):
    rec = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if state is not None:
        rec["in"]["state"] = state
    if fc:
        rec["fc"] = True
    if tag:
        rec["tag"] = tag
    cases.append(rec)


raise_v = lambda t, m: {"__script__": True, "raise": {"type": t, "message": m}}

# ---- 1 _message_channel_ids (no seams) ----
F = "_message_channel_ids"
add(F, {"message": {"channel": {"id": "5", "parent_id": "9"}}})
add(F, {"message": {"channel": {"id": "5"}}})
add(F, {"message": {"channel": {"id": 5, "parent_id": 9}}})
add(F, {"message": {"channel": {}}})
add(F, {"message": {}})
add(F, {"message": {"channel": {"id": 0, "parent_id": "0"}}})
add(F, {"message": {"channel": {"id": "abc", "parent_id": ""}}})
add(F, {"message": {"channel": {"parent_id": "77"}}})

# ---- 2 _thread_parent_channel (no seams) ----
F = "_thread_parent_channel"
add(F, {"channel": {"id": "1", "parent": {"id": "0"}}})
add(F, {"channel": {"id": "1", "parent": None}})
add(F, {"channel": {"id": "1"}})
add(F, {"channel": {"id": "1", "parent": False}})
add(F, {"channel": {}})
add(F, {"channel": {"id": "1", "parent": {}}})

# ---- 3 _thread_created (no seams) ----
F = "_thread_created"
add(F, {"thread": {"id": 7}, "name": "fallback"})
add(F, {"thread": {"id": "7", "name": "Custom"}, "name": "fallback"})
add(F, {"thread": {"id": 7, "name": ""}, "name": "fallback"})
add(F, {"thread": {"id": 7, "name": None}, "name": "fallback"})
add(F, {"thread": {"id": ""}, "name": "fallback"})
add(F, {"thread": {"id": 0}, "name": "zero"})

# ---- 4 _get_parent_channel_id (no seams) ----
F = "_get_parent_channel_id"
add(F, {"channel": {"parent": {"id": 3}}})
add(F, {"channel": {"parent": {"id": None}, "parent_id": "8"}})
add(F, {"channel": {"parent": None, "parent_id": "8"}})
add(F, {"channel": {"parent_id": None}})
add(F, {"channel": {}})
add(F, {"channel": {"parent": {}}})
add(F, {"channel": {"parent_id": 0}})

# ---- 5 _interaction_guild_id (no seams) ----
F = "_interaction_guild_id"
add(F, {"interaction": {"guild_id": 42}})
add(F, {"interaction": {"guild_id": None, "channel": {"guild": {"id": 9}}}})
add(F, {"interaction": {"channel": {"guild": {"id": 9}}}})
add(F, {"interaction": {"guild_id": 0}})
add(F, {"interaction": {}})
add(F, {"interaction": {"guild_id": "g"}})

# ---- 6 _derive_auto_thread_name (re.sub x3, maybe t) ----
F = "_derive_auto_thread_name"
add(F, {"content": "  hello world  "}, {"re.sub": ["hello world", "hello world", "hello world"]})
add(F, {"content": "@bob here"}, {"re.sub": ["bob here", "bob here", "bob here"]})
add(F, {"content": ""}, {"re.sub": ["", "", ""], "t": "Default Thread"})
add(F, {"content": "<@123> ping"}, {"re.sub": [" ping", " ping", "ping"], "t": "D"})
add(F, {"content": "x" * 90}, {"re.sub": ["x" * 90, "x" * 90, "x" * 90]})
add(F, {"content": "multi   space"}, {"re.sub": ["multi   space", "multi   space", "multi space"]})
add(F, {"content": "  hi  "}, {}, fc=True, tag="fail-closed: no re.sub script")
add(F, {"content": "hi"}, {"re.sub": [raise_v("RuntimeError", "boom")]},
    tag="re.sub raise propagates")

# ---- 7 _derive_forum_thread_name (t only when empty) ----
F = "_derive_forum_thread_name"
add(F, {"message": "  # Title here\nsecond line  "})
add(F, {"message": "###"}, {"t": "Forum"})
add(F, {"message": ""}, {"t": "Forum"})
add(F, {"message": "x" * 120})
add(F, {"message": "\nnext line"}, {"t": "Forum"})
add(F, {"message": "  spaced  title  "})
add(F, {"message": "#   "}, {"t": "Forum"})
add(F, {"message": "# OnlyHash"}, {})  # no t needed

# ---- 8 _forum_thread_parts (no seams) ----
F = "_forum_thread_parts"
add(F, {"thread": {"send": {}, "id": "5", "message": {"id": "9"}}})
add(F, {"thread": {"thread": {"id": "6"}, "id": "5"}})
add(F, {"thread": {"id": 7}})
add(F, {"thread": {"send": {"id": "8"}, "message": None}})
add(F, {"thread": {"thread": {}, "id": "3"}})
add(F, {"thread": {}})

# ---- 9 _message_reference_from_ids (discord.MessageReference) ----
F = "_message_reference_from_ids"
add(F, {"message_id": "123", "channel": {"id": 5, "guild": {"id": 9}}},
    {"discord.MessageReference": {"ref": 1}})
add(F, {"message_id": 123, "channel": {}}, {"discord.MessageReference": "REF"})
add(F, {"message_id": "abc", "channel": {}}, {})
add(F, {"message_id": None, "channel": {}}, {})
add(F, {"message_id": " 42 ", "channel": {"id": 7}}, {"discord.MessageReference": "R"})
add(F, {"message_id": "-5", "channel": {}}, {"discord.MessageReference": "R"})
add(F, {"message_id": "5", "channel": {}}, {}, fc=True,
    tag="fail-closed: no discord.MessageReference script")
add(F, {"message_id": [], "channel": {}}, {})

# ---- 10 _reply_reference_for_send (sibling + logger.debug) ----
F = "_reply_reference_for_send"
add(F, {"reply_to": None, "channel": {}}, {})
add(F, {"reply_to": "", "channel": {}}, {})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": {"ref": 1}}, state={"_reply_to_mode": "on"})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {}, state={"_reply_to_mode": "off"})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": raise_v("ValueError", "bad"), "logger.debug": None})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": raise_v("TypeError", "oops"), "logger.debug": None})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": raise_v("RuntimeError", "boom")})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": raise_v("SystemExit", "bye")})
add(F, {"reply_to": "5", "channel": {"id": 1}}, {}, fc=True,
    tag="fail-closed: no sibling script", state={"_reply_to_mode": "on"})
add(F, {"reply_to": "5", "channel": {"id": 1}},
    {"_message_reference_from_ids": raise_v("ValueError", "bad"),
     "logger.debug": raise_v("RuntimeError", "logfail")})

# ---- 11 format_tool_preview (_format_discord_markdown_link) ----
F = "format_tool_preview"
add(F, {"preview": {"url": "", "text": "plain"}}, {})
add(F, {"preview": {"url": None, "text": "plain"}}, {})
add(F, {"preview": {"url": "https://x", "text": "label"}},
    {"_format_discord_markdown_link": "[label](https://x)"})
add(F, {"preview": {"url": "", "text": "no url"}}, {})
add(F, {"preview": {"url": "https://y", "text": "L"}}, {}, fc=True)
add(F, {"preview": {"url": "https://z", "text": "L"}},
    {"_format_discord_markdown_link": raise_v("RuntimeError", "fmt")})

# ---- 12 format_message (convert_table_to_bullets) ----
F = "format_message"
add(F, {"content": ""}, {})
add(F, {"content": None}, {})
add(F, {"content": "plain text"}, {"convert_table_to_bullets": "plain text"})
add(F, {"content": "|a|b|\n|-|-|\n|1|2|"},
    {"convert_table_to_bullets": "- a: 1\n- b: 2"})
add(F, {"content": "hi"}, {}, fc=True)
add(F, {"content": "hi"}, {"convert_table_to_bullets": raise_v("ValueError", "bad")})

# ---- 13 _is_down_notice_content (re.search) ----
F = "_is_down_notice_content"
add(F, {"content": "Hermes is down"}, {"re.search": {"m": 1}})
add(F, {"content": "all good"}, {"re.search": None})
add(F, {"content": "The gateway was unavailable"}, {"re.search": "match"})
add(F, {"content": ""}, {"re.search": None})
add(F, {"content": "BMO appears to be offline"}, {"re.search": {"m": 1}})
add(F, {"content": "hermes is down"}, {}, fc=True)
add(F, {"content": "hermes is down"}, {"re.search": raise_v("RuntimeError", "rx")})

# ---- 14 _is_reply_reference_rejected (no seams) ----
F = "_is_reply_reference_rejected"
add(F, {"err": "error code: 10008"})
add(F, {"err": "HTTP 400: error code: 50035 Cannot reply to a system message"})
add(F, {"err": "error code: 50035 (other validation)"})
add(F, {"err": "Cannot reply to a system message"})
add(F, {"err": ""})
add(F, {"err": "error code: 40004"})

# ---- 15 _is_length_overflow_error (no seams) ----
F = "_is_length_overflow_error"
add(F, {"err": "Error Code: 50035 2000 or fewer characters"})
add(F, {"err": "error code: 50035 fewer in length"})
add(F, {"err": "error code: 50035 bad reply reference"})
add(F, {"err": "error code: 10008"})
add(F, {"err": ""})
add(F, {"err": "2000 or fewer but no code"})

# ---- 16 _first_embed (no seams) ----
F = "_first_embed"
add(F, {"message": {"embeds": [{"title": "A"}, {"title": "B"}]}})
add(F, {"message": {"embeds": []}})
add(F, {"message": {"embeds": None}})
add(F, {"message": {"embeds": None}})
add(F, {"message": {"embeds": [{"description": "only"}]}})
add(F, {"message": {"embeds": []}})

# ---- 17 _config_embed (t, truncate, Color.blue, Embed) ----
F = "_config_embed"
add(F, {"description": "desc", "title": "MyTitle", "color": "red"},
    {"_truncate_discord_component_text": "MyTitle", "discord.Embed": {"e": 1}})
add(F, {"description": "desc"},
    {"t": "Default", "_truncate_discord_component_text": "Default",
     "discord.Color.blue": "blue", "discord.Embed": "E"})
add(F, {"description": "", "title": "", "color": None},
    {"_truncate_discord_component_text": "", "discord.Color.blue": "blue",
     "discord.Embed": "E2"})
add(F, {"description": "d", "title": "T"},
    {}, fc=True, tag="fail-closed: no truncate script")
add(F, {"description": "d", "title": "T"},
    {"_truncate_discord_component_text": raise_v("RuntimeError", "tr")})
add(F, {"description": "d", "title": "T"},
    {"_truncate_discord_component_text": "T",
     "discord.Color.blue": raise_v("RuntimeError", "color")})
add(F, {"description": "d", "title": "T", "color": "red"},
    {"_truncate_discord_component_text": "T",
     "discord.Embed": raise_v("RuntimeError", "embed")})
add(F, {"description": "d"},
    {"t": raise_v("RuntimeError", "tj"), "_truncate_discord_component_text": "T",
     "discord.Color.blue": "blue", "discord.Embed": "E"})

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"WROTE {OUT}: {len(cases)} cases, {len(counts)} funcs")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
