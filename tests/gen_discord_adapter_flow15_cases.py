#!/usr/bin/env python3
"""Deterministic case generator for the Discord adapter FLOW part 15 oracle."""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow15.jsonl"

# EXACTLY the C da15_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    # _reset_voice_timeout
    "cancel_voice_timeout", "voice_timeout_limit", "logger_debug",
    "voice_timeout_handler", "ensure_future",
    # _resolve_channel / _resolve_interaction_channel
    "channel_id_from_link", "get_channel", "fetch_channel",
    # _resolve_channel_skills
    "resolve_channel_skills",
    # _resolve_credentials
    "bytes", "monotonic", "note_dave_passthrough_window",
    # _standalone_read_json_limited
    "read_bytes_limited", "resp_json", "response_encoding", "json_loads",
    # _standalone_is_forum
    "lookup_channel_type", "probe_is_forum_cached", "rest_url", "client_timeout",
    "client_session", "session_get", "remember_channel_is_forum",
    # _should_backfill_discord_message
    "persistently_complete", "active_claim", "attempts_exhausted",
    "has_non_down_bot_response",
    # _self_contained_prompt_content / _run_simple_slash
    "t", "logger_info", "check_slash_authorization", "defer_unless_expired",
    "build_slash_event", "handle_message", "edit_original_response",
    "delete_original_response",
    # _safe_sync_slash_commands
    "tree_get_commands", "tree_fetch_commands", "delete_global_command",
    "upsert_global_command", "edit_global_command", "sleep_between_mutations",
    "existing_command_to_payload", "canonicalize_payload", "patchable_payload",
    # _run_post_connect_initialization
    "command_sync_policy", "tree_sync", "desired_sync_fingerprint",
    "command_sync_skip_reason", "record_command_sync_attempt", "wait_for",
    "safe_sync", "is_discord_rate_limit", "extract_retry_after",
    "record_command_sync_rate_limit", "record_command_sync_success",
    # _run_missed_message_backfill
    "backfill_channels", "ledger_db", "record_recovery_scan_start",
    "finish_recovery_scan", "iter_backfill_candidates", "dedup_contains",
    "dedup_discard", "should_backfill", "record_discord_message_seen",
    "record_recovery_attempt", "dispatch_recovered", "max_dispatches",
    "logger_error", "logger_warning",
    # _resolve_allowed_usernames
    "au_print", "au_fetch_members", "multiplex_active", "replace_bridged_env",
    "forum_read_json",
]

def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


# Benign defaults so every seam is present in every case's io (coverage) and
# no case fails closed on an unused seam. Per-case overrides replace values.
BASE_IO = {
    "cancel_voice_timeout": None,
    "voice_timeout_limit": 300,
    "logger_debug": None,
    "voice_timeout_handler": "<coro>",
    "ensure_future": "<task>",
    "channel_id_from_link": None,
    "get_channel": "<channel>",
    "fetch_channel": "<channel>",
    "resolve_channel_skills": sc([]),
    "bytes": "<bytes>",
    "monotonic": 100.0,
    "note_dave_passthrough_window": None,
    "read_bytes_limited": sc([None, False]),
    "resp_json": {},
    "response_encoding": "utf-8",
    "json_loads": {},
    "lookup_channel_type": None,
    "probe_is_forum_cached": None,
    "rest_url": "http://discord/channels/x",
    "client_timeout": "<timeout>",
    "client_session": "<session>",
    "session_get": "<response>",
    "remember_channel_is_forum": None,
    "persistently_complete": False,
    "active_claim": False,
    "attempts_exhausted": False,
    "has_non_down_bot_response": False,
    "t": "trunc",
    "logger_info": None,
    "check_slash_authorization": True,
    "defer_unless_expired": True,
    "build_slash_event": "<event>",
    "handle_message": None,
    "edit_original_response": None,
    "delete_original_response": None,
    "tree_get_commands": sc([]),
    "tree_fetch_commands": sc([]),
    "delete_global_command": None,
    "upsert_global_command": None,
    "edit_global_command": None,
    "sleep_between_mutations": None,
    "existing_command_to_payload": {},
    "canonicalize_payload": {},
    "patchable_payload": {},
    "command_sync_policy": "normal",
    "tree_sync": sc([]),
    "desired_sync_fingerprint": "fp",
    "command_sync_skip_reason": None,
    "record_command_sync_attempt": None,
    "wait_for": None,
    "safe_sync": {"total": 0, "unchanged": 0, "updated": 0,
                  "recreated": 0, "created": 0, "deleted": 0},
    "is_discord_rate_limit": False,
    "extract_retry_after": None,
    "record_command_sync_rate_limit": None,
    "record_command_sync_success": None,
    "backfill_channels": sc([]),
    "ledger_db": True,
    "record_recovery_scan_start": "scan1",
    "finish_recovery_scan": None,
    "iter_backfill_candidates": sc([]),
    "dedup_contains": False,
    "dedup_discard": None,
    "should_backfill": True,
    "record_discord_message_seen": None,
    "record_recovery_attempt": None,
    "dispatch_recovered": True,
    "max_dispatches": 10,
    "logger_error": None,
    "logger_warning": None,
    "au_print": None,
    "au_fetch_members": sc([]),
    "multiplex_active": False,
    "replace_bridged_env": None,
    "forum_read_json": {},
}
assert set(BASE_IO) == set(SEAM_NAMES), (set(BASE_IO) ^ set(SEAM_NAMES))

