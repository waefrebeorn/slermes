#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_settings_flow.jsonl -- the scripted-seam
FLOW cases for the tui_gateway/methods_config_set.py port
(tui_methods_settings_flow).

Deterministic; >=5 cases per ported func. Each case is
{"func", "in": {"args", "io", "state"}, "fail_closed"?}. io maps seam name ->
script (plain value reused, list consumed per call, or {"__script__": true,
"ret"/"raise"}). A missing script is the fail-closed RuntimeError probe.

Fixture domain (shared with the port header): ASCII strings; absolute POSIX
paths; ints only except event_wait timeout (30.0); session carrier in
state.session (raw dict, agent attribute double, agent_ready Event marker);
parsed model-switch doubles carry the directly-read attributes; list-valued
seam RESULTS wrapped one level deeper (bare array == per-call queue).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_settings_flow.jsonl")

CASES = []


def add(func, args=None, io=None, state=None, fail_closed=False):
    inp = {}
    if args is not None:
        inp["args"] = args
    if io is not None:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    case = {"func": func, "in": inp}
    if fail_closed:
        case["fail_closed"] = True
    CASES.append(case)


def rs(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def ok(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def err(rid, code, message):
    return {"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}}


R = "r1"
OKEnvelope = ok(R, {"key": "k", "value": "v"})

# -- _word --
for v, want in [("  FAST  ", "fast"), ("", ""), (None, ""), (0, ""), ("On", "on"),
                (5, "5"), (False, ""), ("  ", ""), ("MiXeD", "mixed")]:
    add("_word", {"value": v})

# -- _raw_word --
for v in [None, 0, False, [], "  X  ", "", "True", 7]:
    add("_raw_word", {"value": v})

# -- _kv --
add("_kv", {"rid": R, "key": "fast", "value": "fast"}, {"ok": OKEnvelope})
add("_kv", {"rid": R, "key": "fast", "value": "fast", "extra": {"scope": "global"}},
    {"ok": OKEnvelope})
add("_kv", {"rid": R, "key": "k", "value": None}, {"ok": OKEnvelope})
add("_kv", {"rid": R, "key": None, "value": 5}, {"ok": OKEnvelope})
add("_kv", {"rid": R, "key": "k", "value": "v"}, {"ok": rs("OSError", "disk")})
add("_kv", {"rid": R, "key": "k", "value": "v"}, {}, fail_closed=True)

# -- _cfgset_model_ok --
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m"}, {"ok": OKEnvelope})
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m",
                         "confirm_message": "expensive"}, {"ok": OKEnvelope})
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m", "warning": "legacy",
                         "confirm_message": "legacy"}, {"ok": OKEnvelope})
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m", "scope": "global",
                         "extra": {"deferred": True}}, {"ok": OKEnvelope})
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m"},
    {"ok": rs("ValueError", "x")})
add("_cfgset_model_ok", {"rid": R, "key": "model", "value": "m"}, {}, fail_closed=True)

# -- _emit_session_info --
add("_emit_session_info", {"sid": "s1"},
    {"session_info": {"model": "m"}, "emit": None},
    {"session": {"agent": {"model": "m"}}})
add("_emit_session_info", {"sid": "s1"}, {}, {"session": {"agent": None}})
add("_emit_session_info", {"sid": "s1"}, {}, {"session": {}})
add("_emit_session_info", {"sid": "s1"},
    {"session_info": rs("ValueError", "info")}, {"session": {"agent": {"model": "m"}}})
add("_emit_session_info", {"sid": "s1"},
    {"session_info": {"model": "m"}, "emit": rs("OSError", "emit")},
    {"session": {"agent": {"model": "m"}}})
add("_emit_session_info", {"sid": "s1"}, {}, {"session": {"agent": {"model": "m"}}},
    fail_closed=True)

# -- _emit_all_session_info --
add("_emit_all_session_info", {}, {"sessions_items": [[]]})
add("_emit_all_session_info", {},
    {"sessions_items": [[["s1", {"agent": {"model": "a"}}], ["s2", {"agent": {"model": "b"}}]]],
     "session_info": [{"model": "a"}, {"model": "b"}], "emit": [None, None]})
add("_emit_all_session_info", {},
    {"sessions_items": [[["s1", {"agent": None}], ["s2", {"agent": {"model": "b"}}]]],
     "session_info": {"model": "b"}, "emit": None})
add("_emit_all_session_info", {},
    {"sessions_items": [[["s1", {"agent": {"model": "a"}}]]],
     "session_info": rs("ValueError", "x")})
