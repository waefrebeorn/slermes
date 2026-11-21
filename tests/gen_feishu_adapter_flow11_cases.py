#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow11.jsonl — input cases for the
Feishu adapter remainders FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow11_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow11.jsonl"
CASES = []


def add(func, args, io=None, note=None):
    case = {"func": func, "in": {"args": args, "io": dict(io or {})}}
    if note:
        case["_note"] = note
    CASES.append(case)


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


SETTINGS_KEYS = [
    "app_id", "app_secret", "domain_name", "connection_mode", "encrypt_key",
    "verification_token", "group_policy", "allowed_group_users", "bot_open_id",
    "bot_user_id", "bot_name", "dedup_cache_size", "text_batch_delay_seconds",
    "text_batch_split_delay_seconds", "text_batch_max_messages", "text_batch_max_chars",
    "media_batch_delay_seconds", "webhook_host", "webhook_port", "webhook_path",
    "ws_reconnect_nonce", "ws_reconnect_interval", "ws_ping_interval",
    "ws_ping_timeout", "admins", "default_group_policy", "group_rules",
    "allow_bots", "require_mention", "allow_all_dm",
]
SETTINGS_OBJ = {k: 0 for k in SETTINGS_KEYS}


def settings_base_io():
    """Default scripts covering every seam _load_settings reaches."""
    return {
        "extra_or_secret": [ret("")] * 24,
        "scoped_secret": [ret("")] * 24,
        "env_int": [ret(2048)] * 6,
        "env_float": [ret(0.6)] * 5,
        "coerce_required_int": [ret(30)] * 4,
        "coerce_int": [ret(None)] * 4,
        "to_boolean": [ret(True)] * 6,
        "group_rule": [ret({})] * 6,
        "settings": [ret(SETTINGS_OBJ)],
    }


# ── _hydrate_bot_identity ──
BOTINFO = json.dumps({"code": 0, "bot": {"app_name": " Assistant ", "open_id": " ou1 "}})
add("_hydrate_bot_identity", {"client": False, "bot_open_id": "", "bot_name": "", "app_id": "app"}, io={})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret(BOTINFO)],
        "parse_bot_response": [ret({"bot_open_id": " ou1 ", "bot_name": " Bot "})]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "old", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret(BOTINFO)],
        "parse_bot_response": [ret({"bot_open_id": "ou2", "bot_name": "Bot2"})]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret("")], "build_app_request": [ret({})], "run_blocking_app": [ret(None)]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret(BOTINFO)],
        "parse_bot_response": [ret({"bot_open_id": "", "bot_name": ""})],
        "build_app_request": [ret({})], "run_blocking_app": [ret({"data": {"app": {"app_name": " Fallback "}}})],
        "response_success": [ret(True)]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [rais("ValueError", "down")], "build_app_request": [ret({})], "run_blocking_app": [ret(None)]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [rais("SystemExit", "sx")]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret(BOTINFO)], "parse_bot_response": [rais("SystemExit", "sx")]})
add("_hydrate_bot_identity", {"client": True, "bot_open_id": "", "bot_name": "", "app_id": "app"},
    io={"tenant_get_bot_info": [ret("")], "build_app_request": [rais("SystemExit", "sx")]}, note="fail-closed")


# ── _install_lark_ws_isolation ──
add("_install_lark_ws_isolation", {"installed": True, "loop": "L", "connect_name": "connect"}, io={})
add("_install_lark_ws_isolation", {"installed": False, "loop": "L", "connect_name": "connect"},
    io={"wrap_loop": [ret(None)], "install_connect": [ret(None)], "install_receive": [ret(None)]})
add("_install_lark_ws_isolation", {"installed": False, "loop": {"v": 1}, "connect_name": "myconnect"},
    io={"wrap_loop": [ret(None)], "install_connect": [ret(None)], "install_receive": [ret(None)]})
