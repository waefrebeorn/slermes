#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow12.jsonl for the Slack
plugin-adapter FLOW port, wave 12 (src/gateway/platforms/slack_adapter_flow12.c).

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
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow12.jsonl")
SEAMS = [
    "app_action", "app_command", "app_event",
    "chat_postEphemeral", "chat_postMessage",
    "conversations_info", "conversations_replies",
    "drop_bot_sender",
    "event_team_id",
    "extract_additional_text_from_slack_blocks", "extract_text_from_slack_attachments",
    "extract_urls_from_slack_blocks",
    "get_client", "get_scoped_secret", "get_slack_action_handlers",
    "has_app",
    "is_duplicate", "is_enabled_debug", "is_foreign_author", "is_ignored_channel",
    "logger.debug", "logger.info", "logger.warning",
    "metadata_team_id",
    "normalize_changed_message",
    "read_chunk", "re_compile", "re_search", "re_split",
    "reactions_add", "reactions_remove",
    "refusal_text", "register_plugin_action_handlers", "register_vault_code_handlers",
    "release_response", "resolve_thread_ts", "resolve_user_name", "response_text",
    "sanitize_blocks", "slack_file_marker", "slack_native_slashes",
    "slack_response_payload",
    "time.time",
    "trim_bot_message_timestamps", "trim_oldest_dict_entries",
    "unescape_slack_entities", "users_info",
    "wire_plugin_handlers", "workspace_event_id", "workspace_message_marker",
]