add("_emit_all_session_info", {}, {}, fail_closed=True)

# -- _cfgset_await_agent --
add("_cfgset_await_agent", {"rid": R},
    {"wait_agent": err(R, 5032, "boom")}, {"session": {"agent": None}})
add("_cfgset_await_agent", {"rid": R},
    {"wait_agent": None, "err": err(R, 5032, "agent initialization failed")},
    {"session": {"agent": None}})
add("_cfgset_await_agent", {"rid": R}, {"wait_agent": None},
    {"session": {"agent": {"model": "m"}}})
add("_cfgset_await_agent", {"rid": R}, {"wait_agent": rs("OSError", "wait")},
    {"session": {"agent": None}})
add("_cfgset_await_agent", {"rid": R}, {}, {"session": {"agent": None}}, fail_closed=True)

# -- _refuse_managed_keys --
add("_refuse_managed_keys", {"key_paths": ["model.default"]}, {"is_key_managed": False})
add("_refuse_managed_keys", {"key_paths": ["model.default", "model.provider"]},
    {"is_key_managed": [False, True], "managed_source": "admin.yaml"})
add("_refuse_managed_keys", {"key_paths": ["display.skin"]},
    {"is_key_managed": True, "managed_source": "admin.yaml"})
add("_refuse_managed_keys", {"key_paths": ["a"]}, {"is_key_managed": rs("OSError", "x")})
add("_refuse_managed_keys", {"key_paths": ["a"]},
    {"is_key_managed": True, "managed_source": rs("ValueError", "src")})
add("_refuse_managed_keys", {"key_paths": []}, {})
add("_refuse_managed_keys", {"key_paths": ["a"]}, {}, fail_closed=True)

# -- _refuse_managed_model_pick --
add("_refuse_managed_model_pick", {}, {"managed_config_keys": None})
add("_refuse_managed_model_pick", {},
    {"managed_config_keys": ["model.default"], "parse_reasoning_effort": None,
     "is_key_managed": False})
add("_refuse_managed_model_pick", {"reasoning_effort": "high"},
    {"managed_config_keys": ["x"], "parse_reasoning_effort": "high",
     "is_key_managed": False})
add("_refuse_managed_model_pick", {"result": {"model": "m"}},
    {"managed_config_keys": ["x"], "read_user_config_raw": {"model": {"default": "old"}},
     "model_selection_config_updates": [["default", "provider"]],
     "parse_reasoning_effort": None, "is_key_managed": False})
add("_refuse_managed_model_pick", {},
    {"managed_config_keys": ["x"], "parse_reasoning_effort": None,
     "is_key_managed": True, "managed_source": "admin.yaml"})
add("_refuse_managed_model_pick", {}, {"managed_config_keys": rs("OSError", "x")})
add("_refuse_managed_model_pick", {}, {}, fail_closed=True)

# -- _write_display_sections --
add("_write_display_sections", {"display_fields": {"skin": "dark"}},
    {"is_key_managed": False, "supports_file_tooling": False, "write_config_changes": None})
add("_write_display_sections", {"display_fields": {"show_reasoning": True},
                                "sections": {"thinking": "expanded"}},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"display": {"sections": {"tools": "full"}}}, "save_cfg": None})
add("_write_display_sections", {"sections": {"thinking": "hidden"},
                                "drop_sections": ["tools"]},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"display": {"sections": {"tools": "full", "thinking": "x"}}},
     "save_cfg": None})
add("_write_display_sections", {"display_fields": {"skin": "dark"}},
    {"is_key_managed": True, "managed_source": "admin.yaml"})