add("_install_lark_ws_isolation", {"installed": False, "loop": "L", "connect_name": "connect"},
    io={"wrap_loop": [rais("ValueError", "down")]})
add("_install_lark_ws_isolation", {"installed": False, "loop": "L", "connect_name": "connect"},
    io={"wrap_loop": [ret(None)], "install_connect": [rais("SystemExit", "sx")]})
add("_install_lark_ws_isolation", {"installed": False, "loop": "L", "connect_name": "connect"},
    io={"wrap_loop": [ret(None)], "install_connect": [ret(None)], "install_receive": [rais("SystemExit", "sx")]})
add("_install_lark_ws_isolation", {"installed": False, "loop": "L", "connect_name": "connect"},
    io={}, note="fail-closed")


# ── _load_lark_oapi ──
# The live loop does getattr(module, name) for each SDK name; a scripted module
# must carry those names or live fail-closes with AttributeError. A superset dict
# satisfies every module's getattr (extra names are harmless).
MOD = {n: 1 for n in [
    "AccessTokenType", "BaseRequest", "CallBackCard", "CallBackToast", "CreateFileRequest",
    "CreateFileRequestBody", "CreateImageRequest", "CreateImageRequestBody", "CreateMessageRequest",
    "CreateMessageRequestBody", "DeleteMessageRequest", "EventDispatcherHandler", "FEISHU_DOMAIN",
    "GetApplicationRequest", "GetChatRequest", "GetMessageRequest", "GetMessageResourceRequest",
    "HttpMethod", "LARK_DOMAIN", "P2CardActionTriggerResponse", "P2ImMessageMessageReadV1",
    "PatchMessageRequest", "PatchMessageRequestBody", "ReplyMessageRequest", "ReplyMessageRequestBody",
    "UpdateMessageRequest", "UpdateMessageRequestBody", "Client"]}
add("_load_lark_oapi", {"available": True}, io={})
add("_load_lark_oapi", {"available": False}, io={"import_module": [ret(MOD)] * 9})
add("_load_lark_oapi", {"available": False}, io={"import_module": [rais("ImportError", "no lark")]})
add("_load_lark_oapi", {"available": False}, io={"import_module": [ret(MOD), ret(MOD), rais("ModuleNotFoundError", "missing")]})
add("_load_lark_oapi", {"available": False}, io={"import_module": [ret(MOD)] * 4 + [rais("AttributeError", "no attr")]})
add("_load_lark_oapi", {"available": False}, io={"import_module": [ret(MOD), rais("SystemExit", "sx")]})
add("_load_lark_oapi", {"available": False}, io={}, note="fail-closed")


# ── _load_seen_message_ids ──
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [ret('{"message_ids":{"a":100,"b":200}}')]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [ret('{"message_ids":{}}')]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [ret('["x","y"]')]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [ret('{"message_ids":[1,2]}')]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [ret('{}')]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [rais("FileNotFoundError", "gone")]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [rais("OSError", "io")]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={"read_text": [rais("SystemExit", "sx")]})
add("_load_seen_message_ids", {"path": "/h/seen.json"}, io={}, note="fail-closed")


# ── _load_settings ──
EX = {"webhook_port": 8765}
add("_load_settings", EX, io=settings_base_io())
add("_load_settings", dict(EX, group_rules={
    "c1": {"policy": " OPEN ", "allowlist": [" u1 ", "u2", ""], "blacklist": ["b1"], "require_mention": True},
    "c2": {"policy": "allowlist"}, "c3": "not-a-dict"}), io=settings_base_io())
add("_load_settings", EX, io=dict(settings_base_io(),
    extra_or_secret=[ret("all"), ret("app"), ret("sec"), ret("feishu"), ret("websocket"),
                     ret("key"), ret("tok"), ret("host"), ret("/path")] + [ret("")] * 16))
add("_load_settings", EX, io=dict(settings_base_io(),
    extra_or_secret=[ret("weird")] + [ret("")] * 23))
