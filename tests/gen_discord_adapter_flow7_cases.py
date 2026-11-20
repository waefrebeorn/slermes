#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow7.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}}}.
io keys are a subset of the C/oracle seam registry. At least 6 cases per
ported function. `fc: true` marks a deliberate fail-closed case.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow7.jsonl"
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


rv = lambda t, m: {"__script__": True, "raise": {"type": t, "message": m}}
sl = lambda v: {"__script__": True, "ret": v}
N = lambda *x: dict(zip(
    ("everyone", "roles", "users", "replied_user"), x))

# ---- 1 _standalone_response_encoding (resp.get_encoding) ----
F = "_standalone_response_encoding"
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": "utf-16"})
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": "ascii"})
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": ""})
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": None})
add(F, {"resp": {}})
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": rv("ValueError", "boom")})
add(F, {"resp": {"get_encoding": True}}, {"resp.get_encoding": rv("SystemExit", "bye")})
add(F, {"resp": {"get_encoding": True}}, {}, fc=True, tag="fail-closed: no encoding script")

# ---- 2 _standalone_close_response (resp.close / resp.release) ----
F = "_standalone_close_response"
add(F, {"resp": {"close": True}}, {"resp.close": None})
add(F, {"resp": {"release": True}}, {"resp.release": None})
add(F, {"resp": {}})
add(F, {"resp": {"close": True}}, {"resp.close": rv("RuntimeError", "boom")})
add(F, {"resp": {"close": None, "release": True}}, {"resp.release": None})
add(F, {"resp": {"close": True, "release": True}}, {"resp.close": None, "resp.release": None})
add(F, {"resp": {"close": True}}, {}, fc=True, tag="fail-closed: no close script")

# ---- 3 _remember_channel_is_forum (state mutator, no seams) ----
F = "_remember_channel_is_forum"
add(F, {"chat_id": "1", "is_forum": True})
add(F, {"chat_id": 2, "is_forum": False})
add(F, {"chat_id": "x", "is_forum": True}, state={"_channel_type_probe_cache": {"old": False}})
add(F, {"chat_id": "", "is_forum": False})
add(F, {"chat_id": None, "is_forum": True})
add(F, {"chat_id": "é", "is_forum": "yes"})
add(F, {"chat_id": "1", "is_forum": True}, state={"_channel_type_probe_cache": {"1": False}})

# ---- 4 _probe_is_forum_cached (state reader, no seams) ----
F = "_probe_is_forum_cached"
add(F, {"chat_id": "1"}, state={"_channel_type_probe_cache": {"1": True}})
add(F, {"chat_id": "2"}, state={"_channel_type_probe_cache": {"2": False}})
add(F, {"chat_id": "3"}, state={"_channel_type_probe_cache": {}})
add(F, {"chat_id": "1"})
add(F, {"chat_id": 5, "unused": 1}, state={"_channel_type_probe_cache": {"5": True}})
add(F, {"chat_id": None}, state={"_channel_type_probe_cache": {"None": False}})
add(F, {"chat_id": "é"}, state={"_channel_type_probe_cache": {"é": True}})

# ---- 5 _build_allowed_mentions (_extra_or_secret, discord.AllowedMentions) ----
F = "_build_allowed_mentions"
AM = {"discord.AllowedMentions": "AM"}
E4 = {"_extra_or_secret": [None, None, None, None], **AM}
add(F, {}, E4)
add(F, {"extra": {"allow_mentions": {"everyone": True, "roles": "no", "users": False, "replied_user": "YES"}}},
    {"_extra_or_secret": [True, "no", False, "YES"], **AM})
add(F, {"extra": {"allow_mentions": {"everyone": "true", "roles": 1, "users": "0", "replied_user": "off"}}},
    {"_extra_or_secret": ["true", 1, "0", "off"], **AM})
add(F, {"extra": {"allow_mentions": "not-a-dict"}}, E4)
add(F, {}, {"_extra_or_secret": [None, None, rv("RuntimeError", "boom"), None], **AM})
add(F, {}, {}, state={"DISCORD_AVAILABLE": False}, tag="discord unavailable -> None")
add(F, {}, {}, fc=True, tag="fail-closed: no _extra_or_secret script")