add("_write_display_sections", {"display_fields": {"skin": "dark"}},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {}, "save_cfg": rs("ConfigWriteError", "locked")})
add("_write_display_sections", {"display_fields": {"skin": "dark"}},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": rs("OSError", "x")})
add("_write_display_sections", {"display_fields": {"skin": "dark"}}, {}, fail_closed=True)

# -- _stash_pending_model_switch --
PARSED = {"explicit_provider": "", "is_global": False, "is_session": False,
          "is_once": False, "model_input": "m"}
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False, "parsed": PARSED},
    {"resolve_persist_behavior": False, "pending_switch_selection_warning": None,
     "ok": OKEnvelope}, {"session": {"agent": {"provider": "nous"}}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False,
     "parsed": dict(PARSED, is_reset=True)},
    {"config_model_target": [["default-m", "nous"]], "ok": OKEnvelope},
    {"session": {"agent": None}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False,
     "parsed": dict(PARSED, is_global=True)},
    {"resolve_persist_behavior": True, "managed_config_keys": None,
     "pending_switch_selection_warning": None, "ok": OKEnvelope},
    {"session": {"agent": None}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False,
     "parsed": dict(PARSED, model_input_raise={"type": "ValueError", "message": "bad"})},
    {"resolve_persist_behavior": False, "pending_switch_selection_warning": None,
     "ok": OKEnvelope}, {"session": {"agent": None}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False,
     "parsed": dict(PARSED, model_input_raise={"type": "SystemExit", "message": "bye"})},
    {"resolve_persist_behavior": False}, {"session": {"agent": None}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False, "parsed": PARSED},
    {"resolve_persist_behavior": False, "pending_switch_selection_warning": "pricey",
     "ok": OKEnvelope}, {"session": {"agent": {"provider": "nous"}}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": True, "parsed": PARSED},
    {"resolve_persist_behavior": False, "ok": OKEnvelope}, {"session": {"agent": None}})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False, "parsed": PARSED},
    {}, {"session": {"agent": None}}, fail_closed=True)

# -- _cfgset_guarded --
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": ok(R, {"x": 1})}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": rs("ValueError", "boom"), "err": err(R, 5001, "boom")}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": rs("ConfigWriteError", "locked")}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": rs("ManagedKeyError", "managed")}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": rs("SessionNotOwned", "not owned")}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {"guarded_fn": rs("SystemExit", "bye")}, {"session": {}})
add("_cfgset_guarded", {"rid": R, "params": {}, "key": "k", "value": "v"},
    {}, {"session": {}}, fail_closed=True)

# -- _set_word --
add("_set_word", {"rid": R, "params": {}, "key": "busy", "value": "queue"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_word", {"rid": R, "params": {}, "key": "busy", "value": "bogus"},
    {"err": err(R, 4002, "unknown busy mode: bogus")})
add("_set_word", {"rid": R, "params": {}, "key": "approvals.mode", "value": "manual"},
    {"write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_set_word", {"rid": R, "params": {}, "key": "indicator", "value": "bogus"},
    {"err": err(R, 4002, "unknown indicator: 'bogus'; pick one of ascii|emoji|kaomoji|unicode")})
add("_set_word", {"rid": R, "params": {}, "key": "theme", "value": "dark"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_word", {"rid": R, "params": {}, "key": "thinking_mode", "value": "full"},
    {"write_config_keys": None, "ok": OKEnvelope})
add("_set_word", {"rid": R, "params": {}, "key": "details_mode", "value": "expanded"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope})
add("_set_word", {"rid": R, "params": {}, "key": "nope", "value": "x"}, {})
add("_set_word", {"rid": R, "params": {}, "key": "busy", "value": "queue"},
    {"write_config_key": rs("ConfigWriteError", "locked")})
add("_set_word", {"rid": R, "params": {}, "key": "busy", "value": "queue"},
    {}, fail_closed=True)

# -- _set_toggle --
add("_set_toggle", {"rid": R, "params": {}, "key": "density", "value": "toggle"},
    {"display_cfg": {"tui_compact": False}, "write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "density", "value": "on"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "battery", "value": "yes"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "timestamps", "value": ""},
    {"display_cfg": {"timestamps": True}, "write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "statusbar", "value": "toggle"},
    {"display_cfg": {"tui_statusbar": "off"}, "coerce_statusbar": "off",
     "write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "mouse", "value": "toggle"},
    {"display_cfg": {"mouse_tracking": "off"}, "display_mouse_tracking": "off",
     "write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "mouse", "value": "wheel"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_toggle", {"rid": R, "params": {}, "key": "density", "value": "bogus"},
    {"err": err(R, 4002, "unknown density value: bogus")})
add("_set_toggle", {"rid": R, "params": {}, "key": "nope", "value": "on"}, {})
add("_set_toggle", {"rid": R, "params": {}, "key": "density", "value": "toggle"},
    {}, fail_closed=True)

# -- _set_busy --
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": "status"},
    {"load_busy_input_mode": "queue", "ok": OKEnvelope})
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": ""},
    {"load_busy_input_mode": "steer", "ok": OKEnvelope})
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": "queue"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": "bogus"},
    {"err": err(R, 4002, "unknown busy mode: bogus")})
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": "status"},
    {"load_busy_input_mode": rs("OSError", "x")})
add("_set_busy", {"rid": R, "params": {}, "key": "busy", "value": "status"},
    {}, fail_closed=True)

# -- _set_approval_mode --
add("_set_approval_mode", {"rid": R, "params": {}, "key": "approval_mode", "value": "manual"},
    {"write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_set_approval_mode", {"rid": R, "params": {}, "key": "approval_mode", "value": "smart"},
    {"write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_set_approval_mode", {"rid": R, "params": {}, "key": "approval_mode", "value": "bogus"},
    {"err": err(R, 4002, "unknown approval mode: bogus; pick one of manual|smart|off")})
add("_set_approval_mode", {"rid": R, "params": {}, "key": "approval_mode", "value": "off"},
    {"write_config_key": rs("ConfigWriteError", "locked")})
add("_set_approval_mode", {"rid": R, "params": {}, "key": "approval_mode", "value": "manual"},
    {}, fail_closed=True)

# -- _set_details_section --
add("_set_details_section", {"rid": R, "key": "details_mode.thinking", "value": "expanded"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope})
add("_set_details_section", {"rid": R, "key": "details_mode.tools", "value": ""},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"display": {"sections": {"tools": "full"}}}, "save_cfg": None,
     "ok": OKEnvelope})
add("_set_details_section", {"rid": R, "key": "details_mode.bogus", "value": "full"},
    {"err": err(R, 4002, "unknown section: bogus")})
add("_set_details_section", {"rid": R, "key": "details_mode", "value": "full"}, {})
add("_set_details_section", {"rid": R, "key": "details_mode.thinking", "value": "bogus"},
    {"err": err(R, 4002, "unknown details_mode: bogus")})
add("_set_details_section", {"rid": R, "key": "details_mode.thinking", "value": "full"},
    {}, fail_closed=True)

# -- _set_verbose --
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": "all"},
    {"write_config_key": None, "ok": OKEnvelope}, {"session": {}})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": "cycle"},
    {"load_tool_progress_mode": "off", "write_config_key": None, "ok": OKEnvelope},
    {"session": {"tool_progress_mode": "all"}})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": ""},
    {"load_tool_progress_mode": "new", "write_config_key": None, "ok": OKEnvelope})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": "bogus"},
    {"err": err(R, 4002, "unknown verbose mode: bogus")}, {"session": {}})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": "verbose"},
    {"write_config_key": None, "ok": OKEnvelope},
    {"session": {"agent": {"verbose_logging": False}}})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": "cycle"},
    {"load_tool_progress_mode": rs("OSError", "x")}, {"session": {}})