add("_load_settings", dict(EX, require_mention=False, admins=[" a ", "b", "a"],
                           default_group_policy=" ADMIN "), io=settings_base_io())
add("_load_settings", EX, io=dict(settings_base_io(),
    coerce_required_int=[rais("SystemExit", "sx")]))
add("_load_settings", EX, io=dict(settings_base_io(),
    env_int=[ret(2048), ret(2048), ret(2048), ret(2), ret(4001)]))
add("_load_settings", EX, io=dict(settings_base_io(),
    to_boolean=[rais("ValueError", "bad")] ))
add("_load_settings", EX, io={}, note="fail-closed")


# ── _maybe_extract_text_document ──
add("_maybe_extract_text_document", {"cached_path": "", "media_type": ""}, io={})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={"getsize": [ret(100)], "read_text": [ret("hello")], "display_name": [ret("A")]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.MD", "media_type": "application/octet-stream"},
    io={"getsize": [ret(100)], "read_text": [ret("md")], "display_name": [ret("B")]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.pdf", "media_type": "application/pdf"}, io={})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={"getsize": [ret(200000)]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={"getsize": [ret(100)], "read_text": [rais("OSError", "io")]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={"getsize": [rais("SystemExit", "sx")]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={"getsize": [ret(100)], "read_text": [ret("x")], "display_name": [rais("ValueError", "bad")]})
add("_maybe_extract_text_document", {"cached_path": "/tmp/a.txt", "media_type": "text/plain"},
    io={}, note="fail-closed")


# ── _on_drive_comment_event ──
add("_on_drive_comment_event", {"data": {"type": "comment"}}, io={"submit_if_ready": [ret(None)]})
add("_on_drive_comment_event", {"data": {}}, io={"submit_if_ready": [ret("queued")]})
add("_on_drive_comment_event", {"data": {"x": 1}, "owner": "p1"}, io={"submit_if_ready": [ret(None)]})
add("_on_drive_comment_event", {"data": {}}, io={"submit_if_ready": [rais("ValueError", "down")]})
add("_on_drive_comment_event", {"data": {}}, io={"submit_if_ready": [rais("SystemExit", "sx")]})
add("_on_drive_comment_event", {"data": {}}, io={}, note="fail-closed")


# ── _on_meeting_invited_event ──
add("_on_meeting_invited_event", {"data": {"meeting": "m1"}}, io={"submit_if_ready": [ret(None)]})
add("_on_meeting_invited_event", {"data": {}}, io={"submit_if_ready": [ret("queued")]})
add("_on_meeting_invited_event", {"data": {"x": 2}}, io={"submit_if_ready": [ret(None)]})
add("_on_meeting_invited_event", {"data": {}}, io={"submit_if_ready": [rais("ValueError", "down")]})
add("_on_meeting_invited_event", {"data": {}}, io={"submit_if_ready": [rais("SystemExit", "sx")]})
add("_on_meeting_invited_event", {"data": {}}, io={}, note="fail-closed")


# ── _on_message_event ──
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(False)], "enqueue_pending": [ret(False)]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(False)], "enqueue_pending": [ret(True)], "thread_start": [ret(None)]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(True)], "handle_event_data": [ret(None)], "submit_on_loop": [ret(None)]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [rais("ValueError", "down")]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(False)], "enqueue_pending": [rais("SystemExit", "sx")]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(True)], "handle_event_data": [rais("ValueError", "bad")]})
add("_on_message_event", {"loop": "L", "data": {"m": 1}},
    io={"loop_accepts": [ret(True)], "handle_event_data": [ret(None)], "submit_on_loop": [rais("SystemExit", "sx")]})
add("_on_message_event", {"loop": "L", "data": {}}, io={}, note="fail-closed")


