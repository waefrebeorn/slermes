#!/usr/bin/env python3
"""Generate tests/fixtures/photon_adapter_flow2.jsonl for the Photon adapter
FLOW remainder port (src/gateway/platforms/photon_adapter_flow2.c).

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list RESULTS one level deeper), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides (fc=True marks an
intentional gap). Every ported function gets >= 5 cases; error paths,
BaseException propagation and fail-closed scripts are covered per flow.

Corpus restrictions mirror the C port header: json.loads slots carry canonical
JSON or forms both parsers reject; str() slots carry scalars; lower()/name text
is ASCII; home paths are simple POSIX; int() pid strings have no underscores;
mutated state keys are present in input state; task entries are null or id str.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "photon_adapter_flow2.jsonl")
CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def rs(kind, msg, **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def ret(v):
    return {"__script__": True, "ret": v}


REC = "photon-sidecar.json"

# ── _runtime_record_path / _legacy_runtime_record_path ──
for h in ("/h", "/", "//", "/a/b", "h", "/h/"):
    add("_runtime_record_path", {}, io={"get_hermes_home": h})
    add("_legacy_runtime_record_path", {}, io={"get_hermes_home": h})

# ── _current_runtime_record_path ──
add("_current_runtime_record_path", {}, io={"get_hermes_home": "/h", "unlink": None})
add("_current_runtime_record_path", {}, io={"get_hermes_home": "/h", "unlink": rs("OSError", "gone")})
add("_current_runtime_record_path", {}, io={"get_hermes_home": "/h", "unlink": rs("RuntimeError", "boom")})
add("_current_runtime_record_path", {}, io={"get_hermes_home": [rs("OSError", "x"), "/h"]})
add("_current_runtime_record_path", {}, io={"get_hermes_home": "/", "unlink": None})
add("_current_runtime_record_path", {}, fc=True)

# ── _write_runtime_record ──
WIO = {"get_hermes_home": "/h", "unlink": None, "atomic_json_write": None}
add("_write_runtime_record", {"port": 8789, "token": "t", "pid": 1}, io=WIO)
add("_write_runtime_record", {"port": 9000, "token": "x", "pid": 2},
    io={"get_hermes_home": "/h", "unlink": None, "atomic_json_write": rs("RuntimeError", "disk full")})
add("_write_runtime_record", {"port": 8789, "token": "t", "pid": 1},
    io={"get_hermes_home": "/h", "unlink": None, "atomic_json_write": rs("SystemExit", "bye")})
add("_write_runtime_record", {"port": 8789, "token": "t", "pid": 1},
    io={"get_hermes_home": "/h", "unlink": rs("RuntimeError", "boom")})
add("_write_runtime_record", {"port": 8789, "token": "t", "pid": 1},
    io={"get_hermes_home": "/h", "unlink": rs("OSError", "x"), "atomic_json_write": None})
add("_write_runtime_record", {"port": 8789, "token": "t", "pid": 1}, fc=True)

# ── _read_runtime_record ──
def rio(text):
    return {"get_hermes_home": "/h", "unlink": None, "read_text": text}
add("_read_runtime_record", {}, io=rio('{"port":8789,"token":"t","pid":1}'))
add("_read_runtime_record", {}, io=rio('[1,2]'))
add("_read_runtime_record", {}, io=rio('null'))
add("_read_runtime_record", {}, io=rio('notjson'))
add("_read_runtime_record", {}, io=rio(''))
add("_read_runtime_record", {}, io=rio('{"a":1,}'))
add("_read_runtime_record", {}, io={"get_hermes_home": "/h", "unlink": None, "read_text": rs("OSError", "no")})
add("_read_runtime_record", {}, io={"get_hermes_home": "/h", "unlink": None, "read_text": rs("RuntimeError", "boom")})
add("_read_runtime_record", {}, fc=True)

# ── _delete_runtime_record ──
add("_delete_runtime_record", {}, io={"get_hermes_home": "/h", "unlink": None})
add("_delete_runtime_record", {}, io={"get_hermes_home": "/h", "unlink": rs("OSError", "x")})
add("_delete_runtime_record", {}, io={"get_hermes_home": "/h", "unlink": rs("RuntimeError", "boom")})
add("_delete_runtime_record", {}, io={"get_hermes_home": rs("OSError", "x")})
add("_delete_runtime_record", {}, io={"get_hermes_home": "//", "unlink": None})
add("_delete_runtime_record", {}, fc=True)

# ── _sidecar_pid_alive ──
add("_sidecar_pid_alive", {"pid": 123}, io={"pid_exists": True})
add("_sidecar_pid_alive", {"pid": 0}, io={})
add("_sidecar_pid_alive", {"pid": -5}, io={})
add("_sidecar_pid_alive", {"pid": "abc"}, io={})
add("_sidecar_pid_alive", {"pid": None}, io={})
add("_sidecar_pid_alive", {"pid": " 12 "}, io={"pid_exists": False})
add("_sidecar_pid_alive", {"pid": True}, io={"pid_exists": True})
add("_sidecar_pid_alive", {"pid": 123, "os_name": "nt"}, io={"pid_exists": rs("RuntimeError", "x")})
add("_sidecar_pid_alive", {"pid": 123, "os_name": "posix"},
    io={"pid_exists": rs("RuntimeError", "x"), "os.kill": None})
add("_sidecar_pid_alive", {"pid": 123, "os_name": "posix"},
    io={"pid_exists": rs("RuntimeError", "x"), "os.kill": rs("PermissionError", "denied")})
add("_sidecar_pid_alive", {"pid": 123, "os_name": "posix"},
    io={"pid_exists": rs("RuntimeError", "x"), "os.kill": rs("OSError", "no proc")})
add("_sidecar_pid_alive", {"pid": 123}, io={"pid_exists": rs("SystemExit", "bye")})

# ── _sidecar_error_from_response ──
add("_sidecar_error_from_response", {"path": "/send", "status_code": 500,
    "text": '{"error":"boom","error_class":"sidecar_error","retryable":true}'})
add("_sidecar_error_from_response", {"path": "/send", "status_code": 403,
    "text": '{"error_class":"target_not_allowed","error":"raw"}'})
add("_sidecar_error_from_response", {"path": "/x", "status_code": 502, "text": "plain prose"})
add("_sidecar_error_from_response", {"path": "/x", "status_code": 500, "text": ""})
add("_sidecar_error_from_response", {"path": "/x", "status_code": 500, "text": "ignored",
    "data": {"error": "direct", "retryable": False}})
add("_sidecar_error_from_response", {"path": "/x", "status_code": 500, "text": '{"error":123}'})
add("_sidecar_error_from_response", {"path": "/x", "status_code": 500, "text": '{"retryable":false}'})

# ── _is_timeout_error ──
for k in ("TimeoutError", "asyncio.TimeoutError", "ReadTimeout", "ConnectError",
          "ValueError", "MyTimeout", "PoolTimeout"):
    add("_is_timeout_error", {"exc": {"type": k, "message": "m"}})
add("_is_timeout_error", {"exc": {"type": "TimeoutException", "message": "m"}, "httpx_available": False})
add("_is_timeout_error", {"exc": {"type": "ConnectTimeout", "message": "m"}, "httpx_available": True})

# ── validate_config / is_connected ──
def vc_cases(fn):
    add(fn, {"cfg": {"extra": {"project_id": "p", "project_secret": "s"}}}, io={})
    add(fn, {"cfg": {"extra": {"project_id": "p"}}}, io={"get_scoped_secret": "s"})
    add(fn, {"cfg": {"extra": {}}}, io={"get_scoped_secret": None, "load_project_credentials": [[None, None]]})
    add(fn, {"cfg": {"extra": {}}}, io={"get_scoped_secret": ["p", "s"]})
    add(fn, {"cfg": {"extra": {"project_id": "p", "project_secret": ""}}}, io={"get_scoped_secret": "s"})
    add(fn, {"cfg": {"extra": {}}}, io={"get_scoped_secret": None, "load_project_credentials": [["id", "sec"]]})
    add(fn, {"cfg": {"extra": None}}, io={"get_scoped_secret": None, "load_project_credentials": [[None, "sec"]]})
    add(fn, {"cfg": {"extra": {}}}, io={"get_scoped_secret": rs("RuntimeError", "x")})
for fn in ("validate_config", "is_connected"):
    vc_cases(fn)

# ── _env_enablement ──
add("_env_enablement", {}, io={"load_project_credentials": [["id", "sec"]],
    "seed_extra_from_env": {"home_channel": {"chat_id": "+1555", "name": "Home"}}})
add("_env_enablement", {}, io={"load_project_credentials": [[None, "sec"]]})
add("_env_enablement", {}, io={"load_project_credentials": [["id", None]]})
add("_env_enablement", {}, io={"load_project_credentials": [["id", "sec"]], "seed_extra_from_env": {}})
add("_env_enablement", {}, io={"load_project_credentials": rs("RuntimeError", "x")})
add("_env_enablement", {}, io={"load_project_credentials": [["id", "sec"]], "seed_extra_from_env": rs("SystemExit", "bye")})

# ── _sent_text_key ──
add("_sent_text_key", {"chat_id": "any;-;+15551234567"}, io={"normalize_chat_key": "+15551234567"})
add("_sent_text_key", {"chat_id": "+15551234567"}, io={"normalize_chat_key": "+15551234567"})
add("_sent_text_key", {"chat_id": None}, io={})
add("_sent_text_key", {"chat_id": ""}, io={})
add("_sent_text_key", {"chat_id": "guid"}, io={"normalize_chat_key": "guid"})
add("_sent_text_key", {"chat_id": "c"}, io={"normalize_chat_key": rs("RuntimeError", "x")})

# ── _record_sent_text_async ──
add("_record_sent_text_async", {"chat_id": "c", "message_id": "m", "text": "t"},
    io={"normalize_chat_key": "c", "rich_sent_store.record_async": None})
add("_record_sent_text_async", {"chat_id": None, "message_id": "m", "text": "t"},
    io={"rich_sent_store.record_async": None})
add("_record_sent_text_async", {"chat_id": "c", "message_id": "m", "text": "t"},
    io={"normalize_chat_key": "c", "rich_sent_store.record_async": rs("RuntimeError", "x")})
add("_record_sent_text_async", {"chat_id": "c", "message_id": "m", "text": "t"},
    io={"normalize_chat_key": "c", "rich_sent_store.record_async": rs("SystemExit", "bye")})
add("_record_sent_text_async", {"chat_id": "c", "message_id": "m", "text": "t"},
    io={"normalize_chat_key": rs("RuntimeError", "x")})
add("_record_sent_text_async", {"chat_id": "c", "message_id": "m", "text": "t"}, fc=True)

# ── _lookup_sent_text ──
add("_lookup_sent_text", {"chat_id": "c", "message_id": "m"},
    io={"normalize_chat_key": "c", "rich_sent_store.lookup": "stored"})
add("_lookup_sent_text", {"chat_id": "c", "message_id": "m"},
    io={"normalize_chat_key": "c", "rich_sent_store.lookup": None})
add("_lookup_sent_text", {"chat_id": "c", "message_id": "m"},
    io={"normalize_chat_key": "c", "rich_sent_store.lookup": rs("RuntimeError", "x")})
add("_lookup_sent_text", {"chat_id": "c", "message_id": "m"},
    io={"normalize_chat_key": "c", "rich_sent_store.lookup": rs("SystemExit", "bye")})
add("_lookup_sent_text", {"chat_id": None, "message_id": "m"}, io={"rich_sent_store.lookup": None})

# ── _guess_mime ──
add("_guess_mime", {"path": "x.png"}, io={"guess_type": [["image/png", None]]})
add("_guess_mime", {"path": "x.bin"}, io={"guess_type": [[None, None]]})
add("_guess_mime", {"path": "x"}, io={"guess_type": [["", None]]})
add("_guess_mime", {"path": "x.png"}, io={"guess_type": rs("RuntimeError", "x")})
add("_guess_mime", {"path": "a.caf"}, io={"guess_type": [["audio/x-caf", None]]})

# ── _standalone_error ──
SE = {"success": False, "error": "redacted"}
add("_standalone_error", {"resp": {"status_code": 500, "text": "boom"}},
    io={"resp.json": {"error_class": "sidecar_error"}, "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 403, "text": "x"}},
    io={"resp.json": {"error_class": "target_not_allowed"}, "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 503, "text": "unavailable"}},
    io={"resp.json": {}, "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 200, "text": ""}},
    io={"resp.json": {"error": "bad"}, "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 200, "text": ""}},
    io={"resp.json": {}, "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 200, "text": ""}},
    io={"resp.json": rs("RuntimeError", "x"), "send_error": SE})
add("_standalone_error", {"resp": {"status_code": 200, "text": ""}},
    io={"resp.json": rs("SystemExit", "bye")})

# ── _standalone_token_from_record ──
def tio(text, extra=None):
    io = {"get_hermes_home": "/h", "unlink": None, "read_text": text}
    io.update(extra or {})
    return io
add("_standalone_token_from_record", {"port": 8789},
    io=tio('{"token":"t","pid":123,"port":9000}', {"pid_exists": True, "coerce_port": 9000}))
add("_standalone_token_from_record", {"port": 8789},
    io=tio('{"token":"t","pid":123}', {"pid_exists": False}))
add("_standalone_token_from_record", {"port": 8789}, io=tio('null'))
add("_standalone_token_from_record", {"port": 8789}, io=tio('{}'))
add("_standalone_token_from_record", {"port": 8789}, io=tio('{"token":""}'))
add("_standalone_token_from_record", {"port": 8789},
    io={"get_hermes_home": "/h", "unlink": None, "read_text": rs("OSError", "x")})
add("_standalone_token_from_record", {"port": 9000},
    io=tio('{"token":"t","pid":123,"port":"bad"}', {"pid_exists": True, "coerce_port": 9000}))

# ── _sidecar_url ──
for path in ("/inbound", "/healthz", "/send", "", "/probe"):
    add("_sidecar_url", {"path": path}, {"_sidecar_bind": "127.0.0.1", "_sidecar_port": 8789})
add("_sidecar_url", {"path": "/x"}, {"_sidecar_bind": "0.0.0.0", "_sidecar_port": 9000})

# ── _sidecar_headers ──
for t in ("tok", "", "abc123", "x-y-z", "T"):
    add("_sidecar_headers", {}, {"_sidecar_token": t})

# ── _note_upstream_activity ──
for now, fails in ((100.0, 3), (0.0, 0), (55.5, 1), (1000.0, 9), (7.0, 2)):
    add("_note_upstream_activity", {}, {"_last_upstream_activity": 0.0, "_probe_failures": fails},
        {"time.monotonic": now})
add("_note_upstream_activity", {}, {"_last_upstream_activity": 0.0, "_probe_failures": 0},
    {"time.monotonic": rs("SystemExit", "bye")})

# ── _cancel_pending_fffc ──
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {"c": [10, "t1"]}},
    {"task.done": False, "task.cancel": None})
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {"c": [10, "t1"]}},
    {"task.done": True})
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {}}, {})
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {"c": [10, None]}}, {})
add("_cancel_pending_fffc", {"chat_key": "other"}, {"_pending_fffc": {"c": [10, "t1"]}}, {})
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {"c": [10, "t1"]}},
    {"task.done": rs("RuntimeError", "x")})
add("_cancel_pending_fffc", {"chat_key": "c"}, {"_pending_fffc": {"c": [10, "t1"], "d": [20, "t2"]}},
    {"task.done": False, "task.cancel": None})

# ── _gate_stt_chat ──
GST = {"require_mention": True, "config": {"extra": {}}}
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "group"}, "content": {"type": "voice"}}},
    GST, {"reply_inner": {"type": "voice"}, "carries_voice": True, "voice_mention_gate_enabled": True})
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "dm"}, "content": {"type": "voice"}}},
    GST, {"reply_inner": {"type": "voice"}})
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "group"}, "content": {"type": "voice"}}},
    {"require_mention": False, "config": {"extra": {}}}, {"reply_inner": {"type": "voice"}})
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "group"}, "content": {"type": "text"}}},
    GST, {"reply_inner": {"type": "text"}, "carries_voice": False})
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "group"}, "content": {"type": "voice"}}},
    GST, {"reply_inner": {"type": "voice"}, "carries_voice": True, "voice_mention_gate_enabled": False})
add("_gate_stt_chat", {"event": {"space": {}, "content": {}}}, GST, {"reply_inner": {}})
add("_gate_stt_chat", {"event": {"space": {"id": "s", "type": "group"}, "content": {"type": "voice"}}},
    GST, {"reply_inner": rs("RuntimeError", "x")})

# ── _add_reaction / _remove_reaction ──
add("_add_reaction", {"chat_id": "c", "message_id": "m", "emoji": "x"}, io={"sidecar_try": True})
add("_add_reaction", {"chat_id": "c", "message_id": "m", "emoji": "x"}, io={"sidecar_try": False})
add("_add_reaction", {"chat_id": "c", "message_id": "m", "emoji": "x"}, io={"sidecar_try": rs("SystemExit", "bye")})
add("_add_reaction", {"chat_id": "c", "message_id": "m", "emoji": "x"}, fc=True)
add("_add_reaction", {"chat_id": "c", "message_id": "m", "emoji": "y"}, io={"sidecar_try": True})
add("_remove_reaction", {"chat_id": "c", "message_id": "m"}, io={"sidecar_try": True})
add("_remove_reaction", {"chat_id": "c", "message_id": "m"}, io={"sidecar_try": False})
add("_remove_reaction", {"chat_id": "c", "message_id": "m"}, io={"sidecar_try": rs("SystemExit", "bye")})
add("_remove_reaction", {"chat_id": "c", "message_id": "m"}, fc=True)
add("_remove_reaction", {"chat_id": "c", "message_id": "m2"}, io={"sidecar_try": True})

# ── add_reaction / remove_reaction ──
add("add_reaction", {"chat_id": "c", "emoji": "x", "message_id": "m1"}, {}, {"sidecar_try": True})
add("add_reaction", {"chat_id": "c", "emoji": "x", "message_id": None},
    {"_last_inbound_by_chat": {"c": "m2"}}, {"normalize_chat_key": "c", "sidecar_try": True})
add("add_reaction", {"chat_id": "c", "emoji": "x", "message_id": None},
    {"_last_inbound_by_chat": {}}, {"normalize_chat_key": "c"})
add("add_reaction", {"chat_id": "c", "emoji": "x", "message_id": "m1"}, {}, {"sidecar_try": False})
add("add_reaction", {"chat_id": "c", "emoji": "x", "message_id": "m1"}, {}, {"sidecar_try": rs("SystemExit", "bye")})
add("add_reaction", {"chat_id": "any;-;+15551234567", "emoji": "x", "message_id": None},
    {"_last_inbound_by_chat": {"+15551234567": "m3"}}, {"normalize_chat_key": "+15551234567", "sidecar_try": True})
add("remove_reaction", {"chat_id": "c", "message_id": "m1"}, {}, {"sidecar_try": True})
add("remove_reaction", {"chat_id": "c", "message_id": None},
    {"_last_inbound_by_chat": {"c": "m2"}}, {"normalize_chat_key": "c", "sidecar_try": True})
add("remove_reaction", {"chat_id": "c", "message_id": None}, {"_last_inbound_by_chat": {}}, {"normalize_chat_key": "c"})
add("remove_reaction", {"chat_id": "c", "message_id": "m1"}, {}, {"sidecar_try": False})
add("remove_reaction", {"chat_id": "c", "message_id": "m1"}, {}, {"sidecar_try": rs("SystemExit", "bye")})

# ── on_processing_start ──
add("on_processing_start", {"event": {"source": {"chat_id": "c"}, "message_id": "m"}}, {},
    {"reactions_enabled": True, "sidecar_try": True})
add("on_processing_start", {"event": {"source": {"chat_id": "c"}, "message_id": "m"}}, {},
    {"reactions_enabled": False})
add("on_processing_start", {"event": {"source": {"chat_id": None}, "message_id": "m"}}, {},
    {"reactions_enabled": True})
add("on_processing_start", {"event": {"source": {"chat_id": "c"}, "message_id": None}}, {},
    {"reactions_enabled": True})
add("on_processing_start", {"event": {"source": {"chat_id": "c"}, "message_id": "m"}}, {},
    {"reactions_enabled": rs("SystemExit", "bye")})
add("on_processing_start", {"event": {"source": {}, "message_id": "m"}}, {}, {"reactions_enabled": True})

# ── get_chat_info ──
for c in ("c", "+15551234567", "any;-;+15551234567", "", "guid-x"):
    add("get_chat_info", {"chat_id": c})

# ── format_message ──
add("format_message", {"content": "**hi**"}, io={"markdown_enabled": True})
add("format_message", {"content": "**hi**"}, io={"markdown_enabled": False, "strip_markdown": "hi"})
add("format_message", {"content": "see https://x.com"}, io={"markdown_enabled": False, "strip_markdown": "see https://x.com"})
add("format_message", {"content": "x"}, io={"markdown_enabled": rs("RuntimeError", "x")})
add("format_message", {"content": "x"}, io={"markdown_enabled": False, "strip_markdown": rs("RuntimeError", "x")})
add("format_message", {"content": None}, io={"markdown_enabled": True})

# ── send_typing ──
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, {"time.time": 100.0, "sidecar_try": True})
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {"c": 98.0}}, {"time.time": 100.0})
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {"c": 90.0}}, {"time.time": 100.0, "sidecar_try": True})
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, {"time.time": rs("RuntimeError", "x")})
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, {"time.time": 100.0, "sidecar_try": rs("SystemExit", "bye")})
add("send_typing", {"chat_id": "c"}, {"_typing_last_sent": {"c": 95.0}}, {"time.time": 100.0, "sidecar_try": True})

# ── stop_typing ──
add("stop_typing", {"chat_id": "c"}, {"_typing_last_sent": {"c": 100.0}}, {"sidecar_try": True})
add("stop_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, {"sidecar_try": True})
add("stop_typing", {"chat_id": "c"}, {"_typing_last_sent": {"c": 1.0}}, {"sidecar_try": False})
add("stop_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, {"sidecar_try": rs("SystemExit", "bye")})
add("stop_typing", {"chat_id": "c"}, {"_typing_last_sent": {}}, fc=True)

# ── _send_plain_fallback ──
SR = {"success": True, "message_id": "m1", "error": None, "raw_response": None,
      "retryable": False, "retry_after": None, "continuation_message_ids": [], "error_kind": None}
add("_send_plain_fallback", {"chat_id": "c", "content": "hi"}, {}, {"markdown_enabled": True, "sidecar_send": SR})
add("_send_plain_fallback", {"chat_id": "c", "content": "**hi**"}, {},
    {"markdown_enabled": False, "strip_markdown": "hi", "sidecar_send": SR})
add("_send_plain_fallback", {"chat_id": "c", "content": "abcdefghij"}, {"MAX_MESSAGE_LENGTH": 5},
    {"markdown_enabled": True, "sidecar_send": SR})
add("_send_plain_fallback", {"chat_id": "c", "content": "x"}, {}, {"markdown_enabled": rs("RuntimeError", "x")})
add("_send_plain_fallback", {"chat_id": "c", "content": "x"}, {},
    {"markdown_enabled": True, "sidecar_send": rs("SystemExit", "bye")})

# ── write ──
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
short = [fn for fn, n in counts.items() if n < 5]
if short:
    print(f"WARNING funcs with <5 cases: {short}")