add("_set_verbose", {"rid": R, "params": {}, "key": "verbose", "value": ""},
    {}, fail_closed=True)

# -- _set_focus --
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "on"},
    {"display_cfg": {"focus_view": False}, "resolve_focus_arg": [["set", True]],
     "load_tool_progress_mode": "all", "normalize_tool_progress_mode": "all",
     "write_config_keys": None, "ok": OKEnvelope}, {"session": {}})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "off"},
    {"display_cfg": {"focus_view": True, "focus_saved_tool_progress": "new"},
     "resolve_focus_arg": [["set", False]], "normalize_tool_progress_mode": "new",
     "write_config_keys": None, "ok": OKEnvelope}, {"session": {}})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "status"},
    {"display_cfg": {"focus_view": True}, "resolve_focus_arg": [["status", None]],
     "load_tool_progress_mode": "all", "ok": OKEnvelope}, {"session": {}})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "bogus"},
    {"display_cfg": {}, "resolve_focus_arg": [["usage", None]],
     "err": err(R, 4002, "unknown focus value: bogus (use on|off|status)")})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "on"},
    {"display_cfg": {}, "resolve_focus_arg": rs("ValueError", "x")})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "on"},
    {"display_cfg": {"focus_view": True, "focus_saved_tool_progress": "new"},
     "resolve_focus_arg": [["set", True]], "normalize_tool_progress_mode": "new",
     "write_config_keys": None, "ok": OKEnvelope},
    {"session": {"agent": {"tool_progress_mode": "all"}}})
add("_set_focus", {"rid": R, "params": {}, "key": "focus", "value": "on"},
    {}, fail_closed=True)

