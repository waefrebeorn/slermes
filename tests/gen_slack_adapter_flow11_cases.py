#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow11.jsonl for the Slack
plugin-adapter FLOW port, wave 11 (src/gateway/platforms/slack_adapter_flow11.c).

Deterministic corpus: >=6 cases per ported function (16 funcs), covering pure
logic, chained seams with ORDERED traces, fail-closed missing scripts and
BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Every case carries the FULL seam io-key union so fixture io keys equal
SEAM_NAMES exactly (SET EQUALITY).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow11.jsonl")
SEAMS = [
    "append_feedback_block", "apply_slack_proxy", "async_web_client", "auth_fn_call",
    "call_with_block_fallback", "chat_postEphemeral", "collect_thread_root_images", "create_task",
    "date_sub", "ensure_socket_watchdog", "entity_label_sub", "event_declares_bot_sender",
    "evict_oldest_by_ts", "extra_flag", "extract_assistant_thread_metadata", "fetch_thread_context",
    "find_user_mentions", "fenced_code_sub", "format_chunks", "get_client", "get_running_loop",
    "get_thread_watermark", "has_active_session_for_thread", "hermes_user_agent", "inline_code_sub",
    "inline_style_collapse", "is_sender_authorized", "logger.debug", "logger.warning",
    "markdown_block_payload", "mark_thread_rehydration_checked", "maybe_blocks", "mrkdwn_link_sub",
    "permalink_sub", "render_blocks", "resolve_user_is_bot", "resolve_user_name",
    "restart_socket_mode", "runner_auth_fn", "sanitize_blocks", "set_thread_watermark",
    "slack_allow_bots", "slack_response_payload", "slack_unfurl_kwargs", "sub_user_mention",
    "task_cancelled", "task_exception", "thread_rehydration_key", "truncate_message",
    "trim_mentioned_threads", "unescape_slack_entities", "workspace_message_marker",
    "workspace_thread_key",
]

BASE = {n: None for n in SEAMS}
BASE.update({
    "hermes_user_agent": "UA", "async_web_client": {"id": "client"},
    "logger.warning": None, "logger.debug": None,
    "thread_rehydration_key": "rk", "evict_oldest_by_ts": None,
    "workspace_message_marker": "wm", "trim_mentioned_threads": None,
    "task_cancelled": False, "task_exception": None,
    "ensure_socket_watchdog": None, "get_running_loop": {"id": "loop"},
    "restart_socket_mode": None, "create_task": None,
    "event_declares_bot_sender": False, "resolve_user_is_bot": False,
    "slack_allow_bots": "all", "extra_flag": False,
    "markdown_block_payload": ["b"], "append_feedback_block": ["b"],
    "sanitize_blocks": ["b"], "render_blocks": ["b"],
    "format_chunks": ["c1", "c2"], "get_client": {"id": "client"},
    "chat_postEphemeral": {"ok": True}, "slack_response_payload": {"ok": True},
    "truncate_message": ["c1"], "slack_unfurl_kwargs": {},
    "maybe_blocks": ["b"], "call_with_block_fallback": {"ok": True},
    "extract_assistant_thread_metadata": {}, "workspace_thread_key": "key",
    "find_user_mentions": ["U1"], "resolve_user_name": "Alice",
    "sub_user_mention": "@Alice", "has_active_session_for_thread": False,
    "fetch_thread_context": "ctx", "collect_thread_root_images": [["u1"], ["t1"]],
    "get_thread_watermark": "", "set_thread_watermark": None,
    "mark_thread_rehydration_checked": None,
    "unescape_slack_entities": "T", "mrkdwn_link_sub": "T", "date_sub": "T",
    "permalink_sub": "T", "entity_label_sub": "T", "fenced_code_sub": "T",
    "inline_code_sub": "T", "inline_style_collapse": "T",
    "is_sender_authorized": None, "runner_auth_fn": None, "auth_fn_call": True,
})

CASES = []


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


def case(func, args=None, io=None, state=None, expect_raise=None):
    merged = dict(BASE)
    if io:
        merged.update(io)
    for k, v in merged.items():
        if isinstance(v, list):
            merged[k] = script(ret=v)
    inp = {"args": dict(args or {}), "io": merged, "state": dict(state or {})}
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


