#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow.jsonl for the Discord adapter
FLOW port (src/gateway/platforms/discord_adapter_flow.c).

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list RESULTS one level deeper), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides.

Every ported function gets >= 5 cases; error paths, BaseException
propagation and fail-closed scripts are covered per flow.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "discord_adapter_flow.jsonl")

CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True  # intentional fail-closed script gap
    CASES.append(c)


def raise_script(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret_script(v):
    return {"__script__": True, "ret": v}


# ── _image_ext_from_content_type ──
for ct in ("image/jpeg", "image/jpg", "image/gif", "image/webp", "image/png",
           "application/octet-stream", "image/JPEG"):
    add("_image_ext_from_content_type", {"content_type": ct})

# ── _format_discord_markdown_link ──
add("_format_discord_markdown_link", {"label": "Docs", "url": "https://x.com/a b"})
add("_format_discord_markdown_link", {"label": "https://x.com", "url": "https://y.com"})
add("_format_discord_markdown_link", {"label": "HTTP://X.COM", "url": "u"})
add("_format_discord_markdown_link", {"label": "a[b]\\c]d", "url": "https://e.com/p?q=1&z=2#f"})
add("_format_discord_markdown_link", {"label": "", "url": ""})
add("_format_discord_markdown_link", {"label": "see https://x", "url": "https://\u00fc.com/\u00e9"})
add("_format_discord_markdown_link", {"label": "L", "url": "https://h.com/:/?#[]@!$&'*+,;=%~"})

# ── _needs_server_members_intent ──
add("_needs_server_members_intent", {"allowed_user_ids": ["123", "456"], "allowed_role_ids": None})
add("_needs_server_members_intent", {"allowed_user_ids": ["alice"], "allowed_role_ids": None})
add("_needs_server_members_intent", {"allowed_user_ids": ["*"], "allowed_role_ids": None})
add("_needs_server_members_intent", {"allowed_user_ids": ["*", "123"], "allowed_role_ids": ["999"]})
add("_needs_server_members_intent", {"allowed_user_ids": None, "allowed_role_ids": []})
add("_needs_server_members_intent", {"allowed_user_ids": [123, 456], "allowed_role_ids": None})
add("_needs_server_members_intent", {"allowed_user_ids": ["12a"], "allowed_role_ids": None})
add("_needs_server_members_intent", {"allowed_user_ids": [], "allowed_role_ids": None})

# ── _format_privileged_intents_guidance ──
for nm in (True, False, 1, None, "x"):
    add("_format_privileged_intents_guidance", {"needs_members": nm})

# ── _discord_snowflake_time ──
for sf in (0, 175928847299117063, 4194304, 4194303, -1, 1000000000000000000):
    add("_discord_snowflake_time", {"snowflake": sf})

# ── _metadata_marks_nonconversational ──
for md in (None, {}, {"non_conversational": True}, {"non_conversational_history": 1},
           {"non_conversational": False, "non_conversational_history": ""},
           {"other": True}, "notdict", {"non_conversational": []}):
    add("_metadata_marks_nonconversational", {"metadata": md})

# ── _prompt_target_id ──
add("_prompt_target_id", {"chat_id": "c", "metadata": None})
add("_prompt_target_id", {"chat_id": "c", "metadata": {"thread_id": "t"}})
add("_prompt_target_id", {"chat_id": "c", "metadata": {"thread_id": ""}})
add("_prompt_target_id", {"chat_id": "c", "metadata": {}})
add("_prompt_target_id", {"chat_id": "c", "metadata": {"thread_id": 0}})
add("_prompt_target_id", {"chat_id": "c", "metadata": {"thread_id": None}})
add("_prompt_target_id", {"chat_id": "c", "metadata": "str"})

# ── _clean_discord_id ──
for e in ("123", " <@123> ", "<@!123>", "user:123", "USER:alice",
          "  user:  bob  ", "<<@123>>", "user:", "<@!>"):
    add("_clean_discord_id", {"entry": e})

# ── _command_sync_state_key ──
for app in ("123", 123, None, 0, "", True, False):
    add("_command_sync_state_key", {"app_id": app})

# ── _command_sync_mutation_interval_seconds ──
for i in range(5):
    add("_command_sync_mutation_interval_seconds", {}, {"name": f"Discord{i}"})

# ── _truncate_discord_component_text ──
add("_truncate_discord_component_text", {"text": "hello", "limit": 100},
    io={"_prefix_within_utf16_limit": "hello"})
add("_truncate_discord_component_text", {"text": None, "limit": 5},
    io={"_prefix_within_utf16_limit": ""})
add("_truncate_discord_component_text", {"text": 0, "limit": 5},
    io={"_prefix_within_utf16_limit": ""})
add("_truncate_discord_component_text", {"text": 123, "limit": 5},
    io={"_prefix_within_utf16_limit": "123"})
add("_truncate_discord_component_text", {"text": True, "limit": -5},
    io={"_prefix_within_utf16_limit": "True"})
add("_truncate_discord_component_text", {"text": "x", "limit": 5}, fc=True)  # fail-closed
add("_truncate_discord_component_text", {"text": "x", "limit": 5},
    io={"_prefix_within_utf16_limit": raise_script("ValueError", "bad")})

# ── _discord_ready_timeout_seconds ──
add("_discord_ready_timeout_seconds", io={"os.getenv": "30"})
add("_discord_ready_timeout_seconds", io={"os.getenv": "  15.5  "})
add("_discord_ready_timeout_seconds", io={"os.getenv": ""})
add("_discord_ready_timeout_seconds", io={"os.getenv": "abc", "logger.warning": None})
add("_discord_ready_timeout_seconds", io={"os.getenv": "-5"})
add("_discord_ready_timeout_seconds", io={"os.getenv": "nan"})
add("_discord_ready_timeout_seconds", fc=True)  # fail-closed
add("_discord_ready_timeout_seconds", io={"os.getenv": None})  # None.strip AttributeError

# ── _text_batch_flush_deadline_seconds ──
for raw in ("", "10", "0.5", "abc", "-3", "nan", "2"):
    add("_text_batch_flush_deadline_seconds", io={"os.getenv": raw})
add("_text_batch_flush_deadline_seconds", fc=True)  # fail-closed

# ── _multiplex_active ──
add("_multiplex_active", state={"multiplex_import_ok": True}, io={"is_multiplex_active": True})
add("_multiplex_active", state={"multiplex_import_ok": True}, io={"is_multiplex_active": False})
add("_multiplex_active", state={"multiplex_import_ok": True}, io={"is_multiplex_active": 1})
add("_multiplex_active", state={"multiplex_import_ok": True}, io={"is_multiplex_active": None})
add("_multiplex_active", state={"multiplex_import_ok": True},
    io={"is_multiplex_active": raise_script("ValueError", "x")})
add("_multiplex_active", state={"multiplex_import_ok": True},
    io={"is_multiplex_active": raise_script("SystemExit", "bye")})
add("_multiplex_active", state={"multiplex_import_ok": False})
add("_multiplex_active", state={"multiplex_import_ok": True}, fc=True)  # fail-closed -> except -> False

# ── discord_deps_present ──
for flag in (True, False, None, 1):
    add("discord_deps_present", state={"discord_available": flag})
add("discord_deps_present")  # absent flag

# ── _is_discord_transport_error ──
def exc(cls, msg="", dbase=None, attrs=None):
    return {"cls": cls, "msg": msg, "discord_base": dbase, "attrs": attrs or {}}

add("_is_discord_transport_error", {"exc": exc("TimeoutError", "timed out")})
add("_is_discord_transport_error", {"exc": exc("ConnectionResetError", "peer reset")})
add("_is_discord_transport_error", {"exc": exc("OSError", "x")})
add("_is_discord_transport_error", {"exc": exc("FileNotFoundError", "x")})
add("_is_discord_transport_error", {"exc": exc("Exception", "WebSocket closed unexpectedly")})
add("_is_discord_transport_error", {"exc": exc("Exception", "session is closed")})
add("_is_discord_transport_error", {"exc": exc("Exception", "rate limited")})
add("_is_discord_transport_error", {"exc": exc("Custom", "x", "ConnectionClosed")},
    state={"discord_available": True})
add("_is_discord_transport_error", {"exc": exc("Custom", "x", "ConnectionClosed")},
    state={"discord_available": False})
add("_is_discord_transport_error", {"exc": exc("Custom", "x", "GatewayNotFound")},
    state={"discord_available": True})
add("_is_discord_transport_error", {"exc": exc("ValueError", "not connected")})

# ── _config_value ──
CFG = {"config": {"extra": {"k": "v"}}}
add("_config_value", {"key": "k", "default": "d", "env_key": None}, CFG)
add("_config_value", {"key": "missing", "default": "d", "env_key": None}, CFG)
add("_config_value", {"key": "missing", "default": "d", "env_key": "E"}, CFG,
    io={"_scoped_gate_env": "ev"})
add("_config_value", {"key": "missing", "default": "d", "env_key": "E"}, CFG,
    io={"_scoped_gate_env": ""})
add("_config_value", {"key": "missing", "default": "d", "env_key": "E"}, CFG,
    io={"_scoped_gate_env": None})
add("_config_value", {"key": "k", "default": "d", "env_key": "E"},
    {"config": {"extra": {"k": ""}}}, io={"_scoped_gate_env": "ev"})
add("_config_value", {"key": "k", "default": "d", "env_key": "E"},
    {"config": {"extra": {"k": None}}}, io={"_scoped_gate_env": "ev"})
add("_config_value", {"key": "k", "default": "d", "env_key": None},
    {"config": {"extra": {"k": 0}}})
add("_config_value", {"key": "missing", "default": "d", "env_key": ""}, CFG)
add("_config_value", {"key": "missing", "default": "d", "env_key": None},
    {"config": {"extra": [1, 2]}})
add("_config_value", {"key": "missing", "default": "d", "env_key": "E"}, CFG, fc=True)  # fail-closed
add("_config_value", {"key": "missing", "default": "d", "env_key": "E"}, CFG,
    io={"_scoped_gate_env": raise_script("SystemExit", "bye")})

# ── _warn_liveness_config_disabled ──
add("_warn_liveness_config_disabled", {"key": "websocket_event_max_silence_seconds", "raw": "15s"},
    {"name": "Discord"}, io={"logger.warning": None})
add("_warn_liveness_config_disabled", {"key": "other_knob", "raw": None},
    {"name": "D2"}, io={"logger.warning": None})
add("_warn_liveness_config_disabled", {"key": "other_knob", "raw": 0},
    {"name": "D2"}, io={"logger.warning": None})
add("_warn_liveness_config_disabled", {"key": "other_knob", "raw": True},
    {"name": "D2"}, io={"logger.warning": None})
add("_warn_liveness_config_disabled", {"key": "other_knob", "raw": {"a": 1}},
    {"name": "D2"}, io={"logger.warning": None})
add("_warn_liveness_config_disabled", {"key": "k", "raw": "x"}, {"name": "D"}, fc=True)  # fail-closed

# ── _liveness_knob ──
def knob(raw, cast, warn=True, extra_io=None):
    io = {"_config_value": raw}
    if warn:
        io["_warn_liveness_config_disabled"] = None
    if extra_io:
        io.update(extra_io)
    add("_liveness_knob", {"key": "knob", "default": 1, "cast": cast, "env_key": None},
        {"name": "D"}, io)

knob("15", "int", warn=False)
knob("15.5", "int")
knob("15.5", "float", warn=False)
knob(True, "float")
knob("nan", "float")
knob("-3", "int")
knob("0", "int", warn=False)
knob(None, "float")
knob("abc", "int")
knob("1e2", "float", warn=False)
knob(" 30 ", "int", warn=False)
knob("1_0", "int", warn=False)
add("_liveness_knob", {"key": "k", "default": 1, "cast": "int", "env_key": None}, {"name": "D"}, fc=True)
add("_liveness_knob", {"key": "k", "default": 1, "cast": "int", "env_key": None}, {"name": "D"},
    io={"_config_value": raise_script("SystemExit", "bye")})
add("_liveness_knob", {"key": "k", "default": 1, "cast": "int", "env_key": None}, {"name": "D"},
    io={"_config_value": "abc", "_warn_liveness_config_disabled": raise_script("ValueError", "w")})

# ── _finite_positive_config_float / _config_int ──
for fn, cast in (("_finite_positive_config_float", "float"), ("_config_int", "int")):
    add(fn, {"key": "k", "default": 1, "env_key": None}, io={"_liveness_knob": 12.5})
    add(fn, {"key": "k", "default": 1, "env_key": None}, io={"_liveness_knob": 0})
    add(fn, {"key": "k", "default": 1, "env_key": "E"}, io={"_liveness_knob": None})
    add(fn, {"key": "k", "default": 1, "env_key": None}, fc=True)  # fail-closed
    add(fn, {"key": "k", "default": 1, "env_key": None},
        io={"_liveness_knob": raise_script("KeyboardInterrupt", "ki")})

# ── _classify_connect_exception ──
add("_classify_connect_exception", {"error": exc("LoginFailure", "bad token"), "shown": None})
add("_classify_connect_exception", {"error": exc("LoginFailure", "bad"), "shown": "proxy secret"})
add("_classify_connect_exception", {"error": exc("MyLogin", "x", "LoginFailure")},
    state={"discord_available": True})
add("_classify_connect_exception", {"error": exc("MyLogin", "x", "LoginFailure")},
    state={"discord_available": False})
add("_classify_connect_exception", {"error": exc("PrivilegedIntentsRequired", "x")},
    state={"_allowed_user_ids": ["alice"], "_allowed_role_ids": None},
    io={"_needs_server_members_intent": True, "_format_privileged_intents_guidance": "GUIDANCE"})
add("_classify_connect_exception", {"error": exc("X", "x", "PrivilegedIntentsRequired")},
    state={"discord_available": True, "_allowed_user_ids": None, "_allowed_role_ids": ["r"]},
    io={"_needs_server_members_intent": False, "_format_privileged_intents_guidance": "G2"})
add("_classify_connect_exception", {"error": exc("RuntimeError", "boom"), "shown": None})
add("_classify_connect_exception", {"error": exc("PrivilegedIntentsRequired", "x")},
    io={"_needs_server_members_intent": raise_script("SystemExit", "bye")})
add("_classify_connect_exception", {"error": exc("PrivilegedIntentsRequired", "x")}, fc=True)  # fail-closed

# ── _thread_id_and_chat_for_channel ──
add("_thread_id_and_chat_for_channel", {"channel": None})
add("_thread_id_and_chat_for_channel", {"channel": {}})
add("_thread_id_and_chat_for_channel", {"channel": {"id": 123}}, state={"discord_available": False})
add("_thread_id_and_chat_for_channel", {"channel": {"id": 123, "is_thread": True}},
    state={"discord_available": False})
add("_thread_id_and_chat_for_channel", {"channel": {"id": 123, "is_thread": True}},
    state={"discord_available": True})
add("_thread_id_and_chat_for_channel", {"channel": {"id": "c9", "is_thread": False}},
    state={"discord_available": True})
add("_thread_id_and_chat_for_channel", {"channel": {"id": None}})

# ── _source_for_platform_event ──
SK = {"chat_id": "c", "user_id": "u", "user_name": "n", "thread_id": None, "guild_id": "g"}
add("_source_for_platform_event", dict(SK), io={"build_source": {"ok": True}})
add("_source_for_platform_event", dict(SK, thread_id="t"), io={"build_source": {"ok": True}})
add("_source_for_platform_event", dict(SK, user_id=""), io={"build_source": {"ok": True}})
add("_source_for_platform_event", dict(SK, chat_id=None), io={"build_source": {"ok": True}})
add("_source_for_platform_event", {k: v for k, v in SK.items() if k != "guild_id"},
    io={"build_source": {"ok": True}})
add("_source_for_platform_event", {"chat_id": "c"}, io={"build_source": {"ok": True}})
add("_source_for_platform_event", dict(SK, zzz=1), io={"build_source": {"ok": True}})
add("_source_for_platform_event", dict(SK), fc=True)  # fail-closed
add("_source_for_platform_event", dict(SK), io={"build_source": raise_script("SystemExit", "bye")})
add("_source_for_platform_event", dict(SK, message_id="m1"), io={"build_source": None})

# ── _platform_events_subscribed ──
add("_platform_events_subscribed", io={"has_hook": True})
add("_platform_events_subscribed", io={"has_hook": False})
add("_platform_events_subscribed", io={"has_hook": 1})
add("_platform_events_subscribed", io={"has_hook": None})
add("_platform_events_subscribed", io={"has_hook": raise_script("ValueError", "x")})
add("_platform_events_subscribed", io={"has_hook": raise_script("SystemExit", "bye")})
add("_platform_events_subscribed", state={"lifecycle_import_ok": False})
add("_platform_events_subscribed", fc=True)  # fail-closed -> except -> False

# ── _fire_platform_event ──
EV = {"platform": "discord", "event_type": "message_edited", "payload": {"x": 1}}
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": None})
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": "seam", "name": "D"}, io={"platform_event_handler": None})
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": "seam", "name": "D"},
    io={"platform_event_handler": raise_script("ValueError", "x"), "logger.debug": None})
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": "seam", "name": "D"},
    io={"platform_event_handler": raise_script("SystemExit", "bye")})
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": 5, "name": "D"}, io={"logger.debug": None})
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": "seam", "name": "D"},
    io={"platform_event_handler": raise_script("ValueError", "x")}, fc=True)  # logger fail-closed