# -- _set_display_toggle --
add("_set_display_toggle", {"rid": R, "key": "display.message_reactions", "value": "true"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_display_toggle", {"rid": R, "key": "display.in_app_tips", "value": "off"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_display_toggle", {"rid": R, "key": "display.in_app_tours", "value": "bogus"},
    {"err": err(R, 4002, "display.in_app_tours takes true or false")})
add("_set_display_toggle", {"rid": R, "key": "display.message_reactions", "value": 1},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_display_toggle", {"rid": R, "key": "display.message_reactions", "value": "yes"},
    {"write_config_key": rs("ConfigWriteError", "locked")})
add("_set_display_toggle", {"rid": R, "key": "display.message_reactions", "value": "true"},
    {}, fail_closed=True)

# -- _set_daybreak --
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "on"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "false"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "bogus"},
    {"err": err(R, 4002, "daybreak takes true or false")})
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "1"},
    {"write_config_key": None, "ok": OKEnvelope})
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "on"},
    {"write_config_key": rs("OSError", "x")})
add("_set_daybreak", {"rid": R, "key": "daybreak", "value": "on"}, {}, fail_closed=True)

# -- _set_skin --
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "dark"},
    {"write_config_key": None, "resolve_skin": {"skin": "dark"},
     "broadcast_global_event": None, "note_skin_broadcast": None, "ok": OKEnvelope})
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "light"},
    {"write_config_key": None, "resolve_skin": {"skin": "light"},
     "broadcast_global_event": None, "note_skin_broadcast": None, "ok": OKEnvelope})
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "dark"},
    {"write_config_key": rs("ConfigWriteError", "locked")})
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "dark"},
    {"write_config_key": None, "resolve_skin": rs("ValueError", "skin"),
     "err": err(R, 5001, "skin")})
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "dark"},
    {"write_config_key": None, "resolve_skin": {"skin": "dark"},
     "broadcast_global_event": rs("SystemExit", "bye")})
add("_set_skin", {"rid": R, "params": {}, "key": "skin", "value": "dark"},
    {}, fail_closed=True)

# -- _set_prompt --
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "clear"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "hello"},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"custom_prompt": "old"}, "save_cfg": None, "ok": OKEnvelope})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "clear"},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"custom_prompt": "old"}, "save_cfg": None, "ok": OKEnvelope})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "x"},
    {"is_key_managed": True, "managed_source": "admin.yaml"})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "x"},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {}, "save_cfg": rs("ConfigWriteError", "locked")})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "x"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": rs("ValueError", "x"), "err": err(R, 5001, "x")})
add("_set_prompt", {"rid": R, "params": {}, "key": "prompt", "value": "x"},
    {}, fail_closed=True)

# -- _set_personality --
add("_set_personality", {"rid": R, "params": {"session_id": "s1"}, "key": "personality",
                         "value": "friendly"},
    {"is_key_managed": False, "load_cfg_raw": {},
     "validate_personality": [["friendly", "prompt"]], "persist_personality": None,
     "apply_personality_to_session": [[True, {"name": "friendly"}]], "ok": OKEnvelope},
    {"session": {}})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "none"},
    {"is_key_managed": False, "load_cfg_raw": {},
     "validate_personality": [["none", ""]], "persist_personality": None,
     "apply_personality_to_session": [[False, None]], "ok": OKEnvelope}, {"session": {}})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "x"},
    {"is_key_managed": False, "load_cfg_raw": {},
     "validate_personality": [["a", "b", "c"]],
     "err": err(R, 5001, "too many values to unpack (expected 2, got 3)")})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "x"},
    {"is_key_managed": False, "load_cfg_raw": {},
     "validate_personality": [["x", "p"]], "persist_personality": rs("ConfigWriteError", "locked")})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "x"},
    {"is_key_managed": True, "managed_source": "admin.yaml"})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "x"},
    {"is_key_managed": False, "load_cfg_raw": {},
     "validate_personality": [["x", "p"]], "persist_personality": None,
     "apply_personality_to_session": rs("ValueError", "apply"), "err": err(R, 5001, "apply")},
    {"session": {}})
add("_set_personality", {"rid": R, "params": {}, "key": "personality", "value": "x"},
    {}, fail_closed=True)

# -- _set_cwd --
CWD_IO = {"expanduser": "/h/proj", "abspath": "/h/proj", "isdir": True,
          "write_config_key": None, "get_hermes_home": "/h/launch",
          "path_resolve": ["/h/launch", "/h/launch"], "environ_set": None,
          "git_branch": "main", "ok": OKEnvelope}
add("_set_cwd", {"rid": R, "key": "cwd", "value": "~/proj", "hermes_home": "/h/launch"}, CWD_IO)
add("_set_cwd", {"rid": R, "key": "cwd", "value": "/h/proj", "hermes_home": "/h/launch"},
    dict(CWD_IO, path_resolve=["/h/launch", "/h/other"]))
