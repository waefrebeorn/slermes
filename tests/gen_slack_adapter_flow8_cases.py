#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow8.jsonl for the Slack
plugin-adapter FLOW port, wave 8 (src/gateway/platforms/slack_adapter_flow8.c).

Deterministic corpus: >=6 cases per ported function (16 funcs), covering pure
logic, chained seams with ORDERED traces, fail-closed missing scripts and
BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow8.jsonl")
CASES = []


def case(func, args=None, io=None, state=None, expect_raise=None):
    args = dict(args or {})
    st = dict(state) if state is not None else {}
    inp = {"args": args, "state": st}
    if io is not None:
        # These seams return a LIST as their DATA value (not a call-sequence
        # script). Wrap raw lists so the seam backend does not consume them as
        # per-call results.
        _data_list = {"files_with_forwarded", "allowed_channels", "cache_document",
                      "cache_slack_file"}
        io = dict(io)
        for k in _data_list:
            if isinstance(io.get(k), list):
                io[k] = script(ret=io[k])
        inp["io"] = io
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


def script(ret=None, raise_spec=None):
    d = {"__script__": True}
    if raise_spec is not None:
        d["raise"] = raise_spec
    else:
        d["ret"] = ret
    return d


def rais(kind, message, **extra):
    spec = {"type": kind, "message": message}
    spec.update(extra)
    return script(raise_spec=spec)


# ------------------------------- _cancel_socket_tasks
case("_cancel_socket_tasks", {"tasks": []})
case("_cancel_socket_tasks", {"tasks": [{"tag": "a", "has_cancel": False}]})
case("_cancel_socket_tasks", {"tasks": [{"tag": "a", "has_cancel": True, "has_done": True}]},
     io={"task.done": True})
case("_cancel_socket_tasks", {"tasks": [{"tag": "a", "has_cancel": True, "has_done": False}]},
     io={"task.cancel": None, "asyncio.wait": {"done": ["a"], "still": []},
         "task.cancelled": True})
case("_cancel_socket_tasks", {"tasks": [{"tag": "b", "has_cancel": True, "has_done": False}]},
     io={"task.cancel": None, "asyncio.wait": {"done": ["b"], "still": []},
         "task.cancelled": False, "task.exception": "E", "logger.debug": None})
case("_cancel_socket_tasks", {"tasks": [{"tag": "c", "has_cancel": True, "has_done": False}]},
     io={"task.cancel": None, "asyncio.wait": {"done": [], "still": ["c"]},
         "logger.warning": None})
case("_cancel_socket_tasks", {"tasks": [{"tag": "d", "has_cancel": True, "has_done": False}]},
     io={"task.cancel": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _close_workspace_clients
case("_close_workspace_clients", {}, state={"_team_clients": []},
     io={"app_client": None})
case("_close_workspace_clients", {}, state={"_team_clients": []},
     io={"app_client": {"ident": "p", "methods": ["close"]}, "close": None})
case("_close_workspace_clients", {}, state={"_team_clients": [{"ident": "a", "methods": ["close"]}]},
     io={"app_client": None, "close": None})
case("_close_workspace_clients", {},
     state={"_team_clients": [{"ident": "a", "methods": ["close"]}]},
     io={"app_client": {"ident": "a", "methods": ["close"]}, "close": None})
case("_close_workspace_clients", {}, state={"_team_clients": [{"ident": "b", "methods": ["aclose"]}]},
     io={"app_client": None, "aclose": None})
case("_close_workspace_clients", {}, state={"_team_clients": []},
     io={"app_client": {"ident": "p", "methods": ["close"]},
         "close": {"__await__": True}, "await_close": None})
case("_close_workspace_clients", {}, state={"_team_clients": [{"ident": "x", "methods": ["close"]}]},
     io={"app_client": None, "close": rais("KeyboardInterrupt", "base")},
     expect_raise="KeyboardInterrupt")

# ------------------------------- _describe_slack_download_failure
D_BASE = {"file_obj": {"name": "f.txt"}, "exc": {"kind": "X"}}
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": "api detail"})
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": None, "is_http_status_error": True,
         "status_code": 403, "http_template": "tmpl {file_label}",
         "format_template": "formatted"})
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": None, "is_http_status_error": False,
         "str_exc": "Slack returned HTML instead of media here"})
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": None, "is_http_status_error": False,
         "str_exc": "got non-image data"})
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": None, "is_http_status_error": False,
         "str_exc": "plain error"})
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_describe_slack_download_failure", D_BASE,
     io={"attachment_label": "f.txt", "getattr_response": None,
         "describe_api_error": None, "is_http_status_error": True,
         "status_code": 500, "http_template": None, "str_exc": "plain"})

