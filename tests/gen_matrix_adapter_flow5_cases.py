#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow5.jsonl -- input cases for the
Matrix-adapter mention / classification / config flow oracle. The oracle
computes want from LIVE adapter.py at gate time; this script only emits
{func, in} inputs.

Deterministic handcrafted branch-coverage cases (>=6 per function). Every flow
carries normal + branch variants; seam-driven funcs also carry a scripted
Exception-swallow (ret), a BaseException-passthrough and a fail-closed (fc=True)
variant. Attribute carriers are {"__attr__": {...}}. A seam result that is a
LIST is wrapped in __script__ so it is a literal return, not a call-order script.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow5.jsonl"
CASES = []


def add(func, args, io=None, state=None, fc=False):
    inp = {"args": args}
    if io:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    case = {"func": func, "in": inp}
    if fc:
        case["fc"] = True
    CASES.append(case)


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(v):
    return {"__script__": True, "ret": v}


def ad(attrs):
    return {"__attr__": attrs}


# ── _run_media_tool (seam: subprocess.run) ──
add("_run_media_tool", {"cmd": ["ffprobe", "-v", "error"], "timeout": 10, "text": True},
    {"subprocess.run": {"returncode": 0, "stdout": "1.5", "stderr": ""}})
add("_run_media_tool", {"cmd": ["ffmpeg", "-i", "x"], "timeout": 15, "text": False},
    {"subprocess.run": {"returncode": 0, "stdout": "abc"}})
add("_run_media_tool", {"cmd": ["ffmpeg"], "timeout": 5}, {"subprocess.run": None})
add("_run_media_tool", {"cmd": ["ffprobe"], "timeout": 10, "text": True},
    {"subprocess.run": rs("ValueError", "bad media")})
add("_run_media_tool", {"cmd": ["ffprobe"], "timeout": 10, "text": True},
    {"subprocess.run": rs("SystemExit", "bye")})
add("_run_media_tool", {"cmd": [], "timeout": 0}, fc=True)

# ── _apply_yaml_config (seam: _apply_yaml_bridge) ──
add("_apply_yaml_config", {"yaml_cfg": {"matrix": {}}, "matrix_cfg": {"homeserver": "https://h"}},
    {"_apply_yaml_bridge": {"ok": True}})
add("_apply_yaml_config", {"yaml_cfg": {}, "matrix_cfg": {}}, {"_apply_yaml_bridge": None})
add("_apply_yaml_config", {"yaml_cfg": {}, "matrix_cfg": {"a": [1, 2]}},
    {"_apply_yaml_bridge": {"homeserver": "x", "changed": True}})
add("_apply_yaml_config", {"matrix_cfg": {"x": "y"}}, {"_apply_yaml_bridge": rs("RuntimeError", "bridge failed")})
add("_apply_yaml_config", {"matrix_cfg": {}}, {"_apply_yaml_bridge": rs("KeyboardInterrupt", "stop")})
add("_apply_yaml_config", {"matrix_cfg": {}}, fc=True)

# ── _is_connected (seam: get_env_value, sequence) ──
add("_is_connected", {"config": {"extra": {"homeserver": "https://h"}, "token": "tok"}}, {})
add("_is_connected", {"config": {"extra": {"homeserver": "https://h"}}},
    {"get_env_value": ["tok", ""]})
add("_is_connected", {"config": {"extra": {}}},
    {"get_env_value": ["https://h", "tok", "pw"]})
add("_is_connected", {"config": {"extra": {}}},
    {"get_env_value": ["", "", ""]})
add("_is_connected", {"config": {"extra": {"homeserver": "  "}, "token": "  "}}, {})
add("_is_connected", {"config": {"extra": {}}},
    {"get_env_value": rs("SystemExit", "bye")})
add("_is_connected", {"config": {"extra": {}}}, fc=True)

# ── _recovery_key_output_path (seams: _get_scoped_secret, expanduser) ──
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": " ~/keys/key ", "expanduser": "/home/u/keys/key"})
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": "  ", "expanduser": "/unused"})
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": "abc", "expanduser": "abc"})
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": None, "expanduser": "/unused"})
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": rs("ValueError", "bad secret"), "expanduser": "/unused"})
add("_recovery_key_output_path", {},
    {"_get_scoped_secret": "z", "expanduser": rs("SystemExit", "bye")})
add("_recovery_key_output_path", {}, fc=True)

# ── _content_mentions_bot (seam: _is_bot_mentioned) ──
add("_content_mentions_bot",
    {"body": "hi @bot:srv", "content": {"m.mentions": {"user_ids": ["@bot:srv"]}, "formatted_body": "<p>hi</p>"}},
    {"_is_bot_mentioned": True})