add("_set_cwd", {"rid": R, "key": "cwd", "value": "/h/proj"},
    {"expanduser": "/h/proj", "abspath": "/h/proj", "isdir": False,
     "err": err(R, 4002, "working directory does not exist: /h/proj")})
add("_set_cwd", {"rid": R, "key": "cwd", "value": "  "},
    {"err": err(R, 4002, "cwd required")})
add("_set_cwd", {"rid": R, "key": "cwd", "value": "/h/proj"},
    {"expanduser": rs("OSError", "x")})
add("_set_cwd", {"rid": R, "key": "cwd", "value": "/h/proj"},
    dict(CWD_IO, git_branch=rs("ValueError", "git")))
add("_set_cwd", {"rid": R, "key": "cwd", "value": "/h/proj"}, {}, fail_closed=True)

# -- _set_yolo --
add("_set_yolo", {"rid": R, "params": {"scope": "global"}, "key": "yolo", "value": "on"},
    {"load_cfg": {"approvals": {"mode": "manual"}}, "normalize_approval_mode": "manual",
     "write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "on"},
    {"toggle_session_yolo": True, "session_info": {"model": "m"}, "emit": None,
     "ok": OKEnvelope}, {"session": {"session_key": "sk", "agent": {"session_id": "sid"}}})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "on"},
    {"environ_get": None, "is_truthy_value": False, "environ_set": None, "ok": OKEnvelope})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "off"},
    {"environ_get": "1", "is_truthy_value": True, "environ_pop": None, "ok": OKEnvelope})
add("_set_yolo", {"rid": R, "params": {"scope": "global"}, "key": "yolo", "value": "toggle"},
    {"load_cfg": {"approvals": {"mode": "off"}}, "normalize_approval_mode": "off",
     "write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "on"},
    {"toggle_session_yolo": rs("ConfigWriteError", "locked")},
    {"session": {"session_key": "sk", "agent": None}})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "on"},
    {"environ_get": rs("SystemExit", "bye")})
add("_set_yolo", {"rid": R, "params": {}, "key": "yolo", "value": "on"},
    {}, fail_closed=True)

# -- _set_yolo_persist --
add("_set_yolo_persist", {"row_id": "sid", "on": True},
    {"session_db": {"db": 1}, "persistence_for": {"p": 1}, "set_yolo": None},
    {"session": {"session_key": "sk"}})
add("_set_yolo_persist", {"row_id": "sid", "on": False},
    {"session_db": {"db": 1}, "persistence_for": None}, {"session": {"session_key": "sk"}})
add("_set_yolo_persist", {"row_id": "sid", "on": True},
    {"session_db": rs("OSError", "db")}, {"session": {}})
add("_set_yolo_persist", {"row_id": "sid", "on": True},
    {"session_db": {"db": 1}, "persistence_for": {"p": 1}, "set_yolo": rs("ValueError", "x")},
    {"session": {}})
add("_set_yolo_persist", {"row_id": "sid", "on": True},
    {"session_db": {"db": 1}, "persistence_for": rs("ConfigWriteError", "locked")},
    {"session": {}})
add("_set_yolo_persist", {"row_id": "sid", "on": True}, {}, {"session": {}}, fail_closed=True)

# -- _set_reasoning --
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "show"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope}, {"session": {}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "hide"},
    {"is_key_managed": False, "supports_file_tooling": True,
     "load_cfg_raw": {"display": {}}, "save_cfg": None, "ok": OKEnvelope},
    {"session": {"show_reasoning": True}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "full"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope}, {"session": {}})
add("_set_reasoning", {"rid": R, "params": {"scope": "global"}, "key": "reasoning",
                       "value": "high"},
    {"parse_reasoning_effort": "high", "write_config_key": None, "ok": OKEnvelope},
    {"session": {"create_reasoning_override": "low"}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "high"},
    {"parse_reasoning_effort": "high", "persist_live_session_runtime": None,
     "session_info": {"model": "m"}, "emit": None, "ok": OKEnvelope},
    {"session": {"agent": {"reasoning_config": None}}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "bogus"},
    {"parse_reasoning_effort": None, "err": err(R, 4002, "unknown reasoning value: bogus")},
    {"session": {}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "clamp"},
    {"is_key_managed": False, "supports_file_tooling": False,
     "write_config_changes": None, "ok": OKEnvelope}, {"session": {}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "high"},
    {"parse_reasoning_effort": rs("SystemExit", "bye")}, {"session": {}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "show"},
    {}, {"session": {}}, fail_closed=True)

