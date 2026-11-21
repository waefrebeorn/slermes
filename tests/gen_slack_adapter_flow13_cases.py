#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow13.jsonl (>=6 cases per function).

Every case's ``in.io`` carries the FULL seam-name set (defaults overridden per
case), so the oracle's seam-name SET EQUALITY check holds by construction.
"""
import copy
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow13.jsonl")

DEFAULTS = {
    "build": {"__script__": True, "ret": ["fallback text", [{"type": "actions", "elements": []}]]},
    "build_source": {"chat_id": "C1", "chat_type": "dm"},
    "button": {"type": "button", "action_id": "x"},
    "chat_stopStream": {"ok": True},
    "clear_thread_status_quietly": None,
    "describe_slack_api_error": "detail text",
    "dm_target": "D123",
    "emit_media_warning": {"success": True, "error": None, "message_id": None},
    "exists": True,
    "files_info": {"ok": True, "file": {"id": "F1", "url": "https://x/f1"}},
    "format_chunks": {"__script__": True, "ret": ["chunk one"]},
    "gateway_trust_env": False,
    "get_client": {"id": "client"},
    "get_session_store": {"store": 1},
    "has_app": True,
    "has_app_token": True,
    "import_aiohttp": True,
    "is_running": True,
    "logger.debug": None,
    "logger.error": None,
    "logger.warning": None,
    "metadata_team_id": "T1",
    "new_session": {"session": 1},
    "note_attachment_failure": None,
    "parse_users_info": {"__script__": True, "ret": ["alice", False]},
    "post": {"status": 200},
    "post_ephemeral_fallback": {"success": False, "error": "fallback failed", "message_id": None},
    "post_interactive_blocks": {"ts": "171.100"},
    "read_error_text_limited": "error body",
    "resolve_thread_ts": "171.000",
    "seal_stream": None,
    "send_failure_notice": {"success": False, "error": "not found", "message_id": None},
    "send_interactive_prompt": {"success": True, "error": None, "message_id": "171.100"},
    "send_slash_ephemeral": {"success": True, "error": None, "message_id": None},
    "session_status_method": {"setter": 1},
    "set_status": None,
    "slack_json_post": {"ok": True, "channel": {"id": "D123"}},
    "standalone_proxy_kwargs": {"proxy": None},
    "start_socket_mode_handler": None,
    "stop_socket_mode_handler": None,
    "time.time": 1000.5,
    "to_thread": None,
    "trim_oldest_dict_entries": None,
    "trim_slack_dm_cache": None,
    "upload_with_retry": {"success": True, "error": None, "message_id": "f1"},
    "users_info_payload": {"user": {"id": "U1", "name": "alice"}},
    "workspace_message_marker": "T1|171.100",
}


def exc(kind, msg="boom"):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


CASES = []


def add(func, args=None, state=None, io=None, expect_raise=None):
    full = copy.deepcopy(DEFAULTS)
    for k, v in (io or {}).items():
        full[k] = copy.deepcopy(v)
    rec = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": full}}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


# ---- _resolve_file_stub ----
add("_resolve_file_stub", {"f": {}})
add("_resolve_file_stub", {"f": {"id": "F1"}})
add("_resolve_file_stub", {"f": {"id": "F1"}, "notices": [],
    "channel_id": "C1", "team_id": "T1"},
    io={"files_info": {"ok": False, "error": "not_found"}})
add("_resolve_file_stub", {"f": {"id": "F1"}, "notices": []},
    io={"files_info": exc("ValueError", "api down")})
add("_resolve_file_stub", {"f": {"id": "F1"}, "notices": []},
    io={"files_info": exc("SystemExit", "fatal")}, expect_raise="SystemExit")
add("_resolve_file_stub", {"f": {"id": "F1"}, "notices": []},
    io={"get_client": exc("ValueError", "no client")})
add("_resolve_file_stub", {"f": {"id": "F1"}},
    io={"files_info": {"ok": False, "error": "x"}})

# ---- _resolve_slack_user_dm ----
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"})
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"},
    state={"_slack_dm_cache": {"xoxb:U1": "Dcached"}})
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"}, io={"import_aiohttp": False})
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"},
    io={"slack_json_post": {"ok": False, "error": "missing_scope"}})
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"},
    io={"slack_json_post": exc("ValueError", "network")})
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"},
    io={"slack_json_post": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_resolve_slack_user_dm", {"token": "xoxb", "user_id": "U1"},
    io={"slack_json_post": {"ok": True, "channel": {}}})

# ---- _resolve_user_is_bot ----
add("_resolve_user_is_bot", {"user_id": ""})
add("_resolve_user_is_bot", {"user_id": "U1"},
    state={"_user_is_bot_cache": {"|U1": True}})
add("_resolve_user_is_bot", {"user_id": "U1"}, io={"has_app": False})
add("_resolve_user_is_bot", {"user_id": "U1", "chat_id": "C1"},
    state={"_channel_team": {"C1": "T9"}}, io={"users_info_payload": {}})
add("_resolve_user_is_bot", {"user_id": "U1"}, io={"parse_users_info": {"__script__": True, "ret": ["bob", True]}})
add("_resolve_user_is_bot", {"user_id": "U1"}, io={"parse_users_info": exc("ValueError", "bad")})
add("_resolve_user_is_bot", {"user_id": "U1"},
    io={"parse_users_info": exc("KeyboardInterrupt", "stop")}, expect_raise="KeyboardInterrupt")

# ---- _resolve_user_name ----
add("_resolve_user_name", {"user_id": ""})
add("_resolve_user_name", {"user_id": "U1"},
    state={"_user_name_cache": {"|U1": "cached-name"}})
add("_resolve_user_name", {"user_id": "U1"}, io={"has_app": False})
add("_resolve_user_name", {"user_id": "U1"}, io={"users_info_payload": {}})
add("_resolve_user_name", {"user_id": "U1"}, io={"parse_users_info": {"__script__": True, "ret": ["carol", False]}})
add("_resolve_user_name", {"user_id": "U1"}, io={"parse_users_info": exc("ValueError", "bad")})
add("_resolve_user_name", {"user_id": "U1"},
    io={"parse_users_info": exc("GeneratorExit", "gen")}, expect_raise="GeneratorExit")

# ---- _restart_socket_mode ----
add("_restart_socket_mode", {"reason": "ping"}, io={"is_running": False})
add("_restart_socket_mode", {"reason": "ping"}, io={"is_running": [True, False]})
add("_restart_socket_mode", {"reason": "ping"}, io={"has_app": False})
add("_restart_socket_mode", {"reason": "ping"}, io={"has_app_token": False})
add("_restart_socket_mode", {"reason": "ping"})
add("_restart_socket_mode", {"reason": "ping"},
    io={"start_socket_mode_handler": exc("ValueError", "start fail")})
add("_restart_socket_mode", {"reason": "ping"},
    io={"start_socket_mode_handler": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_restart_socket_mode", {"reason": "ping"},
    io={"is_running": [exc("ValueError", "x"), False]})

# ---- _seal_stale_streams ----
add("_seal_stale_streams")
add("_seal_stale_streams", state={"_STREAM_MAX_AGE_S": 100,
    "_active_streams": {"old": {"started": 10, "ts": "1"}, "new": {"started": 999, "ts": "2"}}})
add("_seal_stale_streams", state={"_STREAM_MAX_AGE_S": 100,
    "_active_streams": {"new": {"started": 9999, "ts": "2"}}})
add("_seal_stale_streams", state={"_STREAM_MAX_AGE_S": 100,
    "_active_streams": {"a": {"started": 1}, "b": {"started": 2}, "c": {"started": 9999}}})
add("_seal_stale_streams", io={"time.time": exc("ValueError", "clock")})
add("_seal_stale_streams", io={"time.time": exc("CancelledError", "cancel")}, expect_raise="CancelledError")
add("_seal_stale_streams", state={"_STREAM_MAX_AGE_S": 100,
    "_active_streams": {"old": {"started": 1}}},
    io={"seal_stream": exc("SystemExit", "seal halt")}, expect_raise="SystemExit")

# ---- _seal_stream ----
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}})
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}, "delta": " tail "})
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}, "delta": "   "})
add("_seal_stream", {"key": ["", "C1", "k"], "stream": {"ts": "1"}},
    io={"chat_stopStream": exc("ValueError", "nope")})
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}},
    io={"get_client": exc("ValueError", "no client")})
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}},
    io={"get_client": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_seal_stream", {"key": ["T1", "C1", "k"], "stream": {"ts": "1"}},
    io={"chat_stopStream": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _seed_dm_session ----
add("_seed_dm_session", {"metadata": {}})
add("_seed_dm_session", {"metadata": {"channel_id": "C1"}})
add("_seed_dm_session", {"metadata": {"channel_id": "C1", "user_id": "U1"}, "thread_ts": "171.0"})
add("_seed_dm_session", {"metadata": {"channel_id": "C1", "user_id": "U1", "team_id": "T1"},
    "fail_log": ["[Slack] failed %s", "x"]},
    state={"_channel_name_cache": {"T1|C1": "general"}})
add("_seed_dm_session", {"metadata": {"channel_id": "C1", "user_id": "U1"}},
    io={"to_thread": exc("ValueError", "store fail")})
add("_seed_dm_session", {"metadata": {"channel_id": "C1", "user_id": "U1"}},
    io={"to_thread": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_seed_dm_session", {"metadata": {"channel_id": "C1", "user_id": "U1"}},
    io={"build_source": exc("ValueError", "build fail")})

# ---- _send_exec_approval_prompt ----
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1"}})
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1", "request_id": "req1",
    "session_key": "sk", "actions": [["Approve", "approve", "primary"], ["Deny", "deny", "danger"]]},
    "resolved_max": 5000}, state={"_EA_ACTION_IDS": {"approve": "ea_approve", "deny": "ea_deny"}})
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1", "request_id": "req1",
    "actions": [["Yes", "approve", "primary"]]}},
    io={"send_interactive_prompt": {"success": False, "error": "no", "message_id": None}})
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1",
    "actions": [["Yes", "approve", "primary"]]}},
    io={"button": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1", "request_id": "req1"}},
    io={"metadata_team_id": exc("SystemExit", "halt")})
add("_send_exec_approval_prompt", {"prompt": {"chat_id": "C1"}},
    io={"send_interactive_prompt": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _send_failure_notice ----
for i in range(6):
    add("_send_failure_notice", {"chat_id": "C%d" % i, "caption": "cap%d" % i,
        "notice": "failed %d" % i, "reply_to": None if i % 2 else "171.0",
        "metadata": {"team_id": "T%d" % i}})
add("_send_failure_notice", {"chat_id": "C1", "notice": "x"},
    io={"emit_media_warning": exc("SystemExit", "halt")}, expect_raise="SystemExit")

# ---- _send_interactive_prompt ----
add("_send_interactive_prompt", {"chat_id": "C1", "label": "send_x"}, io={"has_app": False})
add("_send_interactive_prompt", {"chat_id": "C1"},
    io={"dm_target": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_interactive_prompt", {"chat_id": "C1"}, io={"build": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_interactive_prompt", {"chat_id": "C1", "label": "send_x"},
    io={"post_interactive_blocks": exc("ValueError", "post fail")})
add("_send_interactive_prompt", {"chat_id": "C1", "label": "send_x"},
    io={"post_interactive_blocks": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_interactive_prompt", {"chat_id": "C1", "resolved": {"old": True}, "resolved_max": 5000,
    "team_scoped_key": True, "label": "send_x", "metadata": {"team_id": "T1"}})
add("_send_interactive_prompt", {"chat_id": "C1", "resolved": {}, "team_scoped_key": False,
    "label": "send_x", "metadata": {}})
add("_send_interactive_prompt", {"chat_id": "C1", "label": "send_x"},
    io={"post_interactive_blocks": {"ts": ""}})

# ---- _send_local_file ----
add("_send_local_file", {"chat_id": "C1"}, io={"has_app": False})
add("_send_local_file", {"chat_id": "C1", "file_path": "/no", "not_found_error": "missing"})
add("_send_local_file", {"chat_id": "C1", "file_path": "/tmp/v.mp4", "kind": "video",
    "filename": "v.mp4", "caption": "cap", "reply_to": "171.0"})
add("_send_local_file", {"chat_id": "C1", "file_path": "/tmp/d.pdf", "kind": "document",
    "failure_notice": "upload failed"},
    io={"upload_with_retry": exc("ValueError", "upload fail")})
add("_send_local_file", {"chat_id": "C1", "file_path": "/tmp/d.pdf", "kind": "document"},
    io={"upload_with_retry": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_local_file", {"chat_id": "C1", "file_path": "/tmp/v.mp4", "kind": "video"},
    io={"exists": exc("ValueError", "stat")})
add("_send_local_file", {"chat_id": "C1", "file_path": "/tmp/v.mp4", "kind": "video"},
    io={"dm_target": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _send_slash_ephemeral ----
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "hello"})
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "abc"},
    io={"format_chunks": {"__script__": True, "ret": ["a", "b", "c"]}})
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "abc"},
    io={"format_chunks": {"__script__": True, "ret": ["1", "2", "3", "4", "5", "6", "7"]}})
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "x"},
    io={"post": {"status": 500}})
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "x"},
    io={"post": exc("ValueError", "net")})
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "x"},
    io={"post": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_slash_ephemeral", {"ctx": {"response_url": "https://hooks/x"}, "content": "x"},
    io={"format_chunks": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _send_slash_reply ----
add("_send_slash_reply", {"chat_id": "C1", "slash_ctx": {"response_url": "u"}, "content": "x"})
add("_send_slash_reply", {"chat_id": "C1", "slash_ctx": {"response_url": "u"}, "content": "x"},
    io={"send_slash_ephemeral": {"success": False, "error": "swap", "message_id": None},
        "post_ephemeral_fallback": {"success": True, "error": None, "message_id": "e1"}})
add("_send_slash_reply", {"chat_id": "C1", "slash_ctx": {}, "content": "x"},
    io={"send_slash_ephemeral": {"success": False, "error": "swap", "message_id": None}})
add("_send_slash_reply", {"chat_id": "C1", "content": "x"},
    io={"send_slash_ephemeral": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_slash_reply", {"chat_id": "C1", "content": "x"},
    io={"send_slash_ephemeral": {"success": False, "error": "swap", "message_id": None},
        "post_ephemeral_fallback": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_send_slash_reply", {"chat_id": "C1", "content": "x"},
    io={"clear_thread_status_quietly": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _set_thread_status ----
add("_set_thread_status", {"chat_id": "C1", "team_id": "T1", "thread_ts": "171.0",
    "status": "", "fail_label": "start"})
add("_set_thread_status", {"chat_id": "C1", "fail_label": "stop"},
    io={"get_client": exc("ValueError", "no client")})
add("_set_thread_status", {"chat_id": "C1", "fail_label": "stop"},
    io={"get_client": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_set_thread_status", {"chat_id": "C1", "fail_label": "stop"},
    io={"session_status_method": exc("ValueError", "unsupported")})
add("_set_thread_status", {"chat_id": "C1", "fail_label": "stop"},
    io={"set_status": exc("ValueError", "post fail")})
add("_set_thread_status", {"chat_id": "C1", "fail_label": "stop"},
    io={"set_status": exc("CancelledError", "cancel")}, expect_raise="CancelledError")

# ---- _set_thread_watermark ----
add("_set_thread_watermark", {"chat_id": "C1", "watermark_ts": ""})
add("_set_thread_watermark", {"chat_id": "C1", "thread_ts": "171.0", "user_id": "U1",
    "watermark_ts": "171.5", "team_id": "T1"})
add("_set_thread_watermark", {"chat_id": "C1", "watermark_ts": "171.5"},
    io={"to_thread": exc("ValueError", "db fail")})
add("_set_thread_watermark", {"chat_id": "C1", "watermark_ts": "171.5"},
    io={"to_thread": exc("SystemExit", "halt")}, expect_raise="SystemExit")
add("_set_thread_watermark", {"chat_id": "D1", "thread_ts": "2.0", "user_id": "U2",
    "watermark_ts": "2.5", "team_id": "T2"})
add("_set_thread_watermark", {"chat_id": "C1", "watermark_ts": 0})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8") as fh:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    assert all(v >= 6 for v in counts.values()), "thin function"


if __name__ == "__main__":
    main()