add("_fire_platform_event", {"event": EV, "source": {"s": 1}},
    state={"_platform_event_handler": "seam", "name": "D"},
    io={"platform_event_handler": {"x": 1}})

# ── _emit_platform_event ──
PAY = {"chat_id": "c", "message_id": "m"}
SKW = {"chat_id": "c", "user_id": "u", "user_name": "n", "thread_id": None, "guild_id": "g"}
SRC = {"source": True}

_OMIT = object()


def emit(build_script=_OMIT, sub=True, source=_OMIT, fire=_OMIT, log=_OMIT, extra=None):
    io = {"_platform_events_subscribed": sub}
    if build_script is not _OMIT:
        io["build"] = build_script
    if source is not _OMIT:
        io["_source_for_platform_event"] = source
    if fire is not _OMIT:
        io["_fire_platform_event"] = fire
    if log is not _OMIT:
        io["logger.debug"] = log
    if extra:
        io.update(extra)
    add("_emit_platform_event", {"event_type": "message_edited"}, {"name": "D"}, io)

emit(None, sub=False)
emit([None])
emit([[PAY, SKW]], source=SRC, fire=None)
emit([[PAY, {k: v for k, v in SKW.items() if k != "user_id"}]], log=None)
emit([[PAY, dict(SKW, zzz=1)]], log=None)
emit([[PAY, "notdict"]], log=None)
emit([[1, 2, 3]], log=None)
emit([[1]], log=None)
emit([5], log=None)
emit(["ab"], log=None)
emit([{"k1": 1, "k2": 2}], log=None)
emit([raise_script("ValueError", "x")], log=None)
emit([raise_script("SystemExit", "bye")])
emit([[PAY, SKW]], source=raise_script("ValueError", "x"), log=None)
emit([[PAY, SKW]], source=SRC, fire=raise_script("RuntimeError", "f"))
emit(_OMIT)  # subscribed fail-closed
CASES[-1]["fc"] = True
emit(_OMIT, sub=True, log=None)  # build fail-closed -> normalize error
CASES[-1]["fc"] = True

