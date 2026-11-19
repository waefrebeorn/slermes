#!/usr/bin/env python3
"""Generate tests/fixtures/tui_server_flow.jsonl for the tui_gateway/server.py
FLOW + leaf port (src/gateway/tui_server_flow.c).

Each case is {"func": <live name>, "in": {...}} where "in" carries:
  - "state": injected module-state mirror (cfg cache / home / sessions / reprobe / mcp)
  - "io":    scripted universal-seam results (bare array = per-call queue,
             {"__script__": true, "ret": v} = constant list/object, else constant;
             {"__raise__": kind, "message": m} = a raised exception)
  - the call arguments for the target function.

>= 5 cases per function. Pure-Python, no quarry import needed. Run:
  python tests/gen_tui_server_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_server_flow.jsonl")

# Exact live module constants (byte-for-byte from tui_gateway/server.py).
TOUR_UNAVAIL = json.dumps({
    "success": False,
    "error": ("No Hermes Desktop window answered the tour request. The tour is driven by the desktop app's "
              "renderer, which updates separately from this backend, so an app build older than the tour tool "
              "has nothing listening. Update the Hermes Desktop app and start a new session. Do not retry tour "
              "in this session.")})
PREVIEW_UNAVAIL = json.dumps({
    "success": False,
    "error": ("No Hermes Desktop window answered the preview action request. The drive_preview / "
              "annotate_preview bridge is served by the desktop app's renderer, which updates "
              "separately from this backend, so an app build older than the tool has nothing "
              "listening. Update the Hermes Desktop app, open a page with open_preview, and try "
              "again in this session after a short cooldown.")})
MARKER_PREFIX = "[System: The active model for this chat has changed to "
BARE_HINT = "bare `hermes` is interactive \u2014 use `/hermes chat -q \u2026` or run `hermes` in another terminal"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(cfg=None, home=None, sessions=None, reprobe=None, mcp=None):
    return {
        "cfg": cfg if cfg is not None else {"cache": None, "sig": None, "path": None},
        "home": home if home is not None else {"hermes_home": "/h/launch", "at_import": "/h/launch"},
        "sessions": sessions if sessions is not None else {},
        "reprobe": reprobe if reprobe is not None else {},
        "mcp": mcp if mcp is not None else {"loaded_rev": "rev0"},
    }


def raise_spec(kind, message=""):
    return {"__raise__": kind, "message": message}


def script_list(v):
    return {"__script__": True, "ret": v}


# ── _ok ──────────────────────────────────────────────────────────────────────
add("_ok", rid="r1", result={"a": 1})
add("_ok", rid=7, result={})
add("_ok", rid=None, result={"x": [1, 2]})
add("_ok", rid="r2", result={"nested": {"k": "v"}})
add("_ok", rid=0, result={"flag": True})
add("_ok", rid="r3", result={"empty": None})

# ── _normalize_request (seam: _err) ──────────────────────────────────────────
add("_normalize_request", req={"id": "a", "method": "m", "params": {"p": 1}},
    io={"_err": raise_spec("AssertionError", "should not be called")})
add("_normalize_request", req={"id": "a", "method": "m"},
    io={"_err": raise_spec("AssertionError", "no")})
add("_normalize_request", req={"id": "a", "method": "m", "params": None},
    io={"_err": raise_spec("AssertionError", "no")})
add("_normalize_request", req="not-a-dict",
    io={"_err": {"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "invalid request: expected an object"}}})
add("_normalize_request", req={"id": "a", "method": ""},
    io={"_err": {"jsonrpc": "2.0", "id": "a", "error": {"code": -32600, "message": "invalid request: method must be a non-empty string"}}})
add("_normalize_request", req={"id": "a", "method": 5},
    io={"_err": {"jsonrpc": "2.0", "id": "a", "error": {"code": -32600, "message": "invalid request: method must be a non-empty string"}}})
add("_normalize_request", req={"id": "a", "method": "m", "params": [1]},
    io={"_err": {"jsonrpc": "2.0", "id": "a", "error": {"code": -32602, "message": "invalid params: expected an object"}}})
add("_normalize_request", req={"method": "m"},
    io={"_err": raise_spec("AssertionError", "no")})

# ── _coerce_int_config_value ─────────────────────────────────────────────────
add("_coerce_int_config_value", value="12", default=15, min_value=1)
add("_coerce_int_config_value", value=3, default=15, min_value=5)
add("_coerce_int_config_value", value=None, default=15, min_value=1)
add("_coerce_int_config_value", value="abc", default=15, min_value=1)
add("_coerce_int_config_value", value=12.9, default=15, min_value=1)
add("_coerce_int_config_value", value=-12.9, default=0, min_value=-100)
add("_coerce_int_config_value", value=True, default=15, min_value=1)
add("_coerce_int_config_value", value="1_0", default=15, min_value=1)
add("_coerce_int_config_value", value="_1", default=15, min_value=1)
add("_coerce_int_config_value", value=[1], default=15, min_value=1)
add("_coerce_int_config_value", value=" 7 ", default=15, min_value=0)
add("_coerce_int_config_value", value=0, default=15, min_value=0)

# ── _display_cfg (seam: _load_cfg) ───────────────────────────────────────────
add("_display_cfg", io={"_load_cfg": {"display": {"x": 1}}})
add("_display_cfg", io={"_load_cfg": {}})
add("_display_cfg", io={"_load_cfg": {"display": "not-dict"}})
add("_display_cfg", io={"_load_cfg": {"display": None}})
add("_display_cfg", io={"_load_cfg": {"display": {}}})
add("_display_cfg", io={"_load_cfg": None})  # .get on None -> AttributeError propagates

# ── _load_interim_assistant_messages (seams: _display_cfg, is_truthy_value) ──
add("_load_interim_assistant_messages", io={"_display_cfg": {"interim_assistant_messages": True}, "is_truthy_value": True})
add("_load_interim_assistant_messages", io={"_display_cfg": {}, "is_truthy_value": True})
add("_load_interim_assistant_messages", io={"_display_cfg": {"interim_assistant_messages": "off"}, "is_truthy_value": False})
add("_load_interim_assistant_messages", io={"_display_cfg": {"interim_assistant_messages": None}, "is_truthy_value": False})
add("_load_interim_assistant_messages", io={"_display_cfg": {"interim_assistant_messages": False}, "is_truthy_value": False})
add("_load_interim_assistant_messages", io={"_display_cfg": None, "is_truthy_value": True})  # AttributeError

# ── _load_dashboard_process_isolation_config ─────────────────────────────────
add("_load_dashboard_process_isolation_config", cfg={"dashboard": {"turn_isolation": "on", "compute_host_heartbeat_secs": "20", "compute_host_respawn_max": 5}},
    io={"is_truthy_value": True, "_coerce_int_config_value": [20, 5]})
add("_load_dashboard_process_isolation_config", cfg=None,
    io={"_load_cfg": {"dashboard": {}}, "is_truthy_value": False, "_coerce_int_config_value": [15, 3]})
add("_load_dashboard_process_isolation_config", cfg={"dashboard": "bad"},
    io={"is_truthy_value": False, "_coerce_int_config_value": [15, 3]})
add("_load_dashboard_process_isolation_config", cfg="not-dict",
    io={"is_truthy_value": False, "_coerce_int_config_value": [15, 3]})
add("_load_dashboard_process_isolation_config", cfg={},
    io={"is_truthy_value": False, "_coerce_int_config_value": [15, 3]})
add("_load_dashboard_process_isolation_config", cfg=None,
    io={"_load_cfg": None, "is_truthy_value": False, "_coerce_int_config_value": [15, 3]})  # root None -> dash {}

# ── _active_config_path (seam: get_hermes_home_override) ─────────────────────
add("_active_config_path", state=st(), io={"get_hermes_home_override": None})
add("_active_config_path", state=st(), io={"get_hermes_home_override": "/h/override"})
add("_active_config_path", state=st(), io={"get_hermes_home_override": ""})
add("_active_config_path", state=st(), io={"get_hermes_home_override": 5})
add("_active_config_path", state=st(home={"hermes_home": "/h/other", "at_import": "/h/launch"}), io={"get_hermes_home_override": None})
add("_active_config_path", state=st(), io={"get_hermes_home_override": raise_spec("RuntimeError", "boom")})

# ── _load_cfg (seams: _active_config_path, load_user_config_effective) ───────
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": {"a": 1}})
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": {}})
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": raise_spec("ValueError", "bad")})
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": raise_spec("SystemExit", "bye")})
add("_load_cfg", io={"_active_config_path": raise_spec("RuntimeError", "pathfail"), "load_user_config_effective": {}})
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": raise_spec("ConfigBackendUnavailable", "backend")})
add("_load_cfg", io={"_active_config_path": "/h/launch/config.yaml", "load_user_config_effective": {"display": {"pet": {}}}})

# ── _drop_cfg_cache ──────────────────────────────────────────────────────────
add("_drop_cfg_cache", state=st(cfg={"cache": {"a": 1}, "sig": 3, "path": "/h/launch/config.yaml"}))
add("_drop_cfg_cache", state=st())
add("_drop_cfg_cache", state=st(cfg={"cache": {}, "sig": None, "path": "/x"}))
add("_drop_cfg_cache", state=st(cfg={"cache": {"k": [1]}, "sig": [1, 2], "path": "/y"}))
add("_drop_cfg_cache", state=st(cfg={"cache": None, "sig": 9, "path": None}))

# ── _load_cfg_raw ────────────────────────────────────────────────────────────
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1, "config_exists": True, "read_user_config_raw": {"a": 1}})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1, "config_exists": False})
add("_load_cfg_raw", state=st(cfg={"cache": {"cached": 1}, "sig": 1, "path": "/h/launch/config.yaml"}),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1})
add("_load_cfg_raw", state=st(cfg={"cache": {"cached": 1}, "sig": 2, "path": "/h/launch/config.yaml"}),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1, "config_exists": True, "read_user_config_raw": {"fresh": 1}})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": raise_spec("FileNotFoundError", "absent"), "config_exists": True, "read_user_config_raw": {"b": 2}})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1, "config_exists": True, "read_user_config_raw": raise_spec("ValueError", "parse")})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": 1, "config_exists": raise_spec("ConfigBackendUnavailable", "be")})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "config_cache_version": raise_spec("SystemExit", "exit")})
add("_load_cfg_raw", state=st(),
    io={"_active_config_path": raise_spec("RuntimeError", "pf"), "config_cache_version": 1})

# ── _save_cfg ────────────────────────────────────────────────────────────────
add("_save_cfg", cfg={"a": 1}, state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": None, "supports_file_tooling": True, "config_cache_version": 5})
add("_save_cfg", cfg={"a": 1}, state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": None, "supports_file_tooling": False, "_drop_cfg_cache": None})
add("_save_cfg", cfg={"a": 1}, state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": None, "supports_file_tooling": True, "config_cache_version": raise_spec("OSError", "stat")})
add("_save_cfg", cfg={"a": 1}, state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": raise_spec("RuntimeError", "write")})
add("_save_cfg", cfg={"a": 1}, state=st(),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": None, "supports_file_tooling": True, "config_cache_version": raise_spec("ConfigBackendUnavailable", "be")})
add("_save_cfg", cfg={"a": 1}, state=st(cfg={"cache": {"old": 1}, "sig": 9, "path": "/old"}),
    io={"_active_config_path": "/h/launch/config.yaml", "atomic_config_replace": None, "supports_file_tooling": True, "config_cache_version": raise_spec("SystemExit", "x")})

# ── _write_config_changes ────────────────────────────────────────────────────
add("_write_config_changes", sets={"a.b": 1}, unsets=["c"],
    io={"_active_config_path": "/h/launch/config.yaml", "write_changes": None, "_drop_cfg_cache": None})
add("_write_config_changes", sets={}, unsets=[],
    io={"_active_config_path": "/h/launch/config.yaml", "write_changes": None, "_drop_cfg_cache": None})
add("_write_config_changes", sets={"k": "v"}, unsets=[],
    io={"_active_config_path": "/h/launch/config.yaml", "write_changes": raise_spec("RuntimeError", "locked"), "_drop_cfg_cache": None})
add("_write_config_changes", sets={"k": "v"}, unsets=["u"],
    io={"_active_config_path": "/h/launch/config.yaml", "write_changes": raise_spec("SystemExit", "bye"), "_drop_cfg_cache": None})
add("_write_config_changes", sets={"k": 1}, unsets=[],
    io={"_active_config_path": raise_spec("RuntimeError", "pf"), "write_changes": None, "_drop_cfg_cache": None})
add("_write_config_changes", sets={"k": 1}, unsets=[],
    io={"_active_config_path": "/h/launch/config.yaml", "write_changes": None, "_drop_cfg_cache": raise_spec("RuntimeError", "dropfail")})

# ── _write_config_key (seam: _write_config_keys) ─────────────────────────────
add("_write_config_key", key_path="a.b", value=1, io={"_write_config_keys": None})
add("_write_config_key", key_path="leaf", value="v", io={"_write_config_keys": None})
add("_write_config_key", key_path="x", value=None, io={"_write_config_keys": None})
add("_write_config_key", key_path="a.b.c", value=[1], io={"_write_config_keys": None})
add("_write_config_key", key_path="k", value=1, io={"_write_config_keys": raise_spec("RuntimeError", "refused")})

# ── _write_config_keys ───────────────────────────────────────────────────────
add("_write_config_keys", updates={"a.b": 1},
    io={"_refuse_managed_keys": None, "supports_file_tooling": False, "_write_config_changes": None})
add("_write_config_keys", updates={"a.b": 1, "c": 2},
    io={"_refuse_managed_keys": None, "supports_file_tooling": True, "_load_cfg_raw": {}, "_save_cfg": None})
add("_write_config_keys", updates={"leaf": 5},
    io={"_refuse_managed_keys": None, "supports_file_tooling": True, "_load_cfg_raw": {"leaf": 0}, "_save_cfg": None})
add("_write_config_keys", updates={"a.b.c": 9},
    io={"_refuse_managed_keys": None, "supports_file_tooling": True, "_load_cfg_raw": {"a": {"b": {"old": 1}}}, "_save_cfg": None})
add("_write_config_keys", updates={"a.b": 1},
    io={"_refuse_managed_keys": raise_spec("RuntimeError", "managed")})
add("_write_config_keys", updates={"a.b": 1},
    io={"_refuse_managed_keys": None, "supports_file_tooling": True, "_load_cfg_raw": {"a": "not-dict"}, "_save_cfg": None})
add("_write_config_keys", updates={},
    io={"_refuse_managed_keys": None, "supports_file_tooling": True, "_load_cfg_raw": {"z": 1}, "_save_cfg": None})

# ── _is_model_switch_marker ──────────────────────────────────────────────────
add("_is_model_switch_marker", entry={"content": MARKER_PREFIX + "gpt via provider x.]"})
add("_is_model_switch_marker", entry={"content": "plain text"})
add("_is_model_switch_marker", entry={"content": 5})
add("_is_model_switch_marker", entry="not-dict")
add("_is_model_switch_marker", entry={})
add("_is_model_switch_marker", entry={"content": MARKER_PREFIX})
add("_is_model_switch_marker", entry=None)

# ── _is_pivot_marker (seam: _is_model_switch_marker) ─────────────────────────
add("_is_pivot_marker", entry={"display_kind": "personality_switch"}, io={"_is_model_switch_marker": False})
add("_is_pivot_marker", entry={"content": MARKER_PREFIX + "m"}, io={"_is_model_switch_marker": True})
add("_is_pivot_marker", entry={"display_kind": "other"}, io={"_is_model_switch_marker": False})
add("_is_pivot_marker", entry="not-dict", io={"_is_model_switch_marker": False})
add("_is_pivot_marker", entry={}, io={"_is_model_switch_marker": False})
add("_is_pivot_marker", entry={"display_kind": "personality_switch"}, io={"_is_model_switch_marker": True})

# ── _turn_started_at ─────────────────────────────────────────────────────────
add("_turn_started_at", session={"inflight_turn": {"started_at": 123.5}})
add("_turn_started_at", session={"inflight_turn": {"started_at": "100"}})
add("_turn_started_at", session={"inflight_turn": {"started_at": 0}})
add("_turn_started_at", session={"inflight_turn": {}})
add("_turn_started_at", session={"inflight_turn": "not-dict"})
add("_turn_started_at", session=None)
add("_turn_started_at", session={})
add("_turn_started_at", session={"inflight_turn": {"started_at": "1e3"}})

# ── _cli_exec_blocked ────────────────────────────────────────────────────────
add("_cli_exec_blocked", argv=[])
add("_cli_exec_blocked", argv=None)
add("_cli_exec_blocked", argv=["setup"])
add("_cli_exec_blocked", argv=["SETUP", "extra"])
add("_cli_exec_blocked", argv=["sessions", "browse"])
add("_cli_exec_blocked", argv=["config", "edit"])
add("_cli_exec_blocked", argv=["gateway"])
add("_cli_exec_blocked", argv=["chat", "-q", "hi"])
add("_cli_exec_blocked", argv=["sessions"])

# ── _clarify_timeout_seconds (seam: get_clarify_timeout) ─────────────────────
add("_clarify_timeout_seconds", io={"get_clarify_timeout": 120})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": 0})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": -5})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": raise_spec("ValueError", "cfg")})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": raise_spec("SystemExit", "bye")})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": None})
add("_clarify_timeout_seconds", io={"get_clarify_timeout": 0.5})

# ── _ask (seam: send) ────────────────────────────────────────────────────────
add("_ask", method="sudo", sid="s1", params={"p": 1}, timeout=300, io={"send": {"value": "secret"}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": None})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {"value": 5}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {"value": None}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {"value": {"a": 1}}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {"value": [1, 2]}})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": raise_spec("TimeoutError", "late")})
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": [1]})  # truthy non-dict -> AttributeError
add("_ask", method="sudo", sid="s1", params={}, timeout=300, io={"send": {"value": True}})

# ── _clarify_block (seams: _clarify_timeout_seconds, send) ───────────────────
add("_clarify_block", sid="s1", questions=[{"qid": "q1", "question": "Q?", "choices": ["a"], "multi_select": False}],
    io={"_clarify_timeout_seconds": 120, "send": {"answers": {"q1": "a"}, "outcome": "answered"}})
add("_clarify_block", sid="s1", questions=[{"qid": "q1", "question": "Q?", "choices": [], "multi_select": True}],
    io={"_clarify_timeout_seconds": None, "send": None})
add("_clarify_block", sid="s1", questions=[],
    io={"_clarify_timeout_seconds": 300, "send": {}})
add("_clarify_block", sid="s1", questions=[{"question": "Q?", "choices": [], "multi_select": False}],
    io={"_clarify_timeout_seconds": 300, "send": None})  # missing qid -> KeyError
add("_clarify_block", sid="s1", questions=[{"qid": "q1", "question": "Q?", "choices": ["a"], "multi_select": False}],
    io={"_clarify_timeout_seconds": 300, "send": raise_spec("SystemExit", "bye")})
add("_clarify_block", sid="s1", questions=[{"qid": "q1", "question": "Q?", "choices": ["a"], "multi_select": 1}],
    io={"_clarify_timeout_seconds": 300, "send": {"answers": {}, "outcome": "undelivered"}})

# ── _tour_request (seam: _ask; state.sessions) ───────────────────────────────
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}), io={"_ask": "ok"})
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"tour_bridge": "answered"}}), io={"_ask": "ok"})
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"tour_bridge": "unanswered"}}), io={"_ask": "ok"})
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}), io={"_ask": ""})
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"tour_bridge": "answered"}}), io={"_ask": ""})
add("_tour_request", sid="ghost", payload={"a": 1}, state=st(sessions={}), io={"_ask": "ok"})  # detached
add("_tour_request", sid="ghost", payload={"a": 1}, state=st(sessions={}), io={"_ask": ""})  # detached unanswered
add("_tour_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}), io={"_ask": raise_spec("SystemExit", "bye")})

# ── _preview_action_request (seams: _ask, monotonic) ─────────────────────────
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}),
    io={"monotonic": [100, 200], "_ask": "ok"})
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"preview_action_bridge": "answered"}}),
    io={"monotonic": [100], "_ask": "ok"})
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"preview_action_bridge": "unanswered", "preview_action_bridge_retry_at": 500}}),
    io={"monotonic": [100], "_ask": "ok"})  # now < retry_at -> unavailable
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"preview_action_bridge": "unanswered", "preview_action_bridge_retry_at": 50}}),
    io={"monotonic": [100, 300], "_ask": ""})  # reprobe, unanswered -> cooldown
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {"preview_action_bridge": "unanswered"}}, reprobe={"s1": True}),
    io={"monotonic": [100], "_ask": "ok"})  # reprobe in flight -> unavailable
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}),
    io={"monotonic": [100, 200], "_ask": ""})  # unanswered fresh
add("_preview_action_request", sid="ghost", payload={"a": 1}, state=st(sessions={}),
    io={"monotonic": [100, 200], "_ask": "ok"})  # detached
add("_preview_action_request", sid="s1", payload={"a": 1}, state=st(sessions={"s1": {}}),
    io={"monotonic": [100], "_ask": raise_spec("SystemExit", "bye")})  # finally pops reprobe

# ── _status_update (seam: _emit) ─────────────────────────────────────────────
add("_status_update", sid="s1", kind="thinking", text=None, io={"_emit": True})
add("_status_update", sid="s1", kind="thinking", text="hello", io={"_emit": True})
add("_status_update", sid="s1", kind="thinking", text="   ", io={"_emit": True})  # empty body -> return
add("_status_update", sid="s1", kind="  ", text=None, io={"_emit": True})  # empty kind -> return
add("_status_update", sid="s1", kind="tool", text="  run  ", io={"_emit": False})
add("_status_update", sid="s1", kind="x", text="", io={"_emit": True})  # text empty -> return

# ── _resolve_name (seam: resolve_command) ────────────────────────────────────
add("_resolve_name", name="foo", io={"resolve_command": {"name": "foobar"}})
add("_resolve_name", name="foo", io={"resolve_command": None})
add("_resolve_name", name="foo", io={"resolve_command": {}})
add("_resolve_name", name="foo", io={"resolve_command": raise_spec("ValueError", "bad")})
add("_resolve_name", name="foo", io={"resolve_command": raise_spec("SystemExit", "bye")})
add("_resolve_name", name="foo", io={"resolve_command": {"name": ""}})
add("_resolve_name", name="foo", io={"resolve_command": "plain"})  # truthy non-ns -> .name AttributeError -> suppress

# ── _finish_reload (seams: save_config_value, _ok, log) ──────────────────────
add("_finish_reload", rid="r1", params={"always": True}, coalesced=False, state=st(mcp={"loaded_rev": "rev9"}),
    io={"save_config_value": None, "_ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "reloaded", "loaded_rev": "rev9"}}})
add("_finish_reload", rid="r1", params={}, coalesced=True, state=st(mcp={"loaded_rev": "rev9"}),
    io={"_ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "reloaded", "loaded_rev": "rev9", "coalesced": True}}})
add("_finish_reload", rid="r1", params={"always": False}, coalesced=False, state=st(),
    io={"_ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "reloaded", "loaded_rev": "rev0"}}})
add("_finish_reload", rid="r1", params={"always": True}, coalesced=False, state=st(),
    io={"save_config_value": raise_spec("RuntimeError", "savefail"), "log": None,
        "_ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "reloaded", "loaded_rev": "rev0"}}})
add("_finish_reload", rid="r1", params={"always": True}, coalesced=False, state=st(),
    io={"save_config_value": raise_spec("SystemExit", "bye"), "_ok": {"jsonrpc": "2.0", "id": "r1", "result": {}}})
add("_finish_reload", rid="r1", params={"always": "yes"}, coalesced=True, state=st(mcp={"loaded_rev": "abc"}),
    io={"save_config_value": None, "_ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "reloaded", "loaded_rev": "abc", "coalesced": True}}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    funcs = sorted(counts)
    short = [k for k in funcs if counts[k] < 5]
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(funcs), OUT))
    for k in funcs:
        print("  %-42s %d" % (k, counts[k]))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()