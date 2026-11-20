#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow2.jsonl.

Each case is {"func", "in": {"args", "state", "io"}, "fc"?}. io maps seam
name -> scripted result (plain value reused per call, LIST consumed in call
order, or {"__script__": true, "ret"/"raise"}). fc marks an intentional
fail-closed case (missing script -> RuntimeError). >=5 cases per ported func.
Run: python tests/gen_discord_adapter_flow2_cases.py
"""
import json
import os

CASES = []


def case(func, args=None, state=None, io=None, fc=False):
    inp = {}
    if args is not None:
        inp["args"] = args
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    rec = {"func": func, "in": inp}
    if fc:
        rec["fc"] = True
    CASES.append(rec)


def sc(ret):
    return {"__script__": True, "ret": ret}


def rz(type, message, errno=None):
    d = {"__script__": True, "raise": {"type": type, "message": message}}
    if errno is not None:
        d["raise"]["errno"] = errno
    return d


FLAG = "_extra_or_env_flag"

# 1. _extra_or_env_flag
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "true", "truthy": True},
     io={"_extra_or_secret": "yes"})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "true", "truthy": False},
     io={"_extra_or_secret": "off"})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "true", "truthy": False},
     io={"_extra_or_secret": None})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "false", "truthy": True},
     io={"_extra_or_secret": None})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     io={"_extra_or_secret": True})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": False},
     io={"_extra_or_secret": False})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     io={"_extra_or_secret": " ON "})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": False},
     io={"_extra_or_secret": "maybe"})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     io={"_extra_or_secret": "0"})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     state={"config": {"extra": {"k": "v"}}}, io={"_extra_or_secret": "1"})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     io={"_extra_or_secret": rz("ValueError", "boom")})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True},
     io={"_extra_or_secret": rz("SystemExit", "bye")})
case("_extra_or_env_flag", {"key": "k", "env_key": "E", "env_default": "x", "truthy": True}, fc=True)

# 2-5. flag wrappers
for fn in ("_discord_require_mention", "_discord_free_response_auto_thread",
           "_discord_thread_require_mention", "_discord_history_backfill"):
    case(fn, io={FLAG: True})
    case(fn, io={FLAG: False})
    case(fn, io={FLAG: rz("ValueError", "boom")})
    case(fn, io={FLAG: rz("SystemExit", "bye")})
    case(fn, fc=True)

# 6. _discord_max_attachment_bytes
D32 = 32 * 1024 * 1024
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": "10"}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": 0}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": -5}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": 1024}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": True}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {}}},
     io={"_scoped_gate_env": "20"})
case("_discord_max_attachment_bytes", state={"config": {"extra": {}}},
     io={"_scoped_gate_env": ""})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": ""}}})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": "abc"}}},
     io={"logger.warning": None})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": []}}},
     io={"logger.warning": None})
case("_discord_max_attachment_bytes", state={"config": {"extra": None}})
case("_discord_max_attachment_bytes")
case("_discord_max_attachment_bytes", state={"config": {"extra": {}}},
     io={"_scoped_gate_env": rz("ValueError", "boom")})
case("_discord_max_attachment_bytes", state={"config": {"extra": {"max_attachment_bytes": "abc"}}},
     io={"logger.warning": rz("RuntimeError", "log")})
case("_discord_max_attachment_bytes", state={"config": {"extra": {}}}, fc=True)

# 7. _is_discord_voice_message_attachment
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": True})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": 0})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": rz("ValueError", "boom"), "logger.debug": None})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": rz("SystemExit", "bye")})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": rz("OSError", "x", 5), "logger.debug": None})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": True}})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": 0}})
case("_is_discord_voice_message_attachment", {"att": {"duration": 1.5, "waveform": "abc"}})
case("_is_discord_voice_message_attachment", {"att": {"duration": 1.5}})
case("_is_discord_voice_message_attachment", {"att": {"duration": None, "waveform": "x"}})
case("_is_discord_voice_message_attachment", {"att": {}})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}},
     io={"is_voice_message": rz("ValueError", "boom"), "logger.debug": rz("RuntimeError", "log")})
case("_is_discord_voice_message_attachment", {"att": {"is_voice_message": "seam"}}, fc=True)

# 8. _snapshot_gate_env
case("_snapshot_gate_env", io={"_scoped_gate_env": "v"})
case("_snapshot_gate_env", io={"_scoped_gate_env": ["v%d" % i for i in range(11)]})
case("_snapshot_gate_env", io={"_scoped_gate_env": None})
case("_snapshot_gate_env", state={"_gate_env_snapshot": {"old": 1}},
     io={"_scoped_gate_env": "new"})
case("_snapshot_gate_env", io={"_scoped_gate_env": rz("ValueError", "boom")})
case("_snapshot_gate_env", io={"_scoped_gate_env": rz("SystemExit", "bye")})
case("_snapshot_gate_env", fc=True)

# 9. _gate_env
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": {"X": "v"}})
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": {"X": ""}})
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": {"X": None}})
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": {"X": 0}})
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": {}},
     io={"_scoped_gate_env": "v"})
case("_gate_env", {"name": "X"}, io={"_scoped_gate_env": "v"})
case("_gate_env", {"name": "X", "default": "d"}, state={"_gate_env_snapshot": None},
     io={"_scoped_gate_env": "v"})
case("_gate_env", {"name": "X", "default": "d"}, io={"_scoped_gate_env": rz("ValueError", "boom")})
case("_gate_env", {"name": "X", "default": "d"}, fc=True)

# 10. _gate_raw
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, io={"_gate_env": "v"})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, state={"config": {"extra": {"k": "val"}}},
     io={"_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, state={"config": {"extra": {}}},
     io={"_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E", "config": {"extra": {"k": "pv"}}},
     io={"_scoped_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E", "config": {"extra": {"k": "pv"}}},
     io={"_scoped_gate_env": "env"})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, io={"_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E", "config": {}}, io={"_scoped_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, state={"config": {"extra": 5}},
     io={"_gate_env": ""})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, io={"_gate_env": rz("ValueError", "boom")})
case("_gate_raw", {"extra_key": "k", "env_key": "E", "config": {"extra": {}}},
     io={"_scoped_gate_env": rz("RuntimeError", "x")})
case("_gate_raw", {"extra_key": "k", "env_key": "E"}, fc=True)

# 11. _gate_csv_set
case("_gate_csv_set", {"raw": None})
case("_gate_csv_set", {"raw": "a,b"}, io={"_decode_json_list_literal": "a,b"})
case("_gate_csv_set", {"raw": "a, b ,,a"}, io={"_decode_json_list_literal": "a, b ,,a"})
case("_gate_csv_set", {"raw": '["x","y"]'}, io={"_decode_json_list_literal": ["x", "y"]})
case("_gate_csv_set", {"raw": [1, 2]}, io={"_decode_json_list_literal": [1, 2]})
case("_gate_csv_set", {"raw": [None, ""]}, io={"_decode_json_list_literal": [None, ""]})
case("_gate_csv_set", {"raw": " a , "}, io={"_decode_json_list_literal": " a , "})
case("_gate_csv_set", {"raw": 5}, io={"_decode_json_list_literal": 5})
case("_gate_csv_set", {"raw": ""}, io={"_decode_json_list_literal": ""})
case("_gate_csv_set", {"raw": ["  ", "b"]}, io={"_decode_json_list_literal": ["  ", "b"]})
case("_gate_csv_set", {"raw": "x"}, io={"_decode_json_list_literal": rz("ValueError", "boom")})
case("_gate_csv_set", {"raw": "x"}, fc=True)

# 12-14. channel gates
for fn in ("_get_allowed_channels", "_get_ignored_channels", "_get_no_thread_channels"):
    case(fn, io={"_gate_raw": "a,b", "_gate_csv_set": ["a", "b"]})
    case(fn, io={"_gate_raw": None, "_gate_csv_set": []})
    case(fn, io={"_gate_raw": rz("ValueError", "boom")})
    case(fn, io={"_gate_raw": "x", "_gate_csv_set": rz("RuntimeError", "x")})
    case(fn, io={"_gate_raw": rz("SystemExit", "bye")})
    case(fn, fc=True)

# 15. resolved_allowlist_user_ids
case("resolved_allowlist_user_ids", state={"_username_resolved_ids": ["b", "a", "b"]})
case("resolved_allowlist_user_ids", state={"_username_resolved_ids": []})
case("resolved_allowlist_user_ids", state={"_username_resolved_ids": ["x"]})
case("resolved_allowlist_user_ids", state={"_username_resolved_ids": ["10", "2"]})
case("resolved_allowlist_user_ids", state={"_username_resolved_ids": None})
case("resolved_allowlist_user_ids")

# 16. _discord_allow_all_users
case("_discord_allow_all_users", io={"_gate_raw": "true"})
case("_discord_allow_all_users", io={"_gate_raw": "YES"})
case("_discord_allow_all_users", io={"_gate_raw": "1"})
case("_discord_allow_all_users", io={"_gate_raw": "off"})
case("_discord_allow_all_users", io={"_gate_raw": None})
case("_discord_allow_all_users", io={"_gate_raw": 0})
case("_discord_allow_all_users", io={"_gate_raw": " true "})
case("_discord_allow_all_users", io={"_gate_raw": rz("ValueError", "boom")})
case("_discord_allow_all_users", fc=True)

# 17. _gateway_allow_all_users
case("_gateway_allow_all_users", io={"_gate_env": "true"})
case("_gateway_allow_all_users", io={"_gate_env": " yes "})
case("_gateway_allow_all_users", io={"_gate_env": "no"})
case("_gateway_allow_all_users", io={"_gate_env": ""})
case("_gateway_allow_all_users", io={"_gate_env": "1"})
case("_gateway_allow_all_users", io={"_gate_env": 5})
case("_gateway_allow_all_users", io={"_gate_env": rz("ValueError", "boom")})
case("_gateway_allow_all_users", fc=True)

# 18. _get_allow_bots
case("_get_allow_bots", io={"_gate_raw": "ALL"})
case("_get_allow_bots", io={"_gate_raw": " mentions "})
case("_get_allow_bots", io={"_gate_raw": None})
case("_get_allow_bots", io={"_gate_raw": ""})
case("_get_allow_bots", io={"_gate_raw": "none"})
case("_get_allow_bots", io={"_gate_raw": 0})
case("_get_allow_bots", io={"_gate_raw": "  "})
case("_get_allow_bots", io={"_gate_raw": rz("ValueError", "boom")})
case("_get_allow_bots", fc=True)

# 19. _bot_tag_debounce_key
case("_bot_tag_debounce_key", {"message": {"channel": {"id": "c"}, "author": {"id": "a"}}})
case("_bot_tag_debounce_key", {"message": {"channel": {"id": 5}, "author": {"id": 6}}})
case("_bot_tag_debounce_key", {"message": {}})
case("_bot_tag_debounce_key", {"message": {"channel": {"id": None}, "author": {}}})
case("_bot_tag_debounce_key", {"message": {"channel": {}, "author": {"id": "a"}}})
case("_bot_tag_debounce_key", {"message": {"channel": {"id": "c"}, "author": {"id": True}}})
case("_bot_tag_debounce_key", {"message": None})

# 20. _bot_tag_window_seconds
case("_bot_tag_window_seconds", state={"_text_batch_delay_seconds": 1.5,
                                       "_text_batch_split_delay_seconds": 2.0})
case("_bot_tag_window_seconds", state={"_text_batch_delay_seconds": 3,
                                       "_text_batch_split_delay_seconds": 1})
case("_bot_tag_window_seconds")
case("_bot_tag_window_seconds", state={"_text_batch_delay_seconds": 0,
                                       "_text_batch_split_delay_seconds": 0})
case("_bot_tag_window_seconds", state={"_text_batch_delay_seconds": 2.5,
                                       "_text_batch_split_delay_seconds": 2.5})
case("_bot_tag_window_seconds", state={"_text_batch_delay_seconds": -1,
                                       "_text_batch_split_delay_seconds": 0})

# 21. _record_bot_tag_debounce
MSG = {"channel": {"id": "c"}, "author": {"id": "a"}}
RIO = {"is_automated_message": True, "_self_is_explicitly_mentioned": True,
       "time.monotonic": 100.0, "_bot_tag_window_seconds": 2.0, "_bot_tag_debounce_key": "c:a"}
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 0})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": False})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_self_is_explicitly_mentioned": False})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1}, io=RIO)
case("_record_bot_tag_debounce", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"c:a": 50}}, io=RIO)
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": rz("ValueError", "boom")})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_self_is_explicitly_mentioned": rz("RuntimeError", "x")})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_self_is_explicitly_mentioned": True,
         "time.monotonic": rz("RuntimeError", "x")})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_self_is_explicitly_mentioned": True,
         "time.monotonic": 100, "_bot_tag_window_seconds": rz("RuntimeError", "x")})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_self_is_explicitly_mentioned": True,
         "time.monotonic": 100, "_bot_tag_window_seconds": 2,
         "_bot_tag_debounce_key": rz("RuntimeError", "x")})
case("_record_bot_tag_debounce", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": None}, io=RIO)
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": rz("SystemExit", "bye")})
case("_record_bot_tag_debounce", {"message": MSG}, state={"_text_batch_delay_seconds": 1}, fc=True)

# 22. _is_bot_tag_debounce_continuation
CIO = {"is_automated_message": True, "_bot_tag_debounce_key": "k", "time.monotonic": 100}
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 0})
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": False})
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io=CIO)
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"k": 200}},
     io={**CIO, "_bot_tag_window_seconds": 2})
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"k": 100}}, io=CIO)
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"k": 50}}, io=CIO)
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": rz("ValueError", "boom")})
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_bot_tag_debounce_key": rz("RuntimeError", "x")})
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": True, "_bot_tag_debounce_key": "k",
         "time.monotonic": rz("RuntimeError", "x")})
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"k": 200}},
     io={**CIO, "_bot_tag_window_seconds": rz("RuntimeError", "x")})
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": None}, io=CIO)
case("_is_bot_tag_debounce_continuation", {"message": MSG},
     state={"_text_batch_delay_seconds": 1, "_bot_tag_debounce_until": {"k": None}}, io=CIO)
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     io={"is_automated_message": rz("SystemExit", "bye")})
case("_is_bot_tag_debounce_continuation", {"message": MSG}, state={"_text_batch_delay_seconds": 1},
     fc=True)

# 23. _discord_free_response_channels
case("_discord_free_response_channels", state={"config": {"extra": {"free_response_channels": "a,b"}}},
     io={"_gate_csv_set": ["a", "b"]})
case("_discord_free_response_channels", state={"config": {"extra": {"free_response_channels": None}}},
     io={"_gate_env": "x", "_gate_csv_set": ["x"]})
case("_discord_free_response_channels", state={"config": {"extra": {}}},
     io={"_gate_env": "y", "_gate_csv_set": ["y"]})
case("_discord_free_response_channels", state={"config": {"extra": None}})
case("_discord_free_response_channels", state={"config": {"extra": {}}},
     io={"_gate_env": rz("ValueError", "boom")})
case("_discord_free_response_channels", state={"config": {"extra": {"free_response_channels": "z"}}},
     io={"_gate_csv_set": rz("RuntimeError", "x")})
case("_discord_free_response_channels", state={"config": {"extra": {"free_response_channels": ""}}},
     io={"_gate_csv_set": []})
case("_discord_free_response_channels", state={"config": {"extra": {}}}, fc=True)

# 24. _raw_mentioned_user_ids
case("_raw_mentioned_user_ids", {"message": {"content": "hi <@!12> and <@34> <@12>"}})
case("_raw_mentioned_user_ids", {"message": {"content": ""}})
case("_raw_mentioned_user_ids", {"message": {"content": "no mentions"}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@1><@2>"}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@!99>"}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@abc>"}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@12x>"}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@<@5>"}})
case("_raw_mentioned_user_ids", {"message": {"content": None}})
case("_raw_mentioned_user_ids", {"message": {"content": 5}})
case("_raw_mentioned_user_ids", {"message": {}})
case("_raw_mentioned_user_ids", {"message": {"content": "<@0>"}})

# 25. _self_is_explicitly_mentioned
CL = {"_client": {"user": {"id": "1"}}}
case("_self_is_explicitly_mentioned", {"message": {"mentions": [{"id": "1"}]}}, state=CL)
case("_self_is_explicitly_mentioned", {"message": {"mentions": [{"id": "2"}]}}, state=CL,
     io={"_raw_mentioned_user_ids": ["1"]})
case("_self_is_explicitly_mentioned", {"message": {"mentions": []}}, state=CL,
     io={"_raw_mentioned_user_ids": ["2"]})
case("_self_is_explicitly_mentioned", {"message": {}}, state={"_client": None})
case("_self_is_explicitly_mentioned", {"message": {}}, state={"_client": {"user": None}})
case("_self_is_explicitly_mentioned", {"message": {"mentions": [{"id": 1}]}},
     state={"_client": {"user": {"id": 1}}})
case("_self_is_explicitly_mentioned", {"message": {"mentions": []}}, state=CL,
     io={"_raw_mentioned_user_ids": rz("ValueError", "boom")})
case("_self_is_explicitly_mentioned", {"message": {"mentions": []}}, state=CL,
     io={"_raw_mentioned_user_ids": []})
case("_self_is_explicitly_mentioned", {"message": {"mentions": []}}, state=CL,
     io={"_raw_mentioned_user_ids": rz("SystemExit", "bye")})
case("_self_is_explicitly_mentioned", {"message": {"mentions": []}}, state=CL, fc=True)

# 26. _self_is_raw_mentioned
case("_self_is_raw_mentioned", {"message": {}}, state=CL, io={"_raw_mentioned_user_ids": ["1"]})
case("_self_is_raw_mentioned", {"message": {}}, state=CL, io={"_raw_mentioned_user_ids": ["2"]})
case("_self_is_raw_mentioned", {"message": {}}, state={"_client": None})
case("_self_is_raw_mentioned", {"message": {}}, state={"_client": {"user": None}})
case("_self_is_raw_mentioned", {"message": {"mentions": [{"id": "1"}]}}, state=CL,
     io={"_raw_mentioned_user_ids": []})
case("_self_is_raw_mentioned", {"message": {}}, state=CL,
     io={"_raw_mentioned_user_ids": rz("ValueError", "boom")})
case("_self_is_raw_mentioned", {"message": {}}, state=CL, fc=True)

# 27. _discord_bots_require_inline_mention
case("_discord_bots_require_inline_mention", state={"config": {"extra": {"bots_require_inline_mention": "false"}}})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {"bots_require_inline_mention": "YES"}}})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {"bots_require_inline_mention": "on"}}})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {"bots_require_inline_mention": "maybe"}}})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {"bots_require_inline_mention": True}}},
     io={FLAG: True})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {}}}, io={FLAG: False})
case("_discord_bots_require_inline_mention", state={"config": {"extra": None}})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {}}},
     io={FLAG: rz("ValueError", "boom")})
case("_discord_bots_require_inline_mention", state={"config": {"extra": {}}}, fc=True)

# 28. _discord_channel_keys
case("_discord_channel_keys", {"message": {"channel": {"id": "c"}}},
     io={"_discord_channel_keys_from_channel": ["c"]})
case("_discord_channel_keys", {"message": {}, "parent_channel_id": "p"},
     io={"_discord_channel_keys_from_channel": ["p"]})
case("_discord_channel_keys", {"message": {"channel": {"id": "c", "name": "n"}}},
     io={"_discord_channel_keys_from_channel": ["#n", "c", "n"]})
case("_discord_channel_keys", {"message": {}},
     io={"_discord_channel_keys_from_channel": rz("ValueError", "boom")})
case("_discord_channel_keys", {"message": {}},
     io={"_discord_channel_keys_from_channel": rz("SystemExit", "bye")})
case("_discord_channel_keys", {"message": {}}, fc=True)

# 29. _discord_channel_keys_from_channel
case("_discord_channel_keys_from_channel", {"channel": {"id": 5, "name": " general "}})
case("_discord_channel_keys_from_channel", {"channel": {"id": "c", "name": "n", "parent_id": "p"}})
case("_discord_channel_keys_from_channel", {"channel": {"id": "c", "name": "n", "parent": {"name": "pn"}}})
case("_discord_channel_keys_from_channel", {"channel": {}})
case("_discord_channel_keys_from_channel", {"channel": {"id": "c"}, "parent_channel_id": "p"})
case("_discord_channel_keys_from_channel", {"channel": {"id": "c", "parent_id": "p"}, "parent_channel_id": ""})
case("_discord_channel_keys_from_channel", {"channel": {"name": "n"}})
case("_discord_channel_keys_from_channel", {"channel": {"id": None, "name": "n"}})
case("_discord_channel_keys_from_channel", {"channel": {"id": "c", "name": None}})
case("_discord_channel_keys_from_channel", {"channel": {"parent": {"name": "pn"}}})
case("_discord_channel_keys_from_channel", {"channel": {"parent": 0}})
case("_discord_channel_keys_from_channel", {"channel": None, "parent_channel_id": "p"})

# 30. _discord_history_backfill_limit
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": "25"}}})
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": 10}}})
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": -3}}})
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": "abc"}}},
     io={"_scoped_gate_env": "30"})
case("_discord_history_backfill_limit", state={"config": {"extra": {}}}, io={"_scoped_gate_env": "40"})
case("_discord_history_backfill_limit", state={"config": {"extra": {}}}, io={"_scoped_gate_env": "bad"})
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": None}}},
     io={"_scoped_gate_env": "15"})
case("_discord_history_backfill_limit", state={"config": {"extra": None}})
case("_discord_history_backfill_limit", state={"config": {"extra": {}}},
     io={"_scoped_gate_env": rz("ValueError", "boom")})
case("_discord_history_backfill_limit", state={"config": {"extra": {"history_backfill_limit": []}}},
     io={"_scoped_gate_env": "20"})
case("_discord_history_backfill_limit", state={"config": {"extra": {}}}, fc=True)

# 31. _get_discord_command_sync_policy
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "safe"})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "BULK"})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "off"})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "SAFE"})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "bad", "logger.warning": None})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": ""})
case("_get_discord_command_sync_policy", state={"name": "Bot"},
     io={"_scoped_gate_env": "bad", "logger.warning": None})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": rz("ValueError", "boom")})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": "bad",
                                              "logger.warning": rz("RuntimeError", "log")})
case("_get_discord_command_sync_policy", io={"_scoped_gate_env": 5})
case("_get_discord_command_sync_policy", fc=True)

# 32. _normalize_permissions
case("_normalize_permissions", {"value": None})
case("_normalize_permissions", {"value": 8})
case("_normalize_permissions", {"value": "8"})
case("_normalize_permissions", {"value": True})
case("_normalize_permissions", {"value": 0})
case("_normalize_permissions", {"value": ""})
case("_normalize_permissions", {"value": False})

# 33. _canonicalize_app_command_option
case("_canonicalize_app_command_option", {"payload": {}})
case("_canonicalize_app_command_option", {"payload": {"type": "3", "name": "n", "description": "d", "required": True}})
case("_canonicalize_app_command_option", {"payload": {"choices": [{"name": "a", "value": 1}, {"name": "b", "value": None}, 5]}})
case("_canonicalize_app_command_option", {"payload": {"channel_types": [0, 1]}})
case("_canonicalize_app_command_option", {"payload": {"channel_types": "ab"}})
case("_canonicalize_app_command_option", {"payload": {"channel_types": 5}})
case("_canonicalize_app_command_option", {"payload": {"options": [{"name": "sub"}]}})
case("_canonicalize_app_command_option", {"payload": {"type": "bad"}})
case("_canonicalize_app_command_option", {"payload": {"min_value": 1, "max_value": 10, "min_length": 0, "max_length": 100}})
case("_canonicalize_app_command_option", {"payload": {"choices": "ab"}})
case("_canonicalize_app_command_option", {"payload": {"options": 5}})
case("_canonicalize_app_command_option", {"payload": {"name": 0}})
case("_canonicalize_app_command_option", {"payload": {"autocomplete": 1}})

# 34. _canonicalize_app_command_payload
case("_canonicalize_app_command_payload", {"payload": {}}, io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"name": "cmd", "type": "2"}},
     io={"_normalize_permissions": "8"})
case("_canonicalize_app_command_payload", {"payload": {"contexts": [2, 1]}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"contexts": []}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"integration_types": [1, 0]}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"options": [{"name": "o"}]}},
     io={"_normalize_permissions": None, "_canonicalize_app_command_option": {"canonical": 1}})
case("_canonicalize_app_command_payload", {"payload": {"contexts": ["bad"]}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"contexts": "12"}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {}},
     io={"_normalize_permissions": rz("ValueError", "boom")})
case("_canonicalize_app_command_payload", {"payload": {"options": [{"name": "o"}]}},
     io={"_normalize_permissions": None, "_canonicalize_app_command_option": rz("RuntimeError", "x")})
case("_canonicalize_app_command_payload", {"payload": {"dm_permission": None, "nsfw": 1}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"type": 0}}, io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"options": "ab"}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {"contexts": 5}},
     io={"_normalize_permissions": None})
case("_canonicalize_app_command_payload", {"payload": {}}, fc=True)

# 35. _existing_command_to_payload
case("_existing_command_to_payload", {"command": {}}, io={"to_dict": {"name": "n"}})
case("_existing_command_to_payload", {"command": {"nsfw": True}}, io={"to_dict": {"name": "n"}})
case("_existing_command_to_payload", {"command": {"guild_only": True}}, io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {"guild_only": False}}, io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {"default_member_permissions": {"value": 8}}},
     io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {"default_member_permissions": 8}}, io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {"default_member_permissions": {"novalue": 1}}},
     io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {"nsfw": None, "guild_only": None}},
     io={"to_dict": {"x": 1}})
case("_existing_command_to_payload", {"command": {"nsfw": 1, "guild_only": 0,
                                                   "default_member_permissions": {"value": "0"}}},
     io={"to_dict": {}})
case("_existing_command_to_payload", {"command": {}}, io={"to_dict": rz("ValueError", "boom")})
case("_existing_command_to_payload", {"command": {}}, io={"to_dict": 5})
case("_existing_command_to_payload", {"command": {}}, fc=True)

# 36. _patchable_app_command_payload
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": {"name": "n", "description": "d", "options": []}})
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": {"name": "n"}})
case("_patchable_app_command_payload", {"payload": {}}, io={"_canonicalize_app_command_payload": {}})
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": {"name": "n", "description": "d"}})
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": {"name": "n", "description": "d", "options": [1], "extra": 9}})
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": rz("ValueError", "boom")})
case("_patchable_app_command_payload", {"payload": {}},
     io={"_canonicalize_app_command_payload": rz("SystemExit", "bye")})
case("_patchable_app_command_payload", {"payload": {}}, fc=True)


def main():
    # Set-returning seams are single-call LIST results, not per-call queues:
    # wrap bare arrays in {"__script__": true, "ret": [...]} so the one call
    # returns the whole list (a bare [] would read as "script exhausted").
    list_seams = {"_gate_csv_set", "_raw_mentioned_user_ids",
                  "_discord_channel_keys_from_channel"}
    for c in CASES:
        io = c["in"].get("io") or {}
        for k, v in list(io.items()):
            if k in list_seams and isinstance(v, list):
                io[k] = {"__script__": True, "ret": v}
    out = os.path.join(os.path.dirname(__file__), "fixtures", "discord_adapter_flow2.jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [k for k, v in counts.items() if v < 5]
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {out}")
    if thin:
        print(f"WARNING funcs with <5 cases: {thin}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()