add("_content_mentions_bot", {"body": "hi", "content": {}}, {"_is_bot_mentioned": False})
add("_content_mentions_bot", {"body": "hi", "content": {"m.mentions": None}},
    {"_is_bot_mentioned": False})
add("_content_mentions_bot", {"body": "hi", "content": {"m.mentions": "notadict"}},
    {"_is_bot_mentioned": False})
add("_content_mentions_bot", {"body": "hi", "content": {}}, {"_is_bot_mentioned": rs("ValueError", "bad")})
add("_content_mentions_bot", {"body": "hi", "content": {}}, {"_is_bot_mentioned": rs("SystemExit", "bye")})
add("_content_mentions_bot", {"body": "hi", "content": {}}, fc=True)

# ── _is_bot_mentioned (seam: _user_localpart) ──
add("_is_bot_mentioned", {"body": "hey", "user_ids": ["@bot:srv"]},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "", "formatted_body": ""},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "hello @bot:srv hi"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "hello bot hi"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "hello robot hi"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "hi", "formatted_body": "matrix.to/#/@bot:srv"},
    {"_user_localpart": "other"}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "hello bot"},
    {"_user_localpart": rs("SystemExit", "bye")}, state={"_user_id": "@bot:srv"})
add("_is_bot_mentioned", {"body": "x"}, fc=True, state={"_user_id": None})

# ── _has_outbound_room_mention (seam: _protect_outbound_mention_regions) ──
add("_has_outbound_room_mention", {"text": "@room hi"},
    {"_protect_outbound_mention_regions": sc(["@room hi", []])}, state={})
add("_has_outbound_room_mention", {"text": "x@room"},
    {"_protect_outbound_mention_regions": sc(["x@room", []])}, state={})
add("_has_outbound_room_mention", {"text": "@rooms"},
    {"_protect_outbound_mention_regions": sc(["@rooms", []])}, state={})
add("_has_outbound_room_mention", {"text": "@room:s"},
    {"_protect_outbound_mention_regions": sc(["@room:s", []])}, state={})
add("_has_outbound_room_mention", {"text": "/@room"},
    {"_protect_outbound_mention_regions": sc(["/@room", []])}, state={})
add("_has_outbound_room_mention", {"text": "@r"},
    {"_protect_outbound_mention_regions": rs("ValueError", "bad")}, state={})
add("_has_outbound_room_mention", {"text": "@r"},
    {"_protect_outbound_mention_regions": rs("SystemExit", "bye")}, state={})
add("_has_outbound_room_mention", {"text": "@r"}, fc=True, state={})

# ── _extract_outbound_mentions (seam: _protect_outbound_mention_regions) ──
add("_extract_outbound_mentions", {"text": "@alice:srv hi @bob:srv"},
    {"_protect_outbound_mention_regions": sc(["@alice:srv hi @bob:srv", []])}, state={})
add("_extract_outbound_mentions", {"text": "@a:s @a:s"},
    {"_protect_outbound_mention_regions": sc(["@a:s @a:s", []])}, state={})
add("_extract_outbound_mentions", {"text": "@a:s:8448"},
    {"_protect_outbound_mention_regions": sc(["@a:s:8448", []])}, state={})
add("_extract_outbound_mentions", {"text": "x@a:s"},
    {"_protect_outbound_mention_regions": sc(["x@a:s", []])}, state={})
add("_extract_outbound_mentions", {"text": ""},
    {"_protect_outbound_mention_regions": sc(["", []])}, state={})
add("_extract_outbound_mentions", {"text": "x"},
    {"_protect_outbound_mention_regions": rs("ValueError", "bad")}, state={})
add("_extract_outbound_mentions", {"text": "x"},
    {"_protect_outbound_mention_regions": rs("SystemExit", "bye")}, state={})
add("_extract_outbound_mentions", {"text": "x"}, fc=True, state={})

# ── _inject_outbound_mention_links (seam: _protect_outbound_mention_regions) ──
add("_inject_outbound_mention_links", {"text": ""},
    {"_protect_outbound_mention_regions": sc(["", []])}, state={})
add("_inject_outbound_mention_links", {"text": "@a:s hi"},
    {"_protect_outbound_mention_regions": sc(["@a:s hi", []])}, state={})
add("_inject_outbound_mention_links", {"text": "@alice:srv and @bob:srv"},
    {"_protect_outbound_mention_regions": sc(["@alice:srv and @bob:srv", []])}, state={})
add("_inject_outbound_mention_links", {"text": "x@a:s"},
    {"_protect_outbound_mention_regions": sc(["x@a:s", []])}, state={})
add("_inject_outbound_mention_links", {"text": "@a:s"},
    {"_protect_outbound_mention_regions": sc(["@a:s", ["@a:s"]])}, state={})
add("_inject_outbound_mention_links", {"text": "@a:s"},
    {"_protect_outbound_mention_regions": rs("SystemExit", "bye")}, state={})