# ── _message_event_parts ──
def msg_case(message, parts, extra=None, automated=False, io_extra=None):
    io = {"is_automated_message": automated, "_thread_id_and_chat_for_channel": [parts]}
    if extra is not None:
        io["extra_payload"] = extra
    if io_extra:
        io.update(io_extra)
    add("_message_event_parts", {"message": message}, {"name": "D"}, io)

M1 = {"id": "m1", "author": {"id": "u1", "display_name": "Alice"},
      "channel": {"id": "c1"}, "guild": {"id": "g1"}}
msg_case(M1, [None, "c1"], extra={"text": "hi"})
msg_case(M1, ["t1", "c1"], extra={})
msg_case(M1, [None, None], extra={})
msg_case({"id": None, "channel": {}}, [None, "c1"], extra={})
msg_case({"id": "m1", "author": None, "channel": {}}, [None, "c1"], extra={})
msg_case({"id": "m1", "author": {"id": 0}, "channel": {}}, [None, "c1"], extra={})
msg_case({"id": "m1", "channel": {}, "guild": {}}, [None, "c1"], extra={})
msg_case({"id": "m1", "channel": {}, "guild": None}, [None, "c1"], extra={})
msg_case({"id": "m1", "channel": {}, "guild": {"id": None}}, [None, "c1"], extra={})
msg_case(M1, [None, "c1"], extra=[1])
msg_case(M1, [None, "c1"], extra=raise_script("SystemExit", "bye"))
msg_case(M1, [None, "c1"], extra={}, io_extra={"is_automated_message": None})
add("_message_event_parts", {"message": M1}, {"name": "D"},
    io={"_thread_id_and_chat_for_channel": [[None, "c1"]], "extra_payload": {}}, fc=True)