BASE = {n: None for n in SEAMS}
BASE.update({
    "has_app": True,
    "get_client": {"id": "client"},
    "app_event": {"ok": True}, "app_action": {"ok": True}, "app_command": {"ok": True},
    "slack_native_slashes": ["help", "status"],
    "re_compile": {"pattern": "regex"},
    "re_search": None, "re_split": ["a", "b"],
    "get_scoped_secret": None, "get_slack_action_handlers": [],
    "slack_response_payload": {"ok": True},
    "is_enabled_debug": False, "is_duplicate": False, "is_foreign_author": False,
    "is_ignored_channel": False, "drop_bot_sender": False,
    "metadata_team_id": "T", "resolve_thread_ts": "100",
    "workspace_message_marker": "wm", "workspace_event_id": "we",
    "event_team_id": "T1", "refusal_text": "refused",
    "response_text": "body", "time.time": 100,
    "sanitize_blocks": ["b"], "resolve_user_name": "Alice",
    "normalize_changed_message": {"ts": "100", "subtype": "message_changed"},
    "conversations_replies": {"messages": []},
    "conversations_info": {"ok": True, "channel": {"name": "general"}},
    "extract_additional_text_from_slack_blocks": "",
    "extract_text_from_slack_attachments": "",
    "extract_urls_from_slack_blocks": [],
    "slack_file_marker": "file", "unescape_slack_entities": "T",
    "users_info": {"user": {"team_id": "T1"}},
    "reactions_add": {"ok": True}, "reactions_remove": {"ok": True},
    "chat_postEphemeral": {"ok": True}, "chat_postMessage": {"ok": True},
    "read_chunk": "abc",
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
        # A LIST of script dicts is a sequential-call script; keep it as-is.
        if isinstance(v, list) and not (v and all(
                isinstance(x, dict) and x.get("__script__") for x in v)):
            merged[k] = script(ret=v)
    inp = {"args": dict(args or {}), "io": merged, "state": dict(state or {})}
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


# ------------------------------- _read_error_text_limited
_R = {"response": {"content": {"read": True}, "release": True}, "limit": 5}
case("_read_error_text_limited", _R, io={"read_chunk": [script("abc"), script("defg")]})
case("_read_error_text_limited", {"response": {"content": {"read": True}}, "limit": 5},
     io={"read_chunk": [script("ab"), script("")]})
case("_read_error_text_limited", {"response": {"content": {}}, "limit": 8},
     io={"response_text": "hello world"})
case("_read_error_text_limited", {"response": {"content": None}, "limit": 3},
     io={"response_text": 12345})
case("_read_error_text_limited", _R, io={"read_chunk": [script("abc"), rais("ValueError", "c")]})
case("_read_error_text_limited", _R, io={"read_chunk": rais("SystemExit", "base")},
     expect_raise="SystemExit")
case("_read_error_text_limited", {"response": {"content": {}}, "limit": 4},
     io={"response_text": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _react
case("_react", {"channel": "C", "timestamp": "100", "emoji": "eyes", "team_id": "T", "remove": False})
case("_react", {"channel": "C", "timestamp": "100", "emoji": "eyes", "team_id": "", "remove": True})
case("_react", {"channel": "C", "timestamp": "100", "emoji": "x", "team_id": "T", "remove": False},
     io={"has_app": False})
case("_react", {"channel": "C", "timestamp": "100", "emoji": "x", "team_id": "T", "remove": True},
     io={"reactions_remove": rais("ValueError", "scope")})
case("_react", {"channel": "C", "timestamp": "100", "emoji": "x", "team_id": "T", "remove": False},
     io={"reactions_add": rais("ValueError", "exists")})
case("_react", {"channel": "C", "timestamp": "100", "emoji": "x", "team_id": "T", "remove": False},
     io={"reactions_add": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_react", {"channel": "C", "timestamp": "100", "emoji": "x", "team_id": "T", "remove": False},
     io={"reactions_add": rais("ValueError", "e"), "logger.debug": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _remove_reaction
_RR = {"channel": "C", "timestamp": "100", "emoji": "eyes", "team_id": "T"}
case("_remove_reaction", _RR)
case("_remove_reaction", dict(_RR, team_id=""))
case("_remove_reaction", _RR, io={"has_app": False})
case("_remove_reaction", _RR, io={"reactions_remove": {"ok": False}})
case("_remove_reaction", _RR, io={"reactions_remove": rais("ValueError", "x")})
case("_remove_reaction", _RR, io={"reactions_remove": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _record_uploaded_file_thread
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": ""})
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": "100"})
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": "200", "metadata": {"t": "T2"}},
     state={"_bot_message_ts": ["wm"]})
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": "100"},
     io={"workspace_message_marker": "wm2"})
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": "100"},
     io={"metadata_team_id": rais("ValueError", "m")})
case("_record_uploaded_file_thread", {"chat_id": "C", "thread_ts": "100"},
     io={"trim_bot_message_timestamps": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _remember_processed_message_ts
case("_remember_processed_message_ts", {"ts": "", "max_size": 3})
case("_remember_processed_message_ts", {"ts": "t1", "max_size": 3})
case("_remember_processed_message_ts", {"ts": "t1", "max_size": 3},
     state={"_processed_message_ts": {"t1": 1, "t2": 2, "t3": 3}})
case("_remember_processed_message_ts", {"ts": "t1", "max_size": 2},
     state={"_processed_message_ts": {"t0": 5, "t2": 1, "t3": 3}})
case("_remember_processed_message_ts", {"ts": "t1", "max_size": 3},
     io={"time.time": rais("ValueError", "t")})
case("_remember_processed_message_ts", {"ts": "t1", "max_size": 3},
     io={"time.time": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _refuse_other_requester
_RF = {"requester": "U1", "body": {"channel": {"id": "C"}, "user": {"id": "U2"},
                                   "message": {"thread_ts": "100"}}}
case("_refuse_other_requester", {"requester": "", "body": {}})
case("_refuse_other_requester", _RF)
case("_refuse_other_requester", {"requester": "U1", "body": {}})
case("_refuse_other_requester", _RF, io={"event_team_id": ""})
case("_refuse_other_requester", _RF, io={"chat_postEphemeral": rais("ValueError", "c")})
case("_refuse_other_requester", _RF, io={"chat_postEphemeral": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _post_interactive_blocks
_PI = {"chat_id": "C", "text": "hi", "blocks": [{"type": "section"}],
       "metadata": {"team": "T"}, "sanitize": True, "team_scoped": True}
case("_post_interactive_blocks", _PI)
case("_post_interactive_blocks", dict(_PI, sanitize=False, team_scoped=False))
case("_post_interactive_blocks", dict(_PI, metadata=None), io={"resolve_thread_ts": ""})
case("_post_interactive_blocks", _PI, io={"resolve_thread_ts": "100"})
case("_post_interactive_blocks", _PI, io={"sanitize_blocks": rais("ValueError", "s")})
case("_post_interactive_blocks", _PI, io={"chat_postMessage": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _render_message_text
case("_render_message_text", {"msg": {"text": "  hi <@B1>  "}, "bot_uid": "B1"})
case("_render_message_text", {"msg": {"text": "hello"}, "bot_uid": ""})
case("_render_message_text", {"msg": {"text": "hi", "blocks": [
    {"type": "section", "text": {"text": " hidden "}},
    {"type": "divider"},
    {"type": "header", "text": {"text": "Title"}}]}, "bot_uid": ""},
    io={"extract_additional_text_from_slack_blocks": " rich ",
        "extract_urls_from_slack_blocks": ["http://x"],
        "unescape_slack_entities": "hi"})
case("_render_message_text", {"msg": {"text": "hi", "attachments": [{"text": "att"}],
                                      "files": [{"id": "f1"}, "notadict"]}, "bot_uid": ""},
     io={"extract_text_from_slack_attachments": " att ", "slack_file_marker": "[file]"})
case("_render_message_text", {"msg": {"text": "", "blocks": []}, "bot_uid": ""})
case("_render_message_text", {"msg": {"text": "hi", "blocks": [{"type": "section"}]}, "bot_uid": ""},
     io={"extract_additional_text_from_slack_blocks": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _resolve_author_team
_RA = {"user_id": "U1", "channel_id": "C", "team_id": "T1"}
case("_resolve_author_team", {"user_id": "", "channel_id": "C", "team_id": "T1"})
case("_resolve_author_team", _RA, state={"_user_team_cache": {"T1|U1": "T9"}})
case("_resolve_author_team", _RA)
case("_resolve_author_team", _RA, io={"users_info": {"user": {}}})
case("_resolve_author_team", _RA, io={"users_info": rais("ValueError", "u")})
case("_resolve_author_team", _RA, io={"slack_response_payload": rais("ValueError", "p")})
case("_resolve_author_team", _RA, io={"users_info": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _reaction_thread_ts
_RT = {"client": {"id": "client"}, "channel_id": "C", "msg_ts": "100",
       "event": {}, "team_id": "T1", "explicit_allowlist": False}
case("_reaction_thread_ts", dict(_RT, client=None))
case("_reaction_thread_ts", _RT)
case("_reaction_thread_ts", dict(_RT, event={"item_user": "U1"}),
     io={"conversations_replies": {"messages": [{"thread_ts": "200", "user": "U1"}]}})
case("_reaction_thread_ts", _RT,
     io={"conversations_replies": {"messages": [{"ts": "300"}]}},
     state={"_team_bot_user_ids": {"T1": "B1"}, "_bot_user_id": "B0"})
case("_reaction_thread_ts", dict(_RT, event={"item_user": "OTHER"}),
     io={"conversations_replies": {"messages": [{"thread_ts": "200"}]}},
     state={"_team_bot_user_ids": {"T1": "B1"}})
case("_reaction_thread_ts", dict(_RT, explicit_allowlist=True),
     io={"conversations_replies": {"messages": [{"thread_ts": "200"}]}})
case("_reaction_thread_ts", _RT, io={"conversations_replies": rais("ValueError", "c")})
case("_reaction_thread_ts", _RT, io={"conversations_replies": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _resolve_channel_name
_RC = {"channel_id": "C", "team_id": "T1"}
case("_resolve_channel_name", {"channel_id": ""})
case("_resolve_channel_name", _RC, state={"_channel_name_cache": {"T1|C": "cached"}})
case("_resolve_channel_name", _RC)
case("_resolve_channel_name", _RC, io={"conversations_info": {"ok": False}})
case("_resolve_channel_name", dict(_RC, team_id=""),
     io={"conversations_info": {"ok": True, "channel": {"is_im": True, "user": "U1"}}})
case("_resolve_channel_name", _RC, io={"conversations_info": rais("ValueError", "c")})
case("_resolve_channel_name", _RC, io={"conversations_info": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _prefilter_inbound
_PF = {"event": {"type": "message", "channel": "C", "ts": "100", "user": "U1"}, "payload": {}}
case("_prefilter_inbound", _PF)
case("_prefilter_inbound", _PF, io={"is_enabled_debug": True})
case("_prefilter_inbound", {"event": {"subtype": "message_changed", "channel": "C"}, "payload": {}})
case("_prefilter_inbound", _PF, state={"_team_clients": {"T1": {"id": "c"}}},
     io={"event_team_id": ""})
case("_prefilter_inbound", _PF, io={"is_duplicate": True})
case("_prefilter_inbound", _PF, io={"is_ignored_channel": True})
case("_prefilter_inbound", {"event": {"channel": "D1", "user": "U1", "user_team": "TX"}, "payload": {}},
     io={"is_foreign_author": True})
case("_prefilter_inbound", _PF, io={"drop_bot_sender": True})
case("_prefilter_inbound", {"event": {"channel": "C", "subtype": "channel_join"}, "payload": {}})
case("_prefilter_inbound", _PF, io={"is_duplicate": rais("SystemExit", "base")},
     expect_raise="SystemExit")

# ------------------------------- _register_bolt_handlers
_RBH_STATE = {"_APPROVAL_CHOICES": ["approve", "deny"],
              "_CONFIRM_CHOICES": ["confirm_yes"],
              "_MODEL_PICKER_ACTION_IDS": ["model_provider", "model_name"]}
case("_register_bolt_handlers", {}, state=_RBH_STATE)
case("_register_bolt_handlers", {}, state={"_APPROVAL_CHOICES": [], "_CONFIRM_CHOICES": [],
                                           "_MODEL_PICKER_ACTION_IDS": []})
case("_register_bolt_handlers", {}, state=_RBH_STATE,
     io={"slack_native_slashes": []})
case("_register_bolt_handlers", {}, state=_RBH_STATE,
     io={"app_event": rais("ValueError", "e")})
case("_register_bolt_handlers", {}, state=_RBH_STATE,
     io={"slack_native_slashes": rais("SystemExit", "base")}, expect_raise="SystemExit")
case("_register_bolt_handlers", {}, state=_RBH_STATE,
     io={"wire_plugin_handlers": rais("ValueError", "w")})

# ------------------------------- _register_plugin_action_handlers
_RPA = [["a1", {"fn": 1}, "plugA"], ["a2", {"fn": 2}, "plugB"]]
case("_register_plugin_action_handlers", {}, io={"get_slack_action_handlers": _RPA})
case("_register_plugin_action_handlers", {}, io={"get_slack_action_handlers": []})
case("_register_plugin_action_handlers", {}, io={"get_slack_action_handlers": rais("ValueError", "g")})
case("_register_plugin_action_handlers", {}, io={"get_slack_action_handlers": rais("SystemExit", "base")},
     expect_raise="SystemExit")
case("_register_plugin_action_handlers", {}, io={"get_slack_action_handlers": _RPA,
     "app_action": rais("ValueError", "a")})
case("_register_plugin_action_handlers", {}, state={"_app": "APP", "_plugin_actions_app": "APP",
     "_plugin_actions_wired": {"a1|plugA": True}}, io={"get_slack_action_handlers": _RPA})

# ------------------------------- _resolve_download_token
_RDT = {"_team_clients": {"T1": {"token": "tok1"}, "T2": {"token": "tok2"}}, "config": {"token": "primary"}}
case("_resolve_download_token", {"url": "https://x", "team_id": "T1"}, state=_RDT)
case("_resolve_download_token", {"url": "https://x", "team_id": "NONE"}, state=_RDT)
case("_resolve_download_token", {"url": "https://x/files-pri/T2-abc/file", "team_id": ""}, state=_RDT,
     io={"re_search": {"group1": "T2"}})
case("_resolve_download_token", {"url": "https://x", "team_id": ""}, state=_RDT,
     io={"re_search": None})
case("_resolve_download_token", {"url": "https://x/files-pri/T9-abc", "team_id": ""}, state=_RDT,
     io={"re_search": {"group1": "T9"}})
case("_resolve_download_token", {"url": "https://x", "team_id": ""}, state=_RDT,
     io={"re_search": rais("ValueError", "r")})
case("_resolve_download_token", {"url": "https://x", "team_id": ""}, state=_RDT,
     io={"re_search": rais("SystemExit", "base")}, expect_raise="SystemExit")

# ------------------------------- _slack_reaction_triggers
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": None}}},
     io={"get_scoped_secret": None})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": True}}})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": False}}})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {
    "reaction_triggers": [" eyes ", "thumbsup", ""]}}})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": "false"}}})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": "all"}}})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": " eyes, tada "}}},
     io={"re_split": [" eyes", " tada "]})
case("_slack_reaction_triggers", {}, state={"config": {"extra": {"reaction_triggers": None}}},
     io={"get_scoped_secret": rais("SystemExit", "base")}, expect_raise="SystemExit")


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