# -- _set_fast --
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "status"},
    {"load_service_tier": "priority", "service_tier_word": "fast", "ok": OKEnvelope})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "fast"},
    {"resolve_fast_mode_overrides": {"service_tier": "priority"},
     "persist_live_session_runtime": None, "session_info": {"model": "m"}, "emit": None,
     "ok": OKEnvelope}, {"session": {"agent": {"model": "m", "provider": "nous"}}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "normal"},
    {"load_service_tier": None, "write_config_key": None, "ok": OKEnvelope})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "toggle"},
    {"load_service_tier": "priority", "resolve_fast_mode_overrides": {"x": 1},
     "ok": OKEnvelope}, {"session": {}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "ultrafast"},
    {"resolve_fast_mode_overrides": {"x": 1}, "ultrafast_offered": True,
     "persist_live_session_runtime": None, "session_info": {"model": "m"}, "emit": None,
     "ok": OKEnvelope}, {"session": {"agent": {"model": "m", "api_key": "k"}}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "ultrafast"},
    {"resolve_fast_mode_overrides": {"x": 1}, "ultrafast_offered": False,
     "err": err(R, 4002, "ultrafast mode is not available for this model")},
    {"session": {"agent": {"model": "m"}}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "bogus"},
    {"load_service_tier": None, "err": err(R, 4002, "unknown fast mode: bogus")})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "fast"},
    {"load_service_tier": None, "resolve_model": None,
     "err": err(R, 4002, "fast mode is not available without a selected model")})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "status"},
    {"load_service_tier": rs("OSError", "x")})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "status"},
    {}, fail_closed=True)

# -- _set_model --
MPARSED = {"explicit_provider": "", "is_global": False, "is_session": False,
           "is_once": False, "model_input": "m"}
add("_set_model", {"rid": R, "params": {}, "key": "model", "value": ""},
    {"err": err(R, 4002, "model value required")})
add("_set_model", {"rid": R, "params": {}, "key": "model", "value": "m"},
    {"parse_model_switch_args": dict(MPARSED, is_once=True),
     "apply_model_switch": {"value": "m", "warning": ""}, "ok": OKEnvelope})
add("_set_model", {"rid": R, "params": {}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED,
     "err": err(R, 4001, "config.set model requires a live session; to change the "
                "profile default run /setup, or use the Models page (dashboard) / "
                "Settings -> Models (Desktop)")})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "session_profile_runtime_scope": {"scope": 1},
     "apply_model_switch": {"value": "m", "warning": "", "confirm_required": False},
     "ok": OKEnvelope}, {"session": {"agent": {"model": "old"}}})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "resolve_persist_behavior": False,
     "pending_switch_selection_warning": None, "ok": OKEnvelope},
    {"session": {"running": True, "agent": {"provider": "nous"}}})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "event_wait": False,
     "err": err(R, 5032, "Hermes is still starting this session (loading tools), so this "
                "command could not run yet. Wait for the status bar to show ready and try again.")},
    {"session": {"agent": None, "agent_error": "boom", "agent_ready": {"id": "e1"}}})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED,
     "err": err(R, 5032, "boom")},
    {"session": {"agent": None, "agent_error": "boom", "agent_ready": None}})
add("_set_model", {"rid": R, "params": {}, "key": "model", "value": "m"},
    {"parse_model_switch_args": rs("ValueError", "parse"), "err": err(R, 5001, "parse")})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "session_profile_runtime_scope": {"scope": 1},
     "apply_model_switch": rs("ConfigWriteError", "locked")},
    {"session": {"agent": {"model": "old"}}})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {}, {"session": {"agent": {"model": "old"}}}, fail_closed=True)

# -- _h_config_set --
add("_h_config_set", {"rid": R, "params": {"key": "nope", "value": "x"}},
    {"sessions_get": None, "err": err(R, 4002, "unknown config key: nope")})
add("_h_config_set", {"rid": R, "params": {"key": "model", "value": "m", "session_id": "stale"}},
    {"sessions_get": None, "sess_nowait": [["x", err(R, 4001, "stale")]]})
add("_h_config_set", {"rid": R, "params": {"key": "busy", "value": "queue", "session_id": "s1"}},
    {"sessions_get": {"state": True}, "write_config_key": None, "ok": OKEnvelope},
    {"session": {"session_key": "sk"}})
add("_h_config_set", {"rid": R, "params": {"key": "details_mode.thinking", "value": "expanded",
                                            "session_id": "s1"}},
    {"sessions_get": {"state": True}, "is_key_managed": False,
     "supports_file_tooling": False, "write_config_changes": None, "ok": OKEnvelope},
    {"session": {}})