# ── _persist_seen_message_ids ──
P = "/h/state/seen.json"
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a", "b"], "seen_ids": {"a": 1, "b": 2}, "cache_size": 2},
    io={"mkdir": [ret(None)], "atomic_write": [ret(None)]})
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a", "b", "c"], "seen_ids": {"a": 1, "b": 2, "c": 3}, "cache_size": 2},
    io={"mkdir": [ret(None)], "atomic_write": [ret(None)]})
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a"], "seen_ids": {}, "cache_size": 5},
    io={"mkdir": [ret(None)], "atomic_write": [ret(None)]})
add("_persist_seen_message_ids", {"path": P, "seen_order": [], "seen_ids": {}, "cache_size": 2},
    io={"mkdir": [ret(None)], "atomic_write": [ret(None)]})
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a"], "seen_ids": {"a": 1}, "cache_size": 2},
    io={"mkdir": [rais("OSError", "io")]})
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a"], "seen_ids": {"a": 1}, "cache_size": 2},
    io={"mkdir": [ret(None)], "atomic_write": [rais("OSError", "io")]})
add("_persist_seen_message_ids", {"path": P, "seen_order": ["a"], "seen_ids": {"a": 1}, "cache_size": 2},
    io={"mkdir": [rais("SystemExit", "sx")]})
add("_persist_seen_message_ids", {"path": P, "seen_order": [], "seen_ids": {}, "cache_size": 2},
    io={}, note="fail-closed")


# ── _id_set ──
add("_id_set", {"values": []}, io={})
add("_id_set", {"values": ["a"]}, io={})
add("_id_set", {"values": [" a ", "b", "", " c"]}, io={})
add("_id_set", {"values": ["u1", "u1", "u2"]}, io={})
add("_id_set", {"values": [1, 2, "2", True]}, io={})
add("_id_set", {"values": ["Z", "a", "M"]}, io={})


# ── _secret ──
for v in ["", " X ", "abc", " u1 ", "true", "0"]:
    add("_secret", {"name": "FEISHU_APP_ID", "default": ""}, io={"scoped_secret": [ret(v)]})
add("_secret", {"name": "N", "default": "d"}, io={"scoped_secret": [rais("ValueError", "bad")]})
add("_secret", {"name": "N", "default": "d"}, io={}, note="fail-closed")


# ── _extra_or_secret ──
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={"extra_or_secret": [ret(" V ")]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={"extra_or_secret": [ret(123)]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={"extra_or_secret": [ret(None)]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={"extra_or_secret": [ret(True)]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": "d"}, io={"extra_or_secret": [ret("")]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={"extra_or_secret": [rais("ValueError", "bad")]})
add("_extra_or_secret", {"extra": {}, "key": "k", "env": "E", "default": ""}, io={}, note="fail-closed")


# ── _dispatch_connect ──
add("_dispatch_connect", {"pos_args": [], "kwargs": {}},
    io={"connect_kwargs": [ret({})], "real_connect": [ret("ok")]})
add("_dispatch_connect", {"pos_args": ["a"], "kwargs": {"x": 1}},
    io={"connect_kwargs": [ret({"b": 2})], "real_connect": [ret("ok")]})
add("_dispatch_connect", {"pos_args": [], "kwargs": {"x": 1}},
    io={"connect_kwargs": [ret({"x": 9})], "real_connect": [ret("ok")]})
add("_dispatch_connect", {"pos_args": [], "kwargs": {"x": 1, "y": 2}},
    io={"connect_kwargs": [ret(None)], "real_connect": [ret(None)]})
add("_dispatch_connect", {"pos_args": [], "kwargs": {}},
    io={"connect_kwargs": [rais("ValueError", "bad")]})
add("_dispatch_connect", {"pos_args": [], "kwargs": {}},
    io={"connect_kwargs": [ret({})], "real_connect": [rais("SystemExit", "sx")]})
add("_dispatch_connect", {"pos_args": [], "kwargs": {}}, io={}, note="fail-closed")


OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")