# ------------------------------- _client_for
for i, tid in enumerate([None, "T1", "T2", "", "T3"]):
    case("_client_for", {"chat_id": "C%d" % i, "metadata": {"team_id": tid}},
         io={"metadata_team_id": tid, "get_client": {"client": i}})
case("_client_for", {"chat_id": "C", "metadata": None},
     io={"metadata_team_id": None, "get_client": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _dm_target
for i, tid in enumerate([None, "T1", "T2", "T3"]):
    case("_dm_target", {"chat_id": "U%d" % i, "metadata": {"team_id": tid}},
         io={"metadata_team_id": tid, "ensure_dm_conversation": "D%d" % i})
case("_dm_target", {"chat_id": "U5", "metadata": None},
     io={"metadata_team_id": None, "ensure_dm_conversation": rais("ValueError", "dm")},
     expect_raise="ValueError")
case("_dm_target", {"chat_id": "U6", "metadata": {"team_id": "T6"}},
     io={"metadata_team_id": "T6", "ensure_dm_conversation": "D6"})

# ------------------------------- _clear_thread_status_quietly
case("_clear_thread_status_quietly", {"chat_id": "C1", "metadata": None},
     io={"stop_typing": None})
case("_clear_thread_status_quietly", {"chat_id": "C2", "metadata": {"x": 1}},
     io={"stop_typing": None})
case("_clear_thread_status_quietly", {"chat_id": "C3", "metadata": None},
     io={"stop_typing": rais("ValueError", "stop"), "logger.debug": None})
case("_clear_thread_status_quietly", {"chat_id": "C4", "metadata": None},
     io={"stop_typing": rais("TypeError", "stop"), "logger.debug": None})
case("_clear_thread_status_quietly", {"chat_id": "C5", "metadata": None},
     io={"stop_typing": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_clear_thread_status_quietly", {"chat_id": "C6", "metadata": None},
     io={"stop_typing": rais("KeyboardInterrupt", "base")},
     expect_raise="KeyboardInterrupt")

# ------------------------------- _call_with_block_fallback
case("_call_with_block_fallback", {"client_fn": 1, "method": "chat_postMessage",
     "kwargs": {"channel": "C"}, "verb": "post"},
     io={"client_fn": None, "method_call": {"ok": 1}})
case("_call_with_block_fallback", {"client_fn": 1, "method": "chat_update",
     "kwargs": {"blocks": [{"type": "section"}]}, "verb": "edit"},
     io={"client_fn": None, "method_call": {"ok": 2}})
case("_call_with_block_fallback", {"client_fn": 1, "method": "m",
     "kwargs": {"channel": "C"}, "verb": "post"},
     io={"client_fn": None, "method_call": rais("ValueError", "no blocks")},
     expect_raise="ValueError")
case("_call_with_block_fallback", {"client_fn": 1, "method": "m",
     "kwargs": {"channel": "C", "blocks": [{"x": 1}]}, "verb": "post"},
     io={"client_fn": None, "method_call": [rais("ValueError", "reject"), {"ok": 3}],
         "is_block_rejection": True, "logger.info": None})
case("_call_with_block_fallback", {"client_fn": 1, "method": "m",
     "kwargs": {"blocks": [{"x": 1}]}, "verb": "edit"},
     io={"client_fn": None, "method_call": [rais("ValueError", "reject"), {"ok": 4}],
         "is_block_rejection": True, "logger.info": None})
case("_call_with_block_fallback", {"client_fn": 1, "method": "m",
     "kwargs": {"blocks": [{"x": 1}]}, "verb": "post"},
     io={"client_fn": None, "method_call": rais("ValueError", "other"),
         "is_block_rejection": False}, expect_raise="ValueError")
case("_call_with_block_fallback", {"client_fn": 1, "method": "m",
     "kwargs": {"blocks": [{"x": 1}]}, "verb": "post"},
     io={"client_fn": None, "method_call": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _commit_stream
def cs(**over):
    d = dict(key=["T1", "C1", "k"], stream={"ts": "111.1", "base": 2},
             text="abcdef", metadata={"m": 1}, delta="", replace=False)
    d.update(over)
    return d

case("_commit_stream", cs(),
     io={"pop_stream": None, "seal_stream": True, "maybe_blocks": False,
         "stop_typing": None, "send_result": {"success": True, "message_id": "111.1"}})
case("_commit_stream", cs(),
     io={"pop_stream": None, "seal_stream": [False, True], "maybe_blocks": False,
         "stop_typing": None, "send_result": {"success": True, "message_id": "111.1"}})
case("_commit_stream", cs(replace=True),
     io={"pop_stream": None, "seal_stream": True,
         "edit_message": {"success": True}, "stop_typing": None,
         "send_result": {"success": True, "message_id": "111.1"}})
case("_commit_stream", cs(),
     io={"pop_stream": None, "seal_stream": [False, False], "maybe_blocks": False,
         "edit_message": {"success": True}, "logger.warning": None,
         "stop_typing": None, "send_result": {"success": True, "message_id": "111.1"}})
case("_commit_stream", cs(),
     io={"pop_stream": None, "seal_stream": [False, False], "maybe_blocks": False,
         "edit_message": {"success": False}, "logger.warning": None})
case("_commit_stream", cs(),
     io={"pop_stream": None, "seal_stream": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _dm_top_level_threads_as_sessions
case("_dm_top_level_threads_as_sessions", {}, io={"extra_flag": True})
case("_dm_top_level_threads_as_sessions", {}, io={"extra_flag": False})
case("_dm_top_level_threads_as_sessions", {}, io={"extra_flag": None})
case("_dm_top_level_threads_as_sessions", {"x": 1}, io={"extra_flag": True})
case("_dm_top_level_threads_as_sessions", {}, io={"extra_flag": 1})
case("_dm_top_level_threads_as_sessions", {},
     io={"extra_flag": rais("RuntimeError", "flag")}, expect_raise="RuntimeError")

# ------------------------------- _channel_gate_allows
def cg(**over):
    d = dict(channel_id="C1", routing_text="hello", bot_uid="UBOT",
             is_mentioned=False, is_thread_reply=False, event_thread_ts="111.1",
             user_id="U1", team_id="T1", is_dm=False, force_process=False)
    d.update(over)
    return d

case("_channel_gate_allows", cg(channel_id="C2"),
     io={"allowed_channels": script(ret=["C1"])})
case("_channel_gate_allows", cg(force_process=True),
     io={"allowed_channels": [], "ignore_other_user_mentions": False,
         "thread_require_mention": False})
case("_channel_gate_allows", cg(),
     io={"allowed_channels": [], "ignore_other_user_mentions": True,
         "mentions_self": True, "thread_require_mention": False,
         "is_free_channel": True})
case("_channel_gate_allows", cg(),
     io={"allowed_channels": [], "ignore_other_user_mentions": True,
         "mentions_self": False, "addressed_to_other_user": True})
case("_channel_gate_allows", cg(is_thread_reply=True),
     io={"allowed_channels": [], "ignore_other_user_mentions": False,
         "thread_require_mention": True, "is_free_channel": True,
         "logger.debug": None})
case("_channel_gate_allows", cg(),
     io={"allowed_channels": [], "ignore_other_user_mentions": False,
         "thread_require_mention": False, "is_free_channel": False,
         "strict_mention": True})
case("_channel_gate_allows", cg(),
     io={"allowed_channels": [], "ignore_other_user_mentions": False,
         "thread_require_mention": False, "is_free_channel": False,
         "strict_mention": False, "should_wake": True})
case("_channel_gate_allows", cg(is_mentioned=True),
     io={"allowed_channels": [], "ignore_other_user_mentions": False,
         "thread_require_mention": False, "is_free_channel": False,
         "strict_mention": False})
case("_channel_gate_allows", cg(),
     io={"allowed_channels": rais("GeneratorExit", "base")},
     expect_raise="GeneratorExit")

# ------------------------------- _cache_slack_file
AMAP = {".mp4": "audio/mp4", ".mp3": "audio/mpeg"}
VT = {".mp4": "video/mp4"}
case("_cache_slack_file", {"kind": "image", "f": {}, "url": "http://x",
     "mimetype": "image/png", "team_id": "T1"},
     io={"download_file": "/c/png"})
case("_cache_slack_file", {"kind": "image", "f": {}, "url": "http://x",
     "mimetype": "image/bmp", "team_id": "T1"},
     io={"download_file": "/c/jpg"})
case("_cache_slack_file", {"kind": "audio", "f": {"name": "a.mp3"}, "url": "http://x",
     "mimetype": "audio/mpeg", "team_id": "T1"},
     io={"resolve_audio_ext": ".mp3", "download_file": "/c/mp3"})
case("_cache_slack_file", {"kind": "voice clip", "f": {"name": "audio_message"}, "url": "http://x",
     "mimetype": "video/mp4", "team_id": "T1"}, state={"_SLACK_EXT_TO_AUDIO_MIME": AMAP},
     io={"resolve_audio_ext": ".mp4", "download_file": "/c/m4a", "logger.debug": None})
case("_cache_slack_file", {"kind": "video", "f": {"name": "clip.mov"}, "url": "http://x",
     "mimetype": "video/quicktime", "team_id": "T1"}, state={"SUPPORTED_VIDEO_TYPES": VT},
     io={"download_bytes": "RAW", "cache_video": "/c/vid", "logger.debug": None})
case("_cache_slack_file", {"kind": "document", "f": {}, "url": "http://x",
     "mimetype": "", "team_id": "T1"},
     io={"cache_document": ["/c/doc", "application/pdf", ""]})
case("_cache_slack_file", {"kind": "image", "f": {}, "url": "http://x",
     "mimetype": "image/png", "team_id": "T1"},
     io={"download_file": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _collect_inbound_media
case("_collect_inbound_media", {"event": {}, "text": "hello",
     "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": []})
case("_collect_inbound_media", {"event": {"files": [{"mimetype": "image/png"}]},
     "text": "hi", "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"mimetype": "image/png"}]})
case("_collect_inbound_media", {"event": {"files": [{"mimetype": "image/png",
     "url_private": "http://x"}]}, "text": "hi",
     "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"mimetype": "image/png", "url_private": "http://x"}],
         "file_kind": "image", "cache_slack_file": ["/c", "image/png", ""],
         "inline_document_text": {"text": "hi inlined", "inlined": [False]}})
case("_collect_inbound_media", {"event": {"files": [{"url_private": "http://x"}]},
     "text": "hi", "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"url_private": "http://x"}], "file_kind": "image",
         "cache_slack_file": None})
case("_collect_inbound_media", {"event": {"files": [{"file_access": "check_file_info"}]},
     "text": "hi", "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"file_access": "check_file_info"}], "resolve_stub": None})
case("_collect_inbound_media", {"event": {"files": [{"url_private": "http://x"}]},
     "text": "hi", "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"url_private": "http://x"}], "file_kind": "document",
         "cache_slack_file": rais("ValueError", "cache fail"),
         "describe_failure": "download failed", "note_failure": None})
case("_collect_inbound_media", {"event": {"files": [{"url_private": "http://x"}]},
     "text": "hi", "thread_root_media_urls": [], "thread_root_media_types": []},
     io={"files_with_forwarded": [{"url_private": "http://x"}], "file_kind": "image",
         "cache_slack_file": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _conversations_replies_with_backoff
RB = {"channel_id": "C1", "thread_ts": "111.1", "limit": 50, "team_id": "T1"}
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None, "conversations_replies": {"ok": 1}})
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None,
         "conversations_replies": [rais("ValueError", "ratelimited"), {"ok": 2}],
         "sleep": None})
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None,
         "conversations_replies": [rais("ValueError", "429"), rais("ValueError", "rate_limited"),
                                   {"ok": 3}], "sleep": [None, None]})
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None, "conversations_replies": rais("ValueError", "permanent")},
     expect_raise="ValueError")
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None,
         "conversations_replies": [rais("ValueError", "429"), rais("ValueError", "429"),
                                   rais("ValueError", "429")], "sleep": [None, None]},
     expect_raise="ValueError")
case("_conversations_replies_with_backoff", RB,
     io={"get_client": None, "conversations_replies": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _collect_thread_root_images
case("_collect_thread_root_images", {"channel_id": "C1", "thread_ts": "111.1", "team_id": "T1"},
     io={"thread_cache_key": "C1:111.1"})
case("_collect_thread_root_images", {"channel_id": "C2", "thread_ts": "222.2", "team_id": "T1"},
     state={"_thread_context_cache": {"C2:222.2": {"messages": []}}},
     io={"thread_cache_key": "C2:222.2", "thread_root_message": None})
case("_collect_thread_root_images", {"channel_id": "C3", "thread_ts": "333.3", "team_id": "T1"},
     state={"_thread_context_cache": {"C3:333.3": {"messages": [{"ts": "333.3"}]}}},
     io={"thread_cache_key": "C3:333.3",
         "thread_root_message": {"files": [], "attachments": []},
         "files_with_forwarded": []})
case("_collect_thread_root_images", {"channel_id": "C4", "thread_ts": "444.4", "team_id": "T1"},
     state={"_thread_context_cache": {"C4:444.4": {"messages": [{"ts": "444.4"}]}}},
     io={"thread_cache_key": "C4:444.4", "thread_root_message": {"files": [{"mimetype": "text/plain"}]},
         "files_with_forwarded": [{"mimetype": "text/plain"}]})
case("_collect_thread_root_images", {"channel_id": "C5", "thread_ts": "555.5", "team_id": "T1"},
     state={"_thread_context_cache": {"C5:555.5": {"messages": [{"ts": "555.5"}]}}},
     io={"thread_cache_key": "C5:555.5",
         "thread_root_message": {"files": [{"mimetype": "image/png", "url_private": "http://x"}]},
         "files_with_forwarded": [{"mimetype": "image/png", "url_private": "http://x"}],
         "cache_slack_file": ["/c", "image/png", ""]})
case("_collect_thread_root_images", {"channel_id": "C6", "thread_ts": "666.6", "team_id": "T1"},
     state={"_thread_context_cache": {"C6:666.6": {"messages": [{"ts": "666.6"}]}}},
     io={"thread_cache_key": "C6:666.6",
         "thread_root_message": {"files": [{"file_access": "check_file_info"}]},
         "files_with_forwarded": [{"file_access": "check_file_info"}], "resolve_stub": None})
case("_collect_thread_root_images", {"channel_id": "C7", "thread_ts": "777.7", "team_id": "T1"},
     state={"_thread_context_cache": {"C7:777.7": {"messages": [{"ts": "777.7"}]}}},
     io={"thread_cache_key": "C7:777.7",
         "thread_root_message": {"files": [{"mimetype": "image/png", "url_private": "http://x"}]},
         "files_with_forwarded": [{"mimetype": "image/png", "url_private": "http://x"}],
         "cache_slack_file": rais("ValueError", "fail"), "logger.warning": None})
case("_collect_thread_root_images", {"channel_id": "C8", "thread_ts": "888.8", "team_id": "T1"},
     io={"thread_cache_key": rais("ValueError", "key"), "logger.debug": None})
case("_collect_thread_root_images", {"channel_id": "C9", "thread_ts": "999.9", "team_id": "T1"},
     state={"_thread_context_cache": {"C9:999.9": {"messages": [{"ts": "999.9"}]}}},
     io={"thread_cache_key": "C9:999.9",
         "thread_root_message": {"files": [{"mimetype": "image/png", "url_private": "http://x"}]},
         "files_with_forwarded": [{"mimetype": "image/png", "url_private": "http://x"}],
         "cache_slack_file": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _download_slack_file_bytes
DB = {"url": "https://files.slack.com/f", "team_id": "T1", "html_label": "file bytes"}
def okresp(content="DATA", ct="image/png", status=200):
    return {"status_code": status, "headers": {"content-type": ct}, "content": content}
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": False}, expect_raise="ValueError")
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": False}, expect_raise="ValueError")
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": True, "resolve_download_token": "tok",
         "client_get": okresp(), "raise_for_status": None})
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": True, "resolve_download_token": "tok",
         "client_get": okresp(ct="text/html; charset=utf-8"), "raise_for_status": None},
     expect_raise="ValueError")
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": True, "resolve_download_token": "tok",
         "client_get": [okresp(status=302), okresp()],
         "redirect_target": "https://files.slack.com/g", "raise_for_status": None})
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": True, "resolve_download_token": "tok",
         "client_get": [rais("HTTPStatusError", "503", status_code=503), okresp()],
         "raise_for_status": None, "sleep": None})
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": True, "is_slack_cdn_url": True, "resolve_download_token": "tok",
         "client_get": rais("HTTPStatusError", "400", status_code=400)},
     expect_raise="HTTPStatusError")