add("_h_config_set", {"rid": R, "params": {"key": "display.message_reactions", "value": "true",
                                            "session_id": "s1"}},
    {"sessions_get": {"state": True}, "write_config_key": None, "ok": OKEnvelope},
    {"session": {}})
add("_h_config_set", {"rid": R, "params": {"key": "prompt", "value": "x", "session_id": "s1"}},
    {"sessions_get": {"state": True}, "is_key_managed": True, "managed_source": "admin.yaml",
     "err": err(R, 4030, "Cannot set 'custom_prompt': it is managed by your administrator "
                "(admin.yaml) and cannot be changed. Contact your administrator to modify it.")},
    {"session": {}})
add("_h_config_set", {"rid": R, "params": {"key": "skin", "value": "dark", "session_id": "s1"}},
    {"sessions_get": {"state": True}, "write_config_key": rs("ConfigWriteError", "locked"),
     "err": err(R, 4002, "locked")}, {"session": {}})
add("_h_config_set", {"rid": R, "params": {"key": "model", "value": "m", "session_id": "s1",
                                            "scope": "global"}},
    {"sessions_get": None, "parse_model_switch_args": MPARSED,
     "err": err(R, 4001, "config.set model requires a live session; to change the "
                "profile default run /setup, or use the Models page (dashboard) / "
                "Settings -> Models (Desktop)")})
add("_h_config_set", {"rid": R, "params": {"key": "busy", "value": "queue", "session_id": "s1"}},
    {"sessions_get": {"state": True}}, {"session": {}}, fail_closed=True)


# -- extra complex-branch probes --
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "event_wait": True, "event_is_set": True,
     "session_profile_runtime_scope": [{"scope": 1}, {"scope": 1}],
     "apply_model_switch": {"value": "m", "warning": "", "confirm_required": False},
     "restart_failed_agent_build": None, "wait_agent": err(R, 5032, "agent initialization failed")},
    {"session": {"agent": None, "agent_error": "boom", "agent_ready": {"id": "e1"}}})
add("_set_model", {"rid": R, "params": {"session_id": "s1"}, "key": "model", "value": "m"},
    {"parse_model_switch_args": MPARSED, "start_agent_build": None,
     "wait_agent": err(R, 5032, "agent initialization failed")},
    {"session": {"agent": None}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "fast"},
    {"resolve_fast_mode_overrides": {"service_tier": "priority", "speed": "fast"},
     "persist_live_session_runtime": None, "session_info": {"model": "m"}, "emit": None,
     "ok": OKEnvelope},
    {"session": {"agent": {"model": "m", "request_overrides": {"service_tier": "old",
                                                              "speed": "old", "keep": "z"}}}})
add("_set_fast", {"rid": R, "params": {}, "key": "fast", "value": "fast"},
    {"load_service_tier": None, "resolve_fast_mode_overrides": {"x": 1}, "ok": OKEnvelope},
    {"session": {"model_override": {"model": "picked"}}})
add("_set_yolo", {"rid": R, "params": {"scope": "global"}, "key": "yolo", "value": "off"},
    {"load_cfg": {"approvals": {"mode": "manual"}}, "normalize_approval_mode": "manual",
     "write_config_key": None, "sessions_items": [[]], "ok": OKEnvelope})
add("_stash_pending_model_switch",
    {"rid": R, "key": "model", "value": "m", "confirmed": False,
     "parsed": dict(PARSED, explicit_provider=" nous ", model_input="picked")},
    {"resolve_persist_behavior": False, "pending_switch_selection_warning": None,
     "ok": OKEnvelope}, {"session": {"agent": None}})
add("_set_reasoning", {"rid": R, "params": {}, "key": "reasoning", "value": "high"},
    {"parse_reasoning_effort": "high", "ok": OKEnvelope},
    {"session": {"create_reasoning_override": "low", "agent": None}})
add("_h_config_set", {"rid": R, "params": {"key": "yolo", "value": "on", "session_id": "s1"}},
    {"sessions_get": {"state": True}, "toggle_session_yolo": True,
     "session_info": {"model": "m"}, "emit": None, "ok": OKEnvelope},
    {"session": {"session_key": "sk", "agent": {"session_id": "sid"}}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"GEN tui_methods_settings_flow: {len(CASES)} cases, {len(counts)} funcs")
    thin = [f for f, n in counts.items() if n < 5]
    if thin:
        print(f"WARNING thin coverage: {thin}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()