CASES = []


def add(func, args=None, state=None, io=None):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    merged = dict(BASE_IO)
    if io:
        merged.update(io)
    c["in"]["io"] = merged
    CASES.append(c)


# ── 1. _reset_voice_timeout ────────────────────────────────────────────────
add("_reset_voice_timeout", args={"guild_id": 10},
    io={"voice_timeout_limit": 300, "voice_timeout_handler": "c1", "ensure_future": "task1"})
add("_reset_voice_timeout", args={"guild_id": 20},
    io={"voice_timeout_limit": 0})
add("_reset_voice_timeout", args={"guild_id": -1},
    io={"voice_timeout_limit": -5})
add("_reset_voice_timeout", args={"guild_id": 3},
    io={"voice_timeout_limit": 45, "voice_timeout_handler": "c2", "ensure_future": "task2"})
add("_reset_voice_timeout", args={"guild_id": 10},
    io={"voice_timeout_limit": sc(raise_type="RuntimeError", raise_msg="v")})
add("_reset_voice_timeout", args={"guild_id": 10},
    io={"voice_timeout_limit": 30, "voice_timeout_handler": sc(raise_type="ValueError", raise_msg="h")})
add("_reset_voice_timeout", args={"guild_id": 10},
    io={"voice_timeout_limit": 30, "voice_timeout_handler": "c3",
        "ensure_future": sc(raise_type="SystemExit", raise_msg="bye")})

# ── 2. _voice_timeout_limit (extra, zero-seam) ─────────────────────────────
add("_voice_timeout_limit", args={}, state={})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": 42})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": 7.9})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": "33"})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": 0})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": -12})
add("_voice_timeout_limit", args={}, state={"_voice_timeout_seconds": 100000})

# ── 3. _playback_timeout_limit (extra, zero-seam) ──────────────────────────
add("_playback_timeout_limit", args={}, state={})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": 60})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": 5.5})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": "9"})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": 0})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": -3})
add("_playback_timeout_limit", args={}, state={"_playback_timeout_seconds": 9999})

# ── 4. _resolve_channel ────────────────────────────────────────────────────
add("_resolve_channel", args={"channel_id": 123},
    io={"channel_id_from_link": None, "get_channel": "ch1"})
add("_resolve_channel", args={"channel_id": "123"},
    io={"channel_id_from_link": None, "get_channel": None, "fetch_channel": "ch2"})
add("_resolve_channel", args={"channel_id": " 12345 "},
    io={"channel_id_from_link": 12345, "get_channel": "ch3"})
add("_resolve_channel", args={"channel_id": 99},
    io={"get_channel": None, "fetch_channel": sc(raise_type="RuntimeError", raise_msg="rest")})
add("_resolve_channel", args={"channel_id": "abc"},
    io={"channel_id_from_link": None})
add("_resolve_channel", args={"channel_id": "https://discord.com/channels/1/2/777"},
    io={"channel_id_from_link": 777, "get_channel": "ch4"})
add("_resolve_channel", args={"channel_id": 0},
    io={"get_channel": None, "fetch_channel": None})

# ── 5. _resolve_interaction_channel ────────────────────────────────────────
INT_PRESENT = {"channel": {"id": "c1"}, "channel_id": "11"}
add("_resolve_interaction_channel", args={"interaction": INT_PRESENT})
add("_resolve_interaction_channel", args={"interaction": {"channel_id": "22"}},
    io={"get_channel": "gc"})
add("_resolve_interaction_channel", args={"interaction": {"channel_id": "33"}},
    io={"get_channel": None, "fetch_channel": "fc"})
add("_resolve_interaction_channel", args={"interaction": {}})
add("_resolve_interaction_channel", args={"interaction": {"channel": None, "channel_id": "44"}},
    io={"get_channel": None, "fetch_channel": sc(raise_type="RuntimeError", raise_msg="x")})