# ---- 6 _load_discord_int_config (read_raw_config, logger.debug) ----
F = "_load_discord_int_config"
LR = {"logger.debug": None}
BASE = {"key": "k", "default": 5, "minimum": 0}
add(F, dict(BASE), {"read_raw_config": {"discord": {"k": 5}}, **LR})
add(F, dict(BASE), {"read_raw_config": {"discord": {"k": "7"}}, **LR})
add(F, dict(BASE, minimum=3), {"read_raw_config": {"discord": {"k": -2}}, **LR})
add(F, dict(BASE), {"read_raw_config": {"discord": {}}, **LR})
add(F, dict(BASE), {"read_raw_config": None, **LR})
add(F, dict(BASE), {"read_raw_config": {"discord": {"k": "abc"}}, **LR})
add(F, dict(BASE), {"read_raw_config": rv("ValueError", "boom"), **LR})

# ---- 7 _load_voice_timeout (_load_discord_int_config) ----
F = "_load_voice_timeout"
add(F, {}, {"_load_discord_int_config": 300})
add(F, {}, {"_load_discord_int_config": 0})
add(F, {}, {"_load_discord_int_config": rv("RuntimeError", "boom")}, fc=True)
add(F, {}, {}, fc=True, tag="fail-closed: no loader script")
add(F, {}, {"_load_discord_int_config": None})
add(F, {}, {"_load_discord_int_config": "77"})

# ---- 8 _load_playback_timeout (_load_discord_int_config) ----
F = "_load_playback_timeout"
add(F, {}, {"_load_discord_int_config": 120})
add(F, {}, {"_load_discord_int_config": 45})
add(F, {}, {"_load_discord_int_config": rv("ValueError", "boom")}, fc=True)
add(F, {}, {}, fc=True, tag="fail-closed: no loader script")
add(F, {}, {"_load_discord_int_config": None})
add(F, {}, {"_load_discord_int_config": 0})

# ---- 9 _voice_timeout_limit (state attrs, no seams) ----
F = "_voice_timeout_limit"
add(F, {})
add(F, {}, state={"_voice_timeout_seconds": 45})
add(F, {}, state={"VOICE_TIMEOUT": 99})
add(F, {}, state={"_voice_timeout_seconds": "60"})
add(F, {}, state={"_voice_timeout_seconds": -5})
add(F, {}, state={"VOICE_TIMEOUT": 100, "_voice_timeout_seconds": 7})

# ---- 10 _playback_timeout_limit (state attrs, no seams) ----
F = "_playback_timeout_limit"
add(F, {})
add(F, {}, state={"_playback_timeout_seconds": 15})
add(F, {}, state={"PLAYBACK_TIMEOUT": 200})
add(F, {}, state={"_playback_timeout_seconds": "30"})
add(F, {}, state={"_playback_timeout_seconds": 0})
add(F, {}, state={"PLAYBACK_TIMEOUT": 90, "_playback_timeout_seconds": 12})

# ---- 11 _exec_approval_cmd_budget (_approval_mention_content, super budget) ----
F = "_exec_approval_cmd_budget"
BB = {"super._exec_approval_cmd_budget": 100}
add(F, {"description": "d", "smart_denied": False}, {"_approval_mention_content": "<@1>", **BB})
add(F, {"description": "d", "smart_denied": True}, {"_approval_mention_content": None, **BB})
add(F, {"description": "d", "smart_denied": False}, {"_approval_mention_content": "", **BB})
add(F, {"description": "d", "smart_denied": False}, {"_approval_mention_content": "x", "super._exec_approval_cmd_budget": 0})
add(F, {"description": "d", "smart_denied": False}, {"_approval_mention_content": rv("RuntimeError", "boom"), **BB}, fc=True)
add(F, {"description": "d", "smart_denied": False}, {"_approval_mention_content": "x", "super._exec_approval_cmd_budget": rv("ValueError", "boom")}, fc=True)
add(F, {"description": "d", "smart_denied": False}, {}, fc=True, tag="fail-closed: no mention script")

# ---- 12 _text_batch_delay_for (super._text_batch_delay_for) ----
F = "_text_batch_delay_for"
add(F, {"pending": {}}, {"super._text_batch_delay_for": 0.5})
add(F, {"pending": {"_bot_tag_debounce": True}}, {"super._text_batch_delay_for": 0.5},
    state={"_text_batch_split_delay_seconds": 2.5})
add(F, {"pending": {"_bot_tag_debounce": False}}, {"super._text_batch_delay_for": 1.0})
add(F, {"pending": {"_bot_tag_debounce": True}}, {})
add(F, {"pending": {}}, {}, fc=True, tag="fail-closed: no super script")
add(F, {"pending": {}}, {"super._text_batch_delay_for": rv("RuntimeError", "boom")}, fc=True)