msg_case(M1, [1, 2, 3], extra={})
msg_case(dict(M1, id="m" * 200), [None, "c" * 200], extra={})
msg_case(M1, [None, "c1"], extra={}, automated=True)

# ── _thread_event_parts ──
T1 = {"id": "t1", "owner_id": "o1", "parent_id": "p1", "guild": {"id": "g1"}}
add("_thread_event_parts", {"thread": None}, io={"extra_payload": {}})
add("_thread_event_parts", {"thread": T1}, io={"extra_payload": {"name": "n"}})
add("_thread_event_parts", {"thread": dict(T1, owner_id=None)}, io={"extra_payload": {}})
add("_thread_event_parts", {"thread": dict(T1, parent_id=None)}, io={"extra_payload": {}})
add("_thread_event_parts", {"thread": {"id": "t1"}}, io={"extra_payload": {}})
add("_thread_event_parts", {"thread": T1}, io={"extra_payload": 5})
add("_thread_event_parts", {"thread": T1}, io={"extra_payload": raise_script("KeyboardInterrupt", "ki")})
add("_thread_event_parts", {"thread": dict(T1, id="t" * 200)}, io={"extra_payload": {}})

# ── _command_sync_skip_reason ──
def skip(state_entry, now=1000, key="app1", read=None, key_script="app1", extra=None):
    io = {"_read_command_sync_state": state_entry if read is None else read,
          "_command_sync_state_key": key_script, "time.time": now}
    if extra:
        io.update(extra)
    add("_command_sync_skip_reason", {"app_id": 123, "fingerprint": "fp"}, {"name": "D"}, io)

