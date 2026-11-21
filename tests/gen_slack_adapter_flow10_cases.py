#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow10.jsonl for the Slack
plugin-adapter FLOW port, wave 10 (src/gateway/platforms/slack_adapter_flow10.c).

Deterministic corpus: >=6 cases per ported function (16 funcs), covering pure
logic, chained seams with ORDERED traces, fail-closed missing scripts and
BaseException passthrough. Case shape:
{"func", "in": {"args","io"?,"state"}, "expect_raise"?}.

Every case carries the FULL seam io-key union (baseline + overrides) so the
fixture io keys equal SEAM_NAMES exactly (SET EQUALITY).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow10.jsonl")
SEAMS = [
    "_handle_slack_message_impl", "ack", "agent_view_context_for_event",
    "append_block_text", "append_forwards", "append_link_unfurls",
    "apply_bot_mention", "begin_interaction", "build_message_event",
    "build_model_picker_model_blocks", "build_model_picker_provider_blocks",
    "build_source", "build_thread_session_key", "cache_assistant_thread_metadata",
    "channel_gate_allows", "channel_prompt_with_identity", "channel_team",
    "chat_postEphemeral", "chat_postMessage", "chat_update", "clarify_entries_get",
    "clarify_mark_awaiting_text", "clarify_resolve", "client",
    "collect_inbound_media", "dedup_is_duplicate", "describe_slack_api_error",
    "ea_deadline_line", "early_reject_unauthorized", "event_command",
    "event_declares_bot_sender", "event_session_key", "event_team_id",
    "extract_assistant_thread_metadata", "files_info", "finalize_interactive_message",
    "first_file_share", "get_client", "get_label", "get_scoped_secret",
    "handle_message", "handle_slack_message", "handle_slash_command",
    "hydrate_thread_context", "is_foreign_author", "is_ignored_channel",
    "is_interactive_user_authorized", "is_mentioned", "logger.debug",
    "logger.error", "logger.info", "logger.warning",
    "lookup_assistant_thread_metadata", "model_picker_expired_notice",
    "monotonic", "msg_event_set_text", "on_model_selected", "peer_bot_drop",
    "picker_refusal", "picker_state_get", "picker_state_pop", "picker_state_set",
    "prefilter_inbound", "reaction_handler_call", "reaction_handler_get",
    "reaction_thread_ts", "reactions_enabled", "refusal_text",
    "refuse_other_requester", "remember_channel_team", "resolve_approval",
    "resolve_author_team", "resolve_channel_name", "resolve_channel_skills",
    "resolve_command", "resolve_slash_confirm", "resolve_user_is_bot",
    "resolve_user_name", "rewrite_known_bang_command", "sanitize_blocks",
    "section_text", "seed_dm_session", "session_ensure_loaded", "session_entry",
    "session_thread_ts", "set_assistant_suggested_prompts", "slack_allow_bots",
    "slack_disable_dms", "slack_mention_detection_text", "slack_reaction_triggers",
    "slack_reply_expected", "slash_channel_gated", "slash_command_text",
    "slash_lanes_release", "slash_lanes_reserve", "slash_lanes_wait",
    "slash_thread_id", "slash_user_id_reset", "slash_user_id_set", "sleep",
    "source_set_fields", "stash_slash_context", "synthetic_reaction_event",
    "synthetic_set", "t_error_prefix", "team_bot_user_id", "team_clients",
    "team_clients_first", "team_clients_get", "team_in_install_set", "time",
    "track_reacting_message", "update_clarify_message", "update_picker_message",
    "workspace_event_id", "workspace_message_marker",
]

BASE = {n: None for n in SEAMS}
BASE.update({
    "begin_interaction": ["T1", "approve", "S", "msg", "ts", "C", "U", "USR"],
    "prefilter_inbound": [{"text": "hi", "ts": "100"}, "T1", "C1"],
    "picker_state_get": {"providers": [{"slug": "openai", "models": ["gpt"]}],
                         "on_model_selected": None, "stage": "provider"},
    "clarify_entries_get": {"choices": ["A", "B"]},
    "channel_team": "T1", "team_bot_user_id": "B", "event_team_id": "T1",
    "agent_view_context_for_event": {}, "hydrate_thread_context": ["ctx", [], []],
    "collect_inbound_media": [[], [], False, "hi"], "build_message_event": {"text": "hi"},
    "synthetic_reaction_event": {"channel": "C1"}, "first_file_share": {},
    "resolve_command": {"name": "x", "busy_policy": ""},
    "slash_lanes_reserve": ["a", "h"], "event_session_key": "lane",
    "reaction_thread_ts": "100", "files_info": {"ok": True, "file": {"mimetype": "video/mp4"}},
    "rewrite_known_bang_command": "", "append_block_text": "hi",
    "append_forwards": "hi", "append_link_unfurls": "hi",
    "section_text": "orig", "ea_deadline_line": "(deadline)",
    "model_picker_expired_notice": "expired", "get_label": "OpenAI",
    "build_model_picker_model_blocks": ["b"], "build_model_picker_provider_blocks": ["b"],
    "sanitize_blocks": ["b"], "on_model_selected": "ok", "t_error_prefix": "Error:",
    "slack_allow_bots": "all", "session_entry": {"suspended": False},
    "build_thread_session_key": "sk", "client": {"ping_interval": 10, "last_ping_pong_time": 1},
    "monotonic": 100.0, "time": 200.0, "resolve_author_team": "T1",
    "slack_reaction_triggers": [], "is_mentioned": False, "channel_gate_allows": True,
    "early_reject_unauthorized": False, "slash_channel_gated": False,
    "slash_command_text": "/x", "slash_thread_id": "", "resolve_slash_confirm": "done",
    "resolve_approval": 1, "refuse_other_requester": False, "picker_refusal": None,
    "clarify_mark_awaiting_text": True, "clarify_resolve": True,
    "is_ignored_channel": False, "is_interactive_user_authorized": True,
    "is_foreign_author": False, "dedup_is_duplicate": False,
    "workspace_message_marker": "T1:ts", "workspace_event_id": "eid",
    "slack_mention_detection_text": "", "event_declares_bot_sender": False,
    "resolve_user_is_bot": False, "peer_bot_drop": False,
    "reactions_enabled": False, "slack_reply_expected": False,
    "team_in_install_set": "T1", "get_scoped_secret": "", "resolve_channel_name": "chan",
    "resolve_user_name": "user", "event_command": "/x", "is_mentioned": False,
    "client": {"ping_interval": 10, "last_ping_pong_time": 1},
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
    # A LIST is the per-call result sequence in the seam backend; wrap list DATA
    # values as single-value scripts so they are returned verbatim.
    for k, v in merged.items():
        if isinstance(v, list):
            merged[k] = script(ret=v)
    inp = {"args": dict(args or {}), "io": merged, "state": dict(state or {})}
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


def s8(action="approve", value="", msg_ts="ts", channel="C", user="USR", team="T1"):
    return [team, action, "S", {"ts": msg_ts}, msg_ts, channel, "U", user]


# ------------------------------- _hint_allow_bots
case("_hint_allow_bots", io={"slack_allow_bots": "none"})
case("_hint_allow_bots", io={"slack_allow_bots": "all"})
case("_hint_allow_bots", io={"slack_allow_bots": "mentions"})
case("_hint_allow_bots", io={"slack_allow_bots": ""})
case("_hint_allow_bots", io={"slack_allow_bots": script("all")})
case("_hint_allow_bots", io={"slack_allow_bots": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _slack_reaction_trigger_target
case("_slack_reaction_trigger_target", {"extra": {}})
case("_slack_reaction_trigger_target", {"extra": {"reaction_trigger_target": "C1"}})
case("_slack_reaction_trigger_target", {"extra": {"reaction_trigger_target": " C1 : 100 "}})
case("_slack_reaction_trigger_target", {"extra": {"reaction_trigger_target": ""}})
case("_slack_reaction_trigger_target", {"extra": {"reaction_trigger_target": None}},
     io={"get_scoped_secret": "C2:TH"})
case("_slack_reaction_trigger_target", {"extra": {}},
     io={"get_scoped_secret": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _socket_ping_pong_stale
case("_socket_ping_pong_stale", {"stale_factor": 2, "grace_s": 0, "started": None},
     io={"client": {"ping_interval": 10, "last_ping_pong_time": 1}, "time": 30})
case("_socket_ping_pong_stale", {"stale_factor": 2, "started": None},
     io={"client": None})
case("_socket_ping_pong_stale", {"stale_factor": 2, "started": None},
     io={"client": {"ping_interval": 0}})
case("_socket_ping_pong_stale", {"stale_factor": 2, "started": 90, "grace_s": 0},
     io={"client": {"ping_interval": 10}, "monotonic": 200})
case("_socket_ping_pong_stale", {"stale_factor": 2, "started": None},
     io={"client": {"ping_interval": 10, "last_ping_pong_time": "x"}})
case("_socket_ping_pong_stale", {"stale_factor": 2, "started": None},
     io={"client": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _has_active_session_for_thread
case("_has_active_session_for_thread", {"session_store": None})
case("_has_active_session_for_thread", {"session_store": {"x": 1}},
     io={"build_thread_session_key": "", "session_ensure_loaded": None, "session_entry": None})
case("_has_active_session_for_thread", {"session_store": {"x": 1}},
     io={"build_thread_session_key": "sk", "session_ensure_loaded": None,
         "session_entry": {"suspended": False}})
case("_has_active_session_for_thread", {"session_store": {"x": 1}},
     io={"build_thread_session_key": "sk", "session_ensure_loaded": None,
         "session_entry": {"suspended": True}})
case("_has_active_session_for_thread", {"session_store": {"x": 1}},
     io={"build_thread_session_key": "sk", "session_ensure_loaded": rais("ValueError", "x"),
         "session_entry": None})
case("_has_active_session_for_thread", {"session_store": {"x": 1}},
     io={"build_thread_session_key": "sk", "session_ensure_loaded": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _handle_feedback_action
case("_handle_feedback_action", {"action": {"value": "yes"},
     "body": {"message": {"ts": "1"}, "channel": {"id": "C"}, "user": {"id": "U"}}})
case("_handle_feedback_action", {"action": {"value": None}, "body": {}})
case("_handle_feedback_action", {"action": {}, "body": {"message": None, "channel": {"id": "C2"}}})
case("_handle_feedback_action", {"action": {"value": 5}, "body": {"user": {"id": "U"}}})
case("_handle_feedback_action", {"action": {"value": "no"}, "body": {}},
     io={"ack": None})
case("_handle_feedback_action", {"action": {"value": "x"}, "body": {}},
     io={"ack": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_hermes_command
case("_handle_hermes_command", {"command": {"channel_id": "C", "command": "/hermes hi"}},
     io={"slash_channel_gated": True, "handle_slash_command": None})
case("_handle_hermes_command", {"command": {"channel_id": "C", "command": "/stop"}},
     io={"slash_channel_gated": False, "handle_slash_command": None})
case("_handle_hermes_command", {"command": {"channel_id": "", "command": ""}},
     io={"slash_channel_gated": False, "handle_slash_command": None})
case("_handle_hermes_command", {"command": {"channel_id": "C"}},
     io={"slash_channel_gated": False, "handle_slash_command": None})
case("_handle_hermes_command", {"command": {"channel_id": "C", "command": "///x"}},
     io={"slash_channel_gated": False, "handle_slash_command": None})
case("_handle_hermes_command", {"command": {"channel_id": "C"}},
     io={"slash_channel_gated": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_assistant_thread_lifecycle_event
case("_handle_assistant_thread_lifecycle_event",
     {"event": {"tab": "messages"}, "body": {}},
     io={"extract_assistant_thread_metadata": {"thread_ts": "", "channel_id": "C", "team_id": "T"}})
case("_handle_assistant_thread_lifecycle_event",
     {"event": {}, "body": {}},
     io={"extract_assistant_thread_metadata": {"thread_ts": "100", "channel_id": "C", "team_id": "T"}})
case("_handle_assistant_thread_lifecycle_event",
     {"event": {}, "body": {}},
     io={"extract_assistant_thread_metadata": {"thread_ts": None, "channel_id": ""}})
case("_handle_assistant_thread_lifecycle_event",
     {"event": {}, "body": {}},
     io={"extract_assistant_thread_metadata": rais("ValueError", "x")}, expect_raise="ValueError")
case("_handle_assistant_thread_lifecycle_event",
     {"event": {}, "body": {}},
     io={"extract_assistant_thread_metadata": {"thread_ts": ""},
         "cache_assistant_thread_metadata": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_handle_assistant_thread_lifecycle_event",
     {"event": {}, "body": {}},
     io={"extract_assistant_thread_metadata": {"thread_ts": "1", "channel_id": "C"},
         "seed_dm_session": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_approval_action
_ST = "T1:ts"
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("approve", msg_ts="ts"),
         "workspace_message_marker": _ST, "section_text": "orig", "ea_deadline_line": "(deadline)",
         "finalize_interactive_message": None})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("deny", msg_ts="ts"), "section_text": "orig",
         "finalize_interactive_message": None, "logger.info": None},
     state={"_approval_resolved": {"ts": False}})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": None})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("approve", msg_ts="ts"), "refuse_other_requester": True})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("approve", msg_ts="ts"), "section_text": "orig",
         "resolve_approval": 0, "finalize_interactive_message": None})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("approve", msg_ts="ts"), "resolve_approval": rais("ValueError", "x"),
         "logger.error": None, "section_text": "orig", "finalize_interactive_message": None})
case("_handle_approval_action", {"body": {}, "extra": {}},
     io={"begin_interaction": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_slash_confirm_action
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("confirm", value="sid|cid", msg_ts="ts"),
         "section_text": "orig", "finalize_interactive_message": None,
         "resolve_slash_confirm": "done", "logger.info": None})
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("cancel", value="sid|cid", msg_ts="ts"),
         "section_text": "orig", "finalize_interactive_message": None,
         "resolve_slash_confirm": ""})
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("confirm", value="bad", msg_ts="ts")})
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("confirm", value="sid|cid", msg_ts="ts"),
         "section_text": "orig", "finalize_interactive_message": None,
         "resolve_slash_confirm": rais("ValueError", "x"), "logger.error": None})
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("confirm", value="sid|cid", msg_ts="ts"),
         "refuse_other_requester": True})
case("_handle_slash_confirm_action", {"body": {}, "extra": {}},
     io={"begin_interaction": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_clarify_action
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("hermes_clarify_0", value="cid|0", msg_ts="ts"),
         "section_text": "orig", "clarify_entries_get": {"choices": ["A", "B"]},
         "clarify_resolve": True, "update_clarify_message": None})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("hermes_clarify_other", value="cid|other", msg_ts="ts"),
         "section_text": "orig", "clarify_mark_awaiting_text": True,
         "update_clarify_message": None})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("hermes_clarify_other", value="cid|other", msg_ts="ts"),
         "section_text": "orig", "clarify_mark_awaiting_text": False,
         "update_clarify_message": None})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("x", value="cid|bad", msg_ts="ts"), "section_text": "orig"})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("x", value="bad", msg_ts="ts")})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": s8("x", value="cid|0", msg_ts="ts"),
         "section_text": "orig", "clarify_resolve": False, "update_clarify_message": None})