# ------------------------------- _new_web_client
case("_new_web_client", {"token": "xoxb", "proxy_url": "http://p"})
case("_new_web_client", {"token": "xoxb", "proxy_url": None})
case("_new_web_client", {"token": "", "proxy_url": ""}, io={"hermes_user_agent": ""})
case("_new_web_client", {"token": "t"}, io={"hermes_user_agent": rais("ValueError", "u")})
case("_new_web_client", {"token": "t"}, io={"async_web_client": rais("ValueError", "c")})
case("_new_web_client", {"token": "t"}, io={"apply_slack_proxy": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _note_attachment_failure
case("_note_attachment_failure", {"detail": "d", "fallback_msg": "f", "fallback_args": [], "exc_info": False}, state={"notices": []})
case("_note_attachment_failure", {"detail": "", "fallback_msg": "f", "fallback_args": ["a"], "exc_info": True})
case("_note_attachment_failure", {"detail": None, "fallback_msg": "f", "fallback_args": []}, state={"notices": ["x"]})
case("_note_attachment_failure", {"detail": "d", "fallback_msg": "f", "fallback_args": []}, state={"notices": None})
case("_note_attachment_failure", {"detail": 0, "fallback_msg": "f", "fallback_args": ["x", 2]})
case("_note_attachment_failure", {"detail": "d"}, io={"logger.warning": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _mark_thread_rehydration_checked
case("_mark_thread_rehydration_checked", {"channel_id": "C", "thread_ts": "100", "user_id": "U", "team_id": "T"})
case("_mark_thread_rehydration_checked", {"channel_id": "C", "thread_ts": "100", "user_id": "U", "team_id": "T"}, state={"_thread_rehydration_checked": ["rk"]})
case("_mark_thread_rehydration_checked", {"channel_id": "D", "thread_ts": "200", "user_id": "U2", "team_id": ""}, io={"thread_rehydration_key": "rk2"})
case("_mark_thread_rehydration_checked", {"channel_id": "C"}, io={"thread_rehydration_key": rais("ValueError", "k")})
case("_mark_thread_rehydration_checked", {"channel_id": "C"}, io={"evict_oldest_by_ts": rais("ValueError", "e")})
case("_mark_thread_rehydration_checked", {"channel_id": "C"}, io={"evict_oldest_by_ts": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _register_mentioned_thread
case("_register_mentioned_thread", {"thread_ts": "100", "team_id": "T"})
case("_register_mentioned_thread", {"thread_ts": "", "team_id": "T"})
case("_register_mentioned_thread", {"thread_ts": "200", "team_id": ""}, io={"workspace_message_marker": "wm2"})
case("_register_mentioned_thread", {"thread_ts": "100"}, state={"_mentioned_threads": ["wm"]})
case("_register_mentioned_thread", {"thread_ts": "100"}, io={"workspace_message_marker": rais("ValueError", "m")})
case("_register_mentioned_thread", {"thread_ts": "100"}, io={"trim_mentioned_threads": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _track_reacting_message
case("_track_reacting_message", {"team_id": "T", "ts": "100"})
case("_track_reacting_message", {"team_id": "", "ts": "200"}, io={"workspace_message_marker": "wm2"})
case("_track_reacting_message", {"team_id": "T", "ts": "100"}, state={"_reacting_message_ids": ["wm"]})
case("_track_reacting_message", {"team_id": "T", "ts": "100"}, io={"workspace_message_marker": rais("ValueError", "m")})
case("_track_reacting_message", {"team_id": "T", "ts": "100"}, io={"evict_oldest_by_ts": rais("ValueError", "e")})
case("_track_reacting_message", {"team_id": "T", "ts": "100"}, io={"evict_oldest_by_ts": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _on_socket_watchdog_done
_WD = {"task": {"id": "t1"}}
_WDS = {"_socket_watchdog_task": "t1", "_running": True}
case("_on_socket_watchdog_done", _WD, state=_WDS, io={"task_exception": "boom"})
case("_on_socket_watchdog_done", _WD, state=_WDS, io={"task_exception": None})
case("_on_socket_watchdog_done", {"task": {"id": "other"}}, state=_WDS)
case("_on_socket_watchdog_done", _WD, state=_WDS, io={"task_cancelled": True})
case("_on_socket_watchdog_done", _WD, state={"_socket_watchdog_task": "t1", "_running": False})
case("_on_socket_watchdog_done", _WD, state=_WDS, io={"task_exception": rais("ValueError", "x")}, expect_raise="ValueError")
case("_on_socket_watchdog_done", _WD, state=_WDS, io={"ensure_socket_watchdog": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _on_socket_mode_task_done
_MD = {"task": {"id": "t1"}}
_MDS = {"_socket_mode_task": "t1", "_running": True}
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"task_exception": "boom"})
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"task_exception": None})
case("_on_socket_mode_task_done", {"task": {"id": "other"}}, state=_MDS)
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"task_cancelled": True})
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"task_exception": rais("CancelledError", "c")})
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"task_exception": rais("ValueError", "v")})
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"get_running_loop": rais("RuntimeError", "r")})
case("_on_socket_mode_task_done", _MD, state=_MDS, io={"create_task": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _peer_bot_drop
_PD = {"event": {"user": "U"}, "user_id": "U", "bot_uid": "B", "channel_id": "C", "team_id": "T", "is_mentioned": False}
case("_peer_bot_drop", dict(_PD, user_id=""))
case("_peer_bot_drop", dict(_PD, user_id="B"))
case("_peer_bot_drop", _PD, io={"event_declares_bot_sender": True, "slack_allow_bots": "none"})
case("_peer_bot_drop", _PD, io={"event_declares_bot_sender": False, "resolve_user_is_bot": True, "slack_allow_bots": "mentions"})
case("_peer_bot_drop", _PD, io={"event_declares_bot_sender": False, "resolve_user_is_bot": False})
case("_peer_bot_drop", _PD, io={"event_declares_bot_sender": True, "slack_allow_bots": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _maybe_blocks
case("_maybe_blocks", {"content": "hi"}, io={"extra_flag": script(True)})
case("_maybe_blocks", {"content": "hi"}, io={"extra_flag": script(True), "markdown_block_payload": []})
case("_maybe_blocks", {"content": "hi"})
case("_maybe_blocks", {"content": "hi"}, io={"extra_flag": script(False)})
case("_maybe_blocks", {"content": "hi"}, io={"extra_flag": script(True), "markdown_block_payload": [], "render_blocks": rais("ValueError", "r")})
case("_maybe_blocks", {"content": "hi"}, io={"extra_flag": script(True), "markdown_block_payload": [], "render_blocks": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_maybe_blocks", {"content": "hi"}, io={"sanitize_blocks": rais("ValueError", "s")})

# ------------------------------- _post_ephemeral_fallback
_PEF = {"chat_id": "C", "content": "hi"}
case("_post_ephemeral_fallback", dict(_PEF, ctx={}))
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}))
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}), io={"chat_postEphemeral": {"ok": False, "error": "x"}})
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}), io={"slack_response_payload": {}})
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}), io={"chat_postEphemeral": rais("ValueError", "boom")})
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}), io={"chat_postEphemeral": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_post_ephemeral_fallback", dict(_PEF, ctx={"user_id": "U"}), io={"slack_response_payload": rais("ValueError", "p")})

# ------------------------------- _post_chunks
_PC = {"chat_id": "C", "team_id": "T", "content": "hi", "formatted": "hi", "thread_ts": "", "extra": {}, "max_length": 3900}
case("_post_chunks", _PC)
case("_post_chunks", dict(_PC, truncate_message=["a", "b"], thread_ts="100", extra={"reply_broadcast": True}))
case("_post_chunks", dict(_PC, truncate_message=[]))
case("_post_chunks", dict(_PC, truncate_message=["a"], slack_unfurl_kwargs={"unfurl_links": False}))
case("_post_chunks", dict(_PC, truncate_message=["a"]), io={"maybe_blocks": rais("ValueError", "m")})
case("_post_chunks", dict(_PC, truncate_message=["a"]), io={"call_with_block_fallback": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_post_chunks", dict(_PC, truncate_message=["a"]), io={"slack_unfurl_kwargs": rais("ValueError", "u")})

# ------------------------------- _lookup_assistant_thread_metadata
_LA = {"event": {}, "body": {}, "channel_id": "", "thread_ts": "", "team_id": ""}
case("_lookup_assistant_thread_metadata", _LA, state={"_assistant_threads": {"key": {"channel_id": "C", "team_id": "T"}}})
case("_lookup_assistant_thread_metadata", dict(_LA, channel_id="C", thread_ts="100", team_id="T"))
case("_lookup_assistant_thread_metadata", _LA, io={"extract_assistant_thread_metadata": {"channel_id": "C2"}}, state={"_assistant_threads": {"key": {"channel_id": "C"}}})
case("_lookup_assistant_thread_metadata", _LA, state={"_assistant_threads": {"key": {}}})
case("_lookup_assistant_thread_metadata", _LA, io={"workspace_thread_key": ""}, state={"_assistant_threads": {}})
case("_lookup_assistant_thread_metadata", _LA, io={"extract_assistant_thread_metadata": rais("ValueError", "e")})
case("_lookup_assistant_thread_metadata", _LA, io={"workspace_thread_key": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _humanize_user_mentions
_HU = {"text": "hi <@U1|alice>", "chat_id": "C", "team_id": "T"}
case("_humanize_user_mentions", {"text": "no mentions here", "chat_id": "C"})
case("_humanize_user_mentions", {"text": "", "chat_id": "C"})
case("_humanize_user_mentions", _HU)
case("_humanize_user_mentions", _HU, io={"resolve_user_name": ""})
case("_humanize_user_mentions", _HU, io={"find_user_mentions": ["U1", "U2"]})
case("_humanize_user_mentions", _HU, io={"resolve_user_name": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_humanize_user_mentions", _HU, io={"sub_user_mention": rais("ValueError", "s")})

# ------------------------------- _hydrate_thread_context
_HY = {"channel_id": "C", "event_thread_ts": "100", "ts": "200", "user_id": "U", "team_id": "T",
       "is_thread_reply": True, "is_mentioned": False, "is_dm": False}
case("_hydrate_thread_context", dict(_HY, is_thread_reply=False))
case("_hydrate_thread_context", _HY)
case("_hydrate_thread_context", dict(_HY, is_mentioned=True, is_dm=True), io={"has_active_session_for_thread": True})
case("_hydrate_thread_context", _HY, io={"has_active_session_for_thread": True}, state={"_thread_rehydration_checked": ["rk"]})
case("_hydrate_thread_context", _HY, io={"has_active_session_for_thread": True, "get_thread_watermark": "150"})
case("_hydrate_thread_context", _HY, io={"has_active_session_for_thread": True, "get_thread_watermark": ""})
case("_hydrate_thread_context", _HY, io={"has_active_session_for_thread": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_hydrate_thread_context", _HY, io={"mark_thread_rehydration_checked": rais("ValueError", "m")})

# ------------------------------- _normalize_slack_text_for_dedupe
_NR = {"text": "a   b", "bot_uid": ""}
case("_normalize_slack_text_for_dedupe", _NR)
case("_normalize_slack_text_for_dedupe", {"text": "x <@U1> y", "bot_uid": "U1"})
case("_normalize_slack_text_for_dedupe", {"text": "", "bot_uid": ""})
case("_normalize_slack_text_for_dedupe", _NR, io={"unescape_slack_entities": rais("ValueError", "u")})
case("_normalize_slack_text_for_dedupe", _NR, io={"inline_style_collapse": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_normalize_slack_text_for_dedupe", _NR, io={"mrkdwn_link_sub": "a  b", "inline_style_collapse": "a b"})

# ------------------------------- _is_interactive_user_authorized
_IA = {"user_id": " U1 ", "channel_id": "C", "user_name": "Alice", "team_id": "T"}
case("_is_interactive_user_authorized", {"user_id": "", "channel_id": "C"})
case("_is_interactive_user_authorized", _IA, state={"_authorization_check": True}, io={"is_sender_authorized": True})
case("_is_interactive_user_authorized", _IA, state={"_authorization_check": True, "_gate_env": {"SLACK_ALLOW_ALL_USERS": "true"}}, io={"is_sender_authorized": None})
case("_is_interactive_user_authorized", _IA, io={"runner_auth_fn": {"fn": 1}, "auth_fn_call": True})
case("_is_interactive_user_authorized", _IA, io={"runner_auth_fn": {"fn": 1}, "auth_fn_call": rais("ValueError", "v")}, state={"_gate_env": {"GATEWAY_ALLOW_ALL_USERS": "yes"}})
case("_is_interactive_user_authorized", _IA, state={"_gate_env": {"SLACK_ALLOWED_USERS": "U2,U1"}})
case("_is_interactive_user_authorized", _IA, state={"_gate_env": {"GATEWAY_ALLOW_ALL_USERS": "1"}})
case("_is_interactive_user_authorized", _IA, state={"_authorization_check": True}, io={"is_sender_authorized": rais("SystemExit", "base")}, expect_raise="SystemExit")


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print("wrote " + str(len(CASES)) + " cases to " + OUT)
    for k in sorted(counts):
        print("  " + k + ": " + str(counts[k]))


if __name__ == "__main__":
    main()