add("_inject_outbound_mention_links", {"text": "@a:s"}, fc=True, state={})

# ── _voice_may_park (seams: is_voice_event, _thread_root, _content_mentions_bot) ──
VM = {"_require_mention": True, "_free_rooms": [], "_allowed_rooms": [], "_threads": []}
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}, "mention_claimed": True},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False}, state=VM)
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}, "mention_claimed": False},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False},
    state=dict(VM, _require_mention=False))
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": False, "_thread_root": None, "_content_mentions_bot": False}, state=VM)
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False},
    state=dict(VM, _free_rooms=["!r:s"]))
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False},
    state=dict(VM, _allowed_rooms=["!other:s"]))
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": "$t", "_content_mentions_bot": False},
    state=dict(VM, _threads=["$t"]))
add("_voice_may_park",
    {"room_id": "!r:s", "body": "/cmd", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False}, state=VM)
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": None, "_content_mentions_bot": False}, state=VM)
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}},
    {"is_voice_event": True, "_thread_root": None,
     "_content_mentions_bot": rs("SystemExit", "bye")}, state=VM)
add("_voice_may_park",
    {"room_id": "!r:s", "body": "hi", "content": {}, "relates_to": {}}, fc=True, state=VM)

# ── _strip_mention (seam: _user_localpart) ──
add("_strip_mention", {"body": "hello @bot:srv world"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": ""}, {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": "hi @bot there"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": "hi @robot"},
    {"_user_localpart": "bot"}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": "hi , bye"},
    {"_user_localpart": "other"}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": "hi @bot"},
    {"_user_localpart": rs("SystemExit", "bye")}, state={"_user_id": "@bot:srv"})
add("_strip_mention", {"body": "hi @x"}, fc=True, state={"_user_id": "@bot:srv"})

# ── _classify_inbound_media (seam: has_voice_marker) ──
add("_classify_inbound_media", {"msgtype": "m.image", "event_mimetype": "", "source_content": {}},
    {"has_voice_marker": False})
add("_classify_inbound_media", {"msgtype": "m.image", "event_mimetype": "image/jpeg", "source_content": {}},
    {"has_voice_marker": False})
add("_classify_inbound_media", {"msgtype": "m.audio", "event_mimetype": "", "source_content": {"voice": True}},
    {"has_voice_marker": True})
add("_classify_inbound_media", {"msgtype": "m.audio", "event_mimetype": "", "source_content": {}},
    {"has_voice_marker": False})
add("_classify_inbound_media", {"msgtype": "m.video", "event_mimetype": "", "source_content": {}},
    {"has_voice_marker": False})
add("_classify_inbound_media", {"msgtype": "m.file", "event_mimetype": "", "source_content": {}},
    {"has_voice_marker": False})
add("_classify_inbound_media", {"msgtype": "m.audio", "source_content": {}},
    {"has_voice_marker": rs("ValueError", "bad")})
add("_classify_inbound_media", {"msgtype": "m.audio", "source_content": {}},
    {"has_voice_marker": rs("SystemExit", "bye")})
add("_classify_inbound_media", {"msgtype": "m.audio", "source_content": {}}, fc=True)

# ── get_chat_info (seam: _resolve_room_identity) ──
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": ad({"display_name": "Room", "chat_type": "dm"})}, state={})
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": ad({"display_name": "Room", "chat_type": "group"})}, state={})
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": ad({"display_name": "Room", "chat_type": None})}, state={})
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": ad({"display_name": None, "chat_type": "dm"})}, state={})
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": rs("ValueError", "bad")}, state={})
add("get_chat_info", {"chat_id": "!r:s"},
    {"_resolve_room_identity": rs("SystemExit", "bye")}, state={})
add("get_chat_info", {"chat_id": "!r:s"}, fc=True, state={})

# ── _query_own_device_keys (seam: query_keys) ──
CL = ad({"mxid": "@me:s", "device_id": "DEV"})
add("_query_own_device_keys", {"client": CL},
    {"query_keys": ad({"device_keys": {"@me:s": {"DEV": {"key": "v"}}}})})
add("_query_own_device_keys", {"client": CL},
    {"query_keys": ad({"device_keys": {}})})
add("_query_own_device_keys", {"client": CL},
    {"query_keys": ad({})})
add("_query_own_device_keys", {"client": CL},
    {"query_keys": ad({"device_keys": {"@me:s": {}}})})
add("_query_own_device_keys", {"client": CL},
    {"query_keys": ad({"device_keys": {"@other:s": {"DEV": {"key": "v"}}}})})
add("_query_own_device_keys", {"client": CL}, {"query_keys": rs("ValueError", "bad")})
add("_query_own_device_keys", {"client": CL}, {"query_keys": rs("SystemExit", "bye")})
add("_query_own_device_keys", {"client": CL}, fc=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