case("_download_slack_file_bytes", DB,
     io={"is_safe_url": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _download_slack_file
case("_download_slack_file", {"url": "http://x", "ext": ".png", "audio": False, "team_id": "T1"},
     io={"download_bytes": "RAW", "cache_media": "/c/img"})
case("_download_slack_file", {"url": "http://x", "ext": ".mp3", "audio": True, "team_id": "T1"},
     io={"download_bytes": "RAW", "cache_media": "/c/aud"})
case("_download_slack_file", {"url": "http://x", "ext": ".png", "audio": False, "team_id": "T1"},
     io={"download_bytes": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_download_slack_file", {"url": "http://x", "ext": ".png", "audio": False, "team_id": "T1"},
     io={"download_bytes": "RAW", "cache_media": rais("ValueError", "cache")},
     expect_raise="ValueError")
case("_download_slack_file", {"url": "http://y", "ext": ".jpg", "audio": False, "team_id": "T2"},
     io={"download_bytes": "RAW2", "cache_media": "/c/img2"})
case("_download_slack_file", {"url": "http://z", "ext": ".wav", "audio": True, "team_id": ""},
     io={"download_bytes": "RAW3", "cache_media": "/c/aud3"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    from collections import Counter
    c = Counter(r["func"] for r in CASES)
    keys = set()
    for r in CASES:
        keys |= set((r["in"]).get("io") or {})
    print("wrote %d bytes, %d cases, %d funcs" % (os.path.getsize(OUT), len(CASES), len(c)))
    print("min per func:", min(c.values()), "seam io keys:", len(keys))
    for fn in sorted(c):
        print("  %s: %d" % (fn, c[fn]))


if __name__ == "__main__":
    main()