add("_resolve_interaction_channel", args={"interaction": {"channel_id": "55"}},
    io={"get_channel": sc(raise_type="KeyboardInterrupt", raise_msg="k")})
add("_resolve_interaction_channel", args={"interaction": {"channel_id": "66"}},
    io={"get_channel": None, "fetch_channel": "fc2"})

# ── 6. _resolve_channel_skills ─────────────────────────────────────────────
add("_resolve_channel_skills", args={"channel_id": "10"},
    state={"config": {"extra": {"channel_skill_bindings": []}}},
    io={"resolve_channel_skills": sc(["a", "b"])})
add("_resolve_channel_skills", args={"channel_id": "10", "parent_id": "9"},
    state={"config": {"extra": {}}}, io={"resolve_channel_skills": None})
add("_resolve_channel_skills", args={"channel_id": "20"},
    state={"config": {"extra": {"x": 1}}}, io={"resolve_channel_skills": sc([])})
add("_resolve_channel_skills", args={"channel_id": "20"},
    state={"config": {"extra": {}}},
    io={"resolve_channel_skills": sc(raise_type="RuntimeError", raise_msg="s")})
add("_resolve_channel_skills", args={"channel_id": "30", "parent_id": None},
    state={"config": {"extra": {}}}, io={"resolve_channel_skills": sc(["z"])})
add("_resolve_channel_skills", args={"channel_id": ""},
    state={"config": {"extra": {}}}, io={"resolve_channel_skills": sc([])})

# ── 7. _resolve_credentials ────────────────────────────────────────────────
CONN = {"secret_key": "sec", "dave_session": "sess", "dave_protocol_version": 2,
        "dave_downgraded": False, "ssrc": 77, "dave_pending_transitions": {}}
add("_resolve_credentials", args={"conn": CONN}, state={"_dave_passthrough_until": 200.0},
    io={"monotonic": 100.0})
add("_resolve_credentials", args={"conn": dict(CONN, dave_pending_transitions={"a": 0})},
    state={"_dave_passthrough_until": 50.0}, io={"monotonic": 100.0})
add("_resolve_credentials", args={"conn": dict(CONN, dave_pending_transitions={"a": 0})},
    state={"_dave_passthrough_until": 200.0}, io={"monotonic": 100.0})
add("_resolve_credentials", args={"conn": dict(CONN, dave_protocol_version=0, dave_downgraded=True)},
    state={"_dave_passthrough_until": 50.0}, io={"monotonic": 100.0})
add("_resolve_credentials", args={"conn": dict(CONN, dave_pending_transitions={"a": 1})},
    state={"_dave_passthrough_until": 50.0}, io={"monotonic": 100.0})
add("_resolve_credentials", args={"conn": CONN}, state={"_dave_passthrough_until": 50.0},
    io={"monotonic": sc(raise_type="RuntimeError", raise_msg="m")})
add("_resolve_credentials", args={"conn": CONN}, state={"_dave_passthrough_until": 50.0},
    io={"bytes": sc(raise_type="TypeError", raise_msg="b")})

# ── 8. _standalone_read_json_limited ───────────────────────────────────────
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc([None, False]), "resp_json": {"a": 1}})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(["", False])})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(["{\"x\": 2}", False]), "json_loads": {"x": 2}})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(["anything", True])})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(["[]", False]), "json_loads": []})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(raise_type="OSError", raise_msg="r")})
add("_standalone_read_json_limited", args={"limit_bytes": 1000},
    io={"read_bytes_limited": sc(["x", False]), "json_loads": sc(raise_type="ValueError", raise_msg="j")})

# ── 9. _standalone_is_forum ────────────────────────────────────────────────
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": "forum"})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": "text"})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": None, "probe_is_forum_cached": True})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": None, "probe_is_forum_cached": False})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": None, "probe_is_forum_cached": None,
        "rest_url": "http://x", "forum_read_json": {"type": 15}})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": None, "probe_is_forum_cached": None,
        "forum_read_json": {"type": 0}})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": sc(raise_type="RuntimeError", raise_msg="l"),
        "probe_is_forum_cached": None})
add("_standalone_is_forum", args={"chat_id": "100"},
    io={"lookup_channel_type": None, "probe_is_forum_cached": None,
        "rest_url": sc(raise_type="OSError", raise_msg="u")})

# ── 10. _should_backfill_discord_message ───────────────────────────────────
MSG = {"id": 55, "author": {"id": "u1"}}
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"})
add("_should_backfill_discord_message", args={"message": {"id": 55, "author": {"id": "bot"}},
                                              "user_id": "bot"})
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"},
    io={"persistently_complete": True})
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"},
    io={"active_claim": True})
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"},
    io={"attempts_exhausted": True})
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"},
    io={"has_non_down_bot_response": True})