skip({})
skip({"app1": "notdict"})
skip({"app1": {"retry_after_until": 1010}})
skip({"app1": {"retry_after_until": 1000.5}})
skip({"app1": {"retry_after_until": "1010"}})
skip({"app1": {"fingerprint": "fp", "last_success_at": 900, "last_attempt_at": 800}})
skip({"app1": {"fingerprint": "fp", "last_success_at": 0, "last_attempt_at": 800}})
skip({"app1": {"fingerprint": "fp", "last_success_at": 700, "last_attempt_at": 800}})
skip({"app1": {"fingerprint": "other", "last_success_at": 900, "last_attempt_at": 800}})
skip({"app1": {"retry_after_until": "abc"}})
skip({"app1": {}}, now="x")
skip(None)
skip({"app1": {"retry_after_until": [1]}})
add("_command_sync_skip_reason", {"app_id": 123, "fingerprint": "fp"}, {"name": "D"},
    io={"_command_sync_state_key": "app1", "time.time": 1000}, fc=True)  # read fail-closed
skip({"app1": {"retry_after_until": 999, "fingerprint": "fp", "last_success_at": 900,
               "last_attempt_at": 800}})

# ── _extract_discord_retry_after ──
def retry(attrs, parse=_OMIT):
    io = {} if parse is _OMIT else {"parse_retry_after_seconds": parse}
    add("_extract_discord_retry_after", {"exc": exc("HTTPException", "429", attrs=attrs)}, io=io)