# ---- 13 _resolve_channel_prompt (resolve_channel_prompt) ----
F = "_resolve_channel_prompt"
add(F, {"channel_id": "1", "parent_id": None}, {"resolve_channel_prompt": "prompt-1"})
add(F, {"channel_id": "1", "parent_id": "2"}, {"resolve_channel_prompt": "prompt-2"})
add(F, {"channel_id": "1"}, {"resolve_channel_prompt": None}, state={"config_extra": {"x": 1}})
add(F, {"channel_id": "1", "parent_id": "9"}, {"resolve_channel_prompt": ""})
add(F, {"channel_id": "1"}, {}, fc=True, tag="fail-closed: no resolver script")
add(F, {"channel_id": "1"}, {"resolve_channel_prompt": rv("ValueError", "boom")}, fc=True)

# ---- 14 _playback_timeout_for_audio (_playback_timeout_limit, _probe_audio_duration_seconds) ----
F = "_playback_timeout_for_audio"
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": 30, "_probe_audio_duration_seconds": 10})
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": 60, "_probe_audio_duration_seconds": 10})
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": 30, "_probe_audio_duration_seconds": None})
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": 30, "_probe_audio_duration_seconds": 0})
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": rv("RuntimeError", "boom")}, fc=True)
add(F, {"audio_path": "/a.wav"}, {}, fc=True, tag="fail-closed: no limit script")
add(F, {"audio_path": "/a.wav"}, {"_playback_timeout_limit": 30,
     "_probe_audio_duration_seconds": rv("ValueError", "boom")}, fc=True)

# ---- 15 _attachment_message_type (_is_discord_voice_message_attachment) ----
F = "_attachment_message_type"
add(F, {"att": {"content_type": "image/png"}})
add(F, {"att": {"content_type": "video/mp4"}})
add(F, {"att": {"content_type": "audio/ogg"}}, {"_is_discord_voice_message_attachment": True})
add(F, {"att": {"content_type": "audio/ogg"}}, {"_is_discord_voice_message_attachment": False})
add(F, {"att": {"content_type": "text/plain"}})
add(F, {"att": {"content_type": ""}})
add(F, {"att": {"content_type": "audio/mp3"}}, {}, fc=True, tag="fail-closed: no voice script")
add(F, {"att": {"content_type": "audio/mp3"}}, {"_is_discord_voice_message_attachment": rv("RuntimeError", "boom")}, fc=True)

# ---- 16 _guild_member_has_role (guild.get_member) ----
F = "_guild_member_has_role"
G = {"guild": {"id": 1}}
add(F, {**G, "user_id": "42", "allowed_roles": [7]},
    {"guild.get_member": {"roles": [{"id": 7}]}})
add(F, {**G, "user_id": "42", "allowed_roles": [9]},
    {"guild.get_member": {"roles": [{"id": 7}]}})
add(F, {**G, "user_id": "abc", "allowed_roles": [7]})
add(F, {**G, "user_id": "42", "allowed_roles": [7]}, {"guild.get_member": None})
add(F, {**G, "user_id": "42", "allowed_roles": [7]}, {}, fc=True, tag="fail-closed: no get_member script")
add(F, {**G, "user_id": "42", "allowed_roles": [7]}, {"guild.get_member": rv("RuntimeError", "boom")}, fc=True)
add(F, {**G, "user_id": "42", "allowed_roles": [7]}, {"guild.get_member": {"roles": []}})

# ---- 17 _reply_target (_Snowflake) ----
F = "_reply_target"
add(F, {"reference": {"resolved": {"id": 5, "name": "r"}}})
add(F, {"reference": {"resolved": None, "message_id": "123"}}, {"_Snowflake": {"id": 123}})
add(F, {"reference": {"message_id": "abc"}})
add(F, {"reference": {}})
add(F, {"reference": {"resolved": {"name": "no-id"}, "message_id": "9"}}, {"_Snowflake": {"id": 9}})
add(F, {"reference": {"message_id": "123"}}, {}, fc=True, tag="fail-closed: no _Snowflake script")
add(F, {"reference": {"message_id": "123"}}, {"_Snowflake": rv("RuntimeError", "boom")}, fc=True)

# ---- 18 _stamp_auto_thread_name (no seams) ----
F = "_stamp_auto_thread_name"
add(F, {"thread": {"name": "x"}, "thread_name": "orig"})
add(F, {"thread": {}, "thread_name": ""})
add(F, {"thread": "scalar", "thread_name": "n"})
add(F, {"thread": {"id": 1}, "thread_name": "héllo"})
add(F, {"thread": {"_hermes_auto_thread_initial_name": "old"}, "thread_name": "new"})
add(F, {"thread": None, "thread_name": "x"})
add(F, {"thread": {"id": 2}, "thread_name": None})

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"WROTE {OUT}: {len(cases)} cases, {len(counts)} funcs")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