add("_should_backfill_discord_message", args={"message": MSG, "no_client": True})
add("_should_backfill_discord_message", args={"message": MSG, "user_id": "bot"},
    io={"persistently_complete": sc(raise_type="SystemExit", raise_msg="bye")})

# ── 11. _self_contained_prompt_content ─────────────────────────────────────
add("_self_contained_prompt_content", args={"header": "H", "body": "body"})
add("_self_contained_prompt_content", args={"header": "H", "body": "body", "code_block": True})
add("_self_contained_prompt_content", args={"header": "H", "body": "body", "tail": "END"})
add("_self_contained_prompt_content", args={"header": "H", "body": "x" * 2500})
add("_self_contained_prompt_content", args={"header": "H", "body": "x" * 2500, "code_block": True},
    io={"t": "MARK"})
add("_self_contained_prompt_content", args={"header": "", "body": "", "code_block": True, "tail": "T"})
add("_self_contained_prompt_content", args={"header": "H", "body": "abc"},
    io={"t": sc(raise_type="RuntimeError", raise_msg="t")})

# ── 12. _run_simple_slash ──────────────────────────────────────────────────
SI = {"user": {"id": "u1", "name": "n"}, "channel_id": "10", "guild_id": "20"}
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x arg"})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x"},
    io={"check_slash_authorization": False})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x", "followup_msg": "key"})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x", "followup_msg": "key"},
    io={"defer_unless_expired": False})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x"},
    io={"handle_message": sc(raise_type="RuntimeError", raise_msg="h")})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x", "followup_msg": "key"},
    io={"edit_original_response": sc(raise_type="OSError", raise_msg="e")})
add("_run_simple_slash", args={"interaction": {}, "command_text": "/z"})
add("_run_simple_slash", args={"interaction": SI, "command_text": "/x"},
    io={"logger_info": sc(raise_type="KeyboardInterrupt", raise_msg="k")})

# ── 13. _safe_sync_slash_commands ──────────────────────────────────────────
CLIENT = {"application_id": 123, "user": {"id": 456}, "tree": {}, "http": {}}
add("_safe_sync_slash_commands", args={}, state={"_client": None})
add("_safe_sync_slash_commands", args={}, state={"_client": {"user": {}}})
add("_safe_sync_slash_commands", args={}, state={"_client": dict(CLIENT, application_id=None, user={})})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc([]), "tree_fetch_commands": sc([])})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc([{"name": "a", "payload": {"type": 1, "name": "a"}}]),
        "tree_fetch_commands": sc([])})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc([]), "tree_fetch_commands": sc([{"id": 9, "name": "old", "type": 1}])})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc([{"name": "a", "payload": {"type": 1, "name": "a"}}]),
        "tree_fetch_commands": sc([{"id": 9, "name": "a", "type": 1}]),
        "existing_command_to_payload": {"type": 1, "name": "a"},
        "canonicalize_payload": [{"type": 1, "name": "a"}, {"type": 1, "name": "a"}],
        "patchable_payload": [{"x": 1}, {"x": 1}]})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc([{"name": "a", "payload": {"type": 1, "name": "a"}}]),
        "tree_fetch_commands": sc([{"id": 9, "name": "a", "type": 1}]),
        "existing_command_to_payload": {"type": 1, "name": "a", "description": "d"},
        "canonicalize_payload": [{"type": 1, "name": "a", "description": "d"}, {"type": 1, "name": "a"}],
        "patchable_payload": [{"x": 1}, {"x": 2}]})
add("_safe_sync_slash_commands", args={}, state={"_client": CLIENT},
    io={"tree_get_commands": sc(raise_type="RuntimeError", raise_msg="g")})

# ── 14. _run_post_connect_initialization ───────────────────────────────────
add("_run_post_connect_initialization", args={}, state={"_client": None})
# app_id from application_id
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "off"})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "bulk", "tree_sync": ["c1", "c2"]})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": "skip"})
SUMM = {"total": 2, "unchanged": 1, "updated": 1, "recreated": 0, "created": 0, "deleted": 0}
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": None, "safe_sync": SUMM})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": None, "safe_sync": sc(raise_type="RuntimeError", raise_msg="rate"),
        "is_discord_rate_limit": True, "extract_retry_after": 7})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": None, "safe_sync": sc(raise_type="RuntimeError", raise_msg="rate"),
        "is_discord_rate_limit": True, "extract_retry_after": None})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": None, "wait_for": sc(raise_type="TimeoutError", raise_msg="t")})
add("_run_post_connect_initialization", args={}, state={"_client": dict(CLIENT, application_id=123)},
    io={"command_sync_policy": "normal", "desired_sync_fingerprint": "fp",
        "command_sync_skip_reason": None, "wait_for": sc(raise_type="CancelledError", raise_msg="c")})

# ── 15. _run_missed_message_backfill ───────────────────────────────────────
add("_run_missed_message_backfill", args={}, state={"_client": None})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc([]), "ledger_db": False})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc([]), "ledger_db": True})
M1 = {"id": 1, "author": {"id": "u"}, "channel": {"id": "10"}}
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc([M1]), "dedup_contains": False,
        "should_backfill": False})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc([M1]), "dedup_contains": True})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc([M1]), "dedup_contains": False,
        "should_backfill": True, "dispatch_recovered": True})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc([M1]), "dedup_contains": False,
        "should_backfill": True, "dispatch_recovered": False})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc([M1]), "dedup_contains": False,
        "should_backfill": True, "dispatch_recovered": sc(raise_type="RuntimeError", raise_msg="d")})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": True,
        "iter_backfill_candidates": sc(raise_type="CancelledError", raise_msg="c")})
add("_run_missed_message_backfill", args={},
    io={"backfill_channels": sc(["10"]), "ledger_db": sc(raise_type="RuntimeError", raise_msg="l")})

# ── 16. _resolve_allowed_usernames ─────────────────────────────────────────
def au(allowed, guilds=None, prior=None, name="bot", snap=None, io=None):
    a = {"allowed_user_ids": allowed, "name": name}
    if guilds is not None:
        a["guilds"] = guilds
    st = {}
    if prior is not None:
        st["_username_to_id"] = prior
    if snap is not None:
        a["gate_env_snapshot"] = snap
    add("_resolve_allowed_usernames", args=a, state=st, io=io)


def member(name, uid, display=None, glob=None, disc="1"):
    return {"name": name, "id": uid, "display_name": display, "global_name": glob,
            "discriminator": disc}


G_ALICE = {"name": "g1", "member_count": 1, "members": [member("Alice", 111, "A", None, "1")]}
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["123", "456"], "name": "bot"})
add("_resolve_allowed_usernames", args={"allowed_user_ids": [], "name": "bot"})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["alice"], "name": "bot"})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["alice"], "name": "bot",
        "guilds": [G_ALICE]})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["*", "999"], "name": "bot"})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["nick"], "name": "bot",
        "guilds": [{"name": "g1", "member_count": 1,
                    "members": [member("Bob", 222, "Nick", None, "2")]}]})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["alice"], "name": "bot",
        "guilds": [{"name": "g1", "member_count": 2, "members": []}]},
    io={"au_fetch_members": sc([member("Alice", 111, "A", None, "1")])})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["alice"], "name": "bot",
        "guilds": [{"name": "g1", "member_count": 2, "members": []}]},
    io={"au_fetch_members": sc(raise_type="RuntimeError", raise_msg="f")})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["111", "333"], "name": "bot",
        "guilds": [G_ALICE], "gate_env_snapshot": {}},
    state={"_username_to_id": {"alice": "111", "bob": "222"}},
    io={"multiplex_active": True})
add("_resolve_allowed_usernames", args={"allowed_user_ids": ["alice"], "name": "bot",
        "guilds": [G_ALICE], "gate_env_snapshot": {}},
    io={"au_print": sc(raise_type="RuntimeError", raise_msg="p")})

EXPECTED_FUNCS = {
    "_reset_voice_timeout", "_voice_timeout_limit", "_playback_timeout_limit",
    "_resolve_channel", "_resolve_interaction_channel", "_resolve_channel_skills",
    "_resolve_credentials", "_standalone_read_json_limited", "_standalone_is_forum",
    "_should_backfill_discord_message", "_self_contained_prompt_content",
    "_run_simple_slash", "_safe_sync_slash_commands", "_run_post_connect_initialization",
    "_run_missed_message_backfill",
    "_resolve_allowed_usernames",
}

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert set(counts) == EXPECTED_FUNCS, (EXPECTED_FUNCS - set(counts), set(counts) - EXPECTED_FUNCS)
assert all(v >= 6 for v in counts.values()), {k: v for k, v in counts.items() if v < 6}
all_io = set()
for c in CASES:
    all_io |= set(c["in"]["io"])
assert all_io == set(SEAM_NAMES), (set(SEAM_NAMES) - all_io, all_io - set(SEAM_NAMES))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT} ({len(counts)} funcs, {len(all_io)} seams)")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