case("_handle_clarify_action", {"body": {}, "extra": {}},
     io={"begin_interaction": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_model_picker_action
_MPBASE = {"event": {}, "body": {}, "install_set": None, "extra": {},
           "action": {"action_id": "model_picker_cancel"}}
case("_handle_model_picker_action", _MPBASE,
     io={"event_team_id": "T1", "picker_state_get": {"providers": [], "stage": "provider"},
         "update_picker_message": None})
case("_handle_model_picker_action", dict(_MPBASE, action={"action_id": "model_picker_provider",
     "selected_option": {"value": "0"}}),
     io={"event_team_id": "T1",
         "picker_state_get": {"providers": [{"slug": "openai", "models": ["gpt"], "name": "OpenAI"}],
                              "on_model_selected": None, "stage": "provider"},
         "get_client": None, "sanitize_blocks": ["b"], "chat_update": None,
         "build_model_picker_model_blocks": ["b"]})
case("_handle_model_picker_action", dict(_MPBASE, action={"action_id": "model_picker_back"}),
     io={"event_team_id": "T1", "picker_state_get": {"providers": [], "stage": "provider"},
         "get_label": "OpenAI", "build_model_picker_provider_blocks": ["b"],
         "get_client": None, "sanitize_blocks": ["b"], "chat_update": None})
case("_handle_model_picker_action", dict(_MPBASE, action={"action_id": "model_picker_model",
     "selected_option": {"value": "0"}}),
     io={"event_team_id": "T1",
         "picker_state_get": {"providers": [{"slug": "openai", "models": ["gpt"]}],
                              "on_model_selected": "cb", "stage": "model",
                              "selected_provider_slug": "openai", "chat_id": "C"},
         "on_model_selected": "ok", "t_error_prefix": "Error:", "update_picker_message": None})
case("_handle_model_picker_action", _MPBASE,
     io={"event_team_id": "T1", "is_interactive_user_authorized": False, "logger.warning": None})
case("_handle_model_picker_action", _MPBASE,
     io={"event_team_id": "T1", "picker_state_get": None, "model_picker_expired_notice": "expired",
         "update_picker_message": None})
case("_handle_model_picker_action", _MPBASE,
     io={"event_team_id": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_slash_command
_SC = {"command": {"user_id": "U", "channel_id": "C", "team_id": "T", "command": "/x",
                   "response_url": "url"}}
case("_handle_slash_command", _SC, io={"slash_channel_gated": True, "logger.debug": None})
case("_handle_slash_command", _SC, io={"slash_channel_gated": False, "slack_disable_dms": True,
     "logger.info": None})
case("_handle_slash_command", _SC, io={"slash_channel_gated": False,
     "early_reject_unauthorized": True})
case("_handle_slash_command", _SC, io={"slash_channel_gated": False,
     "resolve_command": {"name": "approve", "busy_policy": ""}, "handle_message": None,
     "slash_user_id_set": "t", "slash_user_id_reset": None, "slash_lanes_release": None,
     "event_command": "/approve", "stash_slash_context": None})
case("_handle_slash_command", _SC, io={"slash_channel_gated": False,
     "resolve_command": {"name": "x", "busy_policy": ""}, "handle_message": None,
     "slash_user_id_set": "t", "slash_user_id_reset": None, "slash_lanes_release": None,
     "event_command": "/x", "resolve_channel_name": "chan", "resolve_user_name": "user",
     "source_set_fields": None, "slash_lanes_wait": None, "stash_slash_context": None})
case("_handle_slash_command", {"command": {"user_id": "U", "channel_id": "D1", "command": "hi"}},
     io={"slash_channel_gated": False, "slack_disable_dms": False,
         "early_reject_unauthorized": False, "handle_message": None,
         "slash_user_id_set": "t", "slash_user_id_reset": None, "slash_lanes_release": None,
         "event_command": "hi", "resolve_command": None, "resolve_channel_name": "chan",
         "resolve_user_name": "user", "source_set_fields": None})
case("_handle_slash_command", _SC,
     io={"slash_channel_gated": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _handle_slack_message
case("_handle_slack_message", {"event": {"ts": "100"}, "name": "slack"},
     io={"_handle_slack_message_impl": None})
case("_handle_slack_message", {"event": {"ts": "100"}, "name": "slack"},
     io={"_handle_slack_message_impl": rais("ValueError", "x"), "logger.warning": None},
     state={"_processed_message_ts": {"100": True}})
case("_handle_slack_message", {"event": {"ts": "100"}, "name": "slack"},
     io={"_handle_slack_message_impl": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_handle_slack_message", {"event": {}}, io={"_handle_slack_message_impl": None})
case("_handle_slack_message", {"event": {"ts": ""}, "name": "n"},
     io={"_handle_slack_message_impl": None})
case("_handle_slack_message", {"event": {"ts": "5"}, "name": "n"},
     io={"_handle_slack_message_impl": None})

# ------------------------------- _handle_slack_message_impl
_IM = {"event": {"text": "hi", "ts": "100", "user": "U", "channel_type": "channel"},
       "payload": None}
_IMIO = {"prefilter_inbound": [{"text": "hi", "ts": "100"}, "T1", "C1"],
         "rewrite_known_bang_command": "", "append_forwards": "hi",
         "append_link_unfurls": "hi", "lookup_assistant_thread_metadata": {},
         "agent_view_context_for_event": {}, "slack_disable_dms": False,
         "session_thread_ts": "100", "event_declares_bot_sender": False,
         "early_reject_unauthorized": False, "team_bot_user_id": "B",
         "slack_mention_detection_text": "", "is_mentioned": False,
         "peer_bot_drop": False, "channel_gate_allows": True,
         "hydrate_thread_context": ["ctx", [], []], "collect_inbound_media": [[], [], False, "hi"],
         "slack_reply_expected": False, "build_message_event": {"text": "hi"},
         "reactions_enabled": False, "handle_message": None}
case("_handle_slack_message_impl", _IM, io=dict(_IMIO))
case("_handle_slack_message_impl", _IM, io=dict(_IMIO, prefilter_inbound=None))
case("_handle_slack_message_impl", _IM, io=dict(_IMIO, early_reject_unauthorized=True))
case("_handle_slack_message_impl", {"event": {"text": "!deploy", "ts": "1", "user": "U",
      "channel_type": "channel"}}, io=dict(_IMIO, rewrite_known_bang_command="/deploy",
      prefilter_inbound=[{"text": "!deploy", "ts": "1"}, "T1", "C1"]))
case("_handle_slack_message_impl", _IM, io=dict(_IMIO, is_mentioned=True,
     apply_bot_mention=["hi", "hi", "/hi", True], reactions_enabled=True,
     track_reacting_message=None))
case("_handle_slack_message_impl", _IM, io=dict(_IMIO, prefilter_inbound=rais("SystemExit", "base")),
     expect_raise="SystemExit")

# ------------------------------- _handle_slack_file_shared
_FS = {"event": {"channel_id": "C", "file_id": "F"}, "body": {}, "extra": {}}
_FSIO = {"is_ignored_channel": False, "event_team_id": "T1",
         "files_info": {"ok": True, "file": {"mimetype": "video/mp4", "user": "U"}},
         "first_file_share": {"ts": "100", "thread_ts": ""}, "sleep": None,
         "dedup_is_duplicate": False, "workspace_event_id": "eid", "handle_slack_message": None}
case("_handle_slack_file_shared", _FS, io=dict(_FSIO))
case("_handle_slack_file_shared", _FS, io=dict(_FSIO, is_ignored_channel=True, **{"logger.info": None}))
case("_handle_slack_file_shared", _FS, io=dict(_FSIO, files_info={"ok": False, "error": "x"},
     describe_slack_api_error="detail", **{"logger.warning": None}))
case("_handle_slack_file_shared", _FS, io=dict(_FSIO,
     files_info={"ok": True, "file": {"mimetype": "text/plain"}}))
case("_handle_slack_file_shared", {"event": {"channel_id": "D1", "file_id": "F", "user_team": "T9"},
      "body": {}, "extra": {}}, io=dict(_FSIO, is_foreign_author=True, **{"logger.info": None},
      describe_slack_api_error="detail", first_file_share={"ts": "1"}))
case("_handle_slack_file_shared", _FS, io=dict(_FSIO, dedup_is_duplicate=True))
case("_handle_slack_file_shared", _FS,
     io=dict(_FSIO, files_info=rais("SystemExit", "base")), expect_raise="SystemExit")

# ------------------------------- _handle_slack_reaction
_RX = {"event": {"item": {"type": "message", "channel": "C", "ts": "100"},
                 "user": "U", "reaction": "eyes"}, "bot_user_id": "B", "removed": False}
_RXIO = {"channel_team": "T1", "team_clients": {"T1": 1}, "team_clients_get": None,
         "reaction_handler_get": None, "slack_reaction_triggers": [],
         "reaction_thread_ts": "100", "synthetic_reaction_event": {"channel": "C"},
         "handle_slack_message": None}
case("_handle_slack_reaction", _RX, io=dict(_RXIO))
case("_handle_slack_reaction", _RX, io=dict(_RXIO, slack_reaction_triggers=None))
case("_handle_slack_reaction", _RX, io=dict(_RXIO, slack_reaction_triggers=["fire"]))
case("_handle_slack_reaction", dict(_RX, removed=True), io=dict(_RXIO))
case("_handle_slack_reaction", {"event": {"item": {"type": "file", "channel": "C", "ts": "1"},
     "user": "U", "reaction": "x"}, "bot_user_id": "B"}, io=dict(_RXIO))
case("_handle_slack_reaction", dict(_RX, event=dict(_RX["event"], user="B")), io=dict(_RXIO))
case("_handle_slack_reaction", _RX,
     io=dict(_RXIO, channel_team=rais("SystemExit", "base")), expect_raise="SystemExit")


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