retry({"retry_after": 5}, 5)
retry({"retry_after": 0.2}, 0.2)
retry({"retry_after": "x"}, None)
retry({"response": {"headers": {"Retry-After": "3"}}}, 3)
retry({"response": {"headers": {}}}, None)
retry({"response": {"headers": {"X-RateLimit-Reset-After": "2"}}}, [None, 2])
retry({"response": {"headers": {"X-RateLimit-Reset-After": "2"}}}, [None, None])
retry({"response": {"headers": "str"}}, None)
retry({"response": {"headers": {"X-RateLimit-Reset-After": "2"}}},
      [None, raise_script("ValueError", "x")])
retry({"retry_after": 5}, raise_script("SystemExit", "bye"))
retry({"retry_after": 5}, "x")
retry({"retry_after": 5})  # fail-closed
CASES[-1]["fc"] = True

# ── _is_discord_rate_limit ──
add("_is_discord_rate_limit", {"exc": exc("X", "", "RateLimited")}, state={"discord_available": True})
add("_is_discord_rate_limit", {"exc": exc("X", "", "HTTPException", {"status": 429})},
    state={"discord_available": True})
add("_is_discord_rate_limit", {"exc": exc("X", "", "HTTPException", {"status": 400})},
    state={"discord_available": True})
add("_is_discord_rate_limit", {"exc": exc("MyRateLimited", "", None, {"retry_after": 5})})
add("_is_discord_rate_limit", {"exc": exc("rate_limit_error", "", None, {"retry_after": None})})
add("_is_discord_rate_limit", {"exc": exc("X", "", None, {"response": {"status": 429}})})
add("_is_discord_rate_limit", {"exc": exc("X", "", None, {"response": {"status": 0, "status_code": 429}})})
add("_is_discord_rate_limit", {"exc": exc("X", "", None, {"response": {"status": 404}})})
add("_is_discord_rate_limit", {"exc": exc("RateLimited", "", "RateLimited", {"retry_after": None})},
    state={"discord_available": False})

# ── _is_discord_unknown_interaction ──
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"code": 10062})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"code": "10062"})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"data": {"code": 10062}})})
add("_is_discord_unknown_interaction",
    {"exc": exc("X", "Unknown Interaction", None, {"status": 404})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "other", None, {"status": 404})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"code": 10062, "status": 404})})
add("_is_discord_unknown_interaction",
    {"exc": exc("X", "unknown interaction", None, {"code": "abc", "status": "404"})})
add("_is_discord_unknown_interaction",
    {"exc": exc("X", "unknown interaction", None, {"response": {"status_code": 404}})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"code": 10008})})
add("_is_discord_unknown_interaction", {"exc": exc("X", "", None, {"code": True})})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"FATAL: funcs with <5 cases: {thin}")


if __name__ == "__main__":
    main()