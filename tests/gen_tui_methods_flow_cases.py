#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_flow.jsonl — the scripted-seam FLOW
cases for the tui_gateway/methods_profiles.py port (tui_methods_flow).

Deterministic; >=5 cases per ported func. Each case is
{"func", "in": {"args", "io", "state"}, "fail_closed"?}. io maps seam name ->
script (plain value reused, list consumed per call, or {"__script__": true,
"ret"/"raise"}). A missing script is the fail-closed RuntimeError probe.

Fixture domain (shared with the port header): ASCII strings; absolute POSIX
paths without trailing slash; ints only (no float literals in compared
positions); row/persistence/writer scripts are dicts or null; ui_meta incoming
is a dict with str/int/bool/null/list/dict values; _save_mcp_toggles copies
from launch_mcp only with a ONE-member wanted set.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_flow.jsonl")

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


def raise_script(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def row(sid="s1", source="web", archived=False, title="Bot Chat", started_at=10,
        last_activity_at=20, message_count=3, end_reason=None):
    return {"session_id": sid, "source": source, "archived": archived, "title": title,
            "started_at": started_at, "last_activity_at": last_activity_at,
            "message_count": message_count, "end_reason": end_reason}


P = "/h/p1"

# ── _model_provider_params ──
add("_model_provider_params", {"params": {"model": " m ", "provider": " p "}})
add("_model_provider_params", {"params": {}})
add("_model_provider_params", {"params": {"model": None, "provider": ""}})
add("_model_provider_params", {"params": {"model": 0, "provider": 5}})
add("_model_provider_params", {"params": {"model": "   ", "provider": True}})
add("_model_provider_params", {"params": {"model": "gpt", "provider": "nous"}})

# ── _clean_revisions ──
add("_clean_revisions", {"raw": {"a": -3, "b": True, "c": "x", "d": 7, "e": None}})
add("_clean_revisions", {"raw": {}})
add("_clean_revisions", {"raw": {"a": "1", "b": False, "c": None}})
add("_clean_revisions", {"raw": {"a": 0, "b": -100, "c": 42}})
add("_clean_revisions", {"raw": {"a": 5.5, "b": 2}})
add("_clean_revisions", {"raw": {"k": 999999999999}})

# ── _clean_names ──
add("_clean_names", {"values": [" b ", "a", "", "b", "  "]})
add("_clean_names", {"values": []})
add("_clean_names", {"values": ["", "   ", "\t"]})
add("_clean_names", {"values": [5, True, None, "x"]})
add("_clean_names", {"values": ["solo"]})
add("_clean_names", {"values": ["z", "a", "m", "a"]})

# ── _bots_title ──
add("_bots_title", {"ui_meta": {"hermes-bots": {"title": "T"}}})
add("_bots_title", {"ui_meta": {"hermes-bots": {}}})
add("_bots_title", {"ui_meta": {"hermes-bots": "nope"}})
add("_bots_title", {"ui_meta": {}})
add("_bots_title", {"ui_meta": None})
add("_bots_title", {"ui_meta": {"hermes-bots": {"title": None}}})

# ── _mcp_toggle_state ──
add("_mcp_toggle_state", {"servers": {"a": {"enabled": True, "disabled": False},
                                      "b": {"enabled": False}, "c": "skip"}})
add("_mcp_toggle_state", {"servers": {"a": {}}})
add("_mcp_toggle_state", {"servers": {"a": {"disabled": True}}})
add("_mcp_toggle_state", {"servers": "passthrough"})
add("_mcp_toggle_state", {"servers": None})
add("_mcp_toggle_state", {"servers": {}})

# ── _try ──
add("_try", {"default": 9}, {"try_fn": "ok"})
add("_try", {"default": 9}, {"try_fn": raise_script("ValueError", "x")})
add("_try", {"default": None}, {"try_fn": raise_script("RuntimeError", "y")})
add("_try", {"default": 9}, {"try_fn": raise_script("SystemExit", "bye")})
add("_try", {"default": 9}, {}, fail_closed=True)
add("_try", {"default": "d"}, {"try_fn": 0})

# ── _best_effort ──
add("_best_effort", {}, {"effort_fn": "discarded"})
add("_best_effort", {}, {"effort_fn": raise_script("ValueError", "x")})
add("_best_effort", {}, {"effort_fn": raise_script("SystemExit", "bye")})
add("_best_effort", {}, {}, fail_closed=True)
add("_best_effort", {}, {"effort_fn": None})

# ── _lazy ──
add("_lazy", {"module": "hermes_cli.profiles", "name": "read_profile_meta"},
    {"import_attr": "marker"})
add("_lazy", {"module": "m", "name": "n"}, {"import_attr": {"fn": 1}})
add("_lazy", {"module": "m", "name": "n"},
    {"import_attr": raise_script("AttributeError", "module 'm' has no attribute 'n'")})
add("_lazy", {"module": "m", "name": "n"}, {"import_attr": None})
add("_lazy", {"module": "m", "name": "n"}, {}, fail_closed=True)

# ── _pin_profile_model ──
add("_pin_profile_model", {"profile_dir": P, "provider": "nous", "model": "m"},
    {"import_attr": "write_profile_model", "write_profile_model": None})
add("_pin_profile_model", {"profile_dir": P, "provider": "p", "model": "m", "base_url": "u"},
    {"import_attr": "w", "write_profile_model": None})
add("_pin_profile_model", {"profile_dir": P, "provider": "p", "model": "m"},
    {"import_attr": raise_script("ImportError", "no module")})
add("_pin_profile_model", {"profile_dir": P, "provider": "p", "model": "m"},
    {"import_attr": "w", "write_profile_model": raise_script("OSError", "disk")})
add("_pin_profile_model", {"profile_dir": P, "provider": "p", "model": "m"},
    {"import_attr": "w", "write_profile_model": raise_script("SystemExit", "bye")})
add("_pin_profile_model", {"profile_dir": P, "provider": "p", "model": "m"},
    {}, fail_closed=True)

# ── _hermes_home_scope ──
add("_hermes_home_scope", {"path": P},
    {"launch_home": "/h/launch", "profile_config_scope": {}})
add("_hermes_home_scope", {"path": P},
    {"launch_home": None, "profile_config_scope": {"scope": 1}})
add("_hermes_home_scope", {"path": P},
    {"launch_home": raise_script("RuntimeError", "home")})
add("_hermes_home_scope", {"path": P},
    {"launch_home": "/h", "profile_config_scope": raise_script("ValueError", "scope")})
add("_hermes_home_scope", {"path": P},
    {"launch_home": "/h", "profile_config_scope": None})
add("_hermes_home_scope", {"path": P}, {}, fail_closed=True)

# ── _resolve_profile ──
ERR4063 = {"jsonrpc": "2.0", "id": "r1", "error": {"code": 4063, "message": "name required"}}
ERR4064 = {"jsonrpc": "2.0", "id": "r1", "error": {"code": 4064, "message": "profile 'x' not found"}}
add("_resolve_profile", {"rid": "r1", "params": {"name": "  "}}, {"err": ERR4063})
add("_resolve_profile", {"rid": "r1", "params": {"name": " x "}},
    {"get_profile_dir": "/h/x", "is_dir": True})
add("_resolve_profile", {"rid": "r1", "params": {"name": "x"}},
    {"get_profile_dir": raise_script("ValueError", "bad"), "err": ERR4064})
add("_resolve_profile", {"rid": "r1", "params": {"name": "x"}},
    {"get_profile_dir": "/h/x", "is_dir": False, "err": ERR4064})
add("_resolve_profile", {"rid": "r1", "params": {"name": "x"}},
    {"get_profile_dir": raise_script("RuntimeError", "boom")})
add("_resolve_profile", {"rid": "r1", "params": {"name": "x"}},
    {"get_profile_dir": "/h/x", "is_dir": raise_script("SystemExit", "bye")})
add("_resolve_profile", {"rid": "r1", "params": {"name": "o'brien"}},
    {"get_profile_dir": "/h/o", "is_dir": True})
add("_resolve_profile", {"rid": "r1", "params": {"name": "x"}}, {}, fail_closed=True)

# ── _read_profile_yaml ──
add("_read_profile_yaml", {"profile_dir": P}, {"is_file": False})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": True, "read_text": "ui_meta: {}", "yaml_safe_load": {"ui_meta": {"a": 1}}})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": True, "read_text": "t", "yaml_safe_load": None})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": True, "read_text": "t", "yaml_safe_load": [1, 2]})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": True, "read_text": raise_script("OSError", "locked")})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": True, "read_text": "t", "yaml_safe_load": raise_script("SystemExit", "bye")})
add("_read_profile_yaml", {"profile_dir": P},
    {"is_file": raise_script("PermissionError", "no")})
add("_read_profile_yaml", {"profile_dir": P}, {}, fail_closed=True)

# ── _latest_message_preview ──
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": "hello world"})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": "  a   b\t c\n "})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": "x" * 120})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": None})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": 42})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": raise_script("RuntimeError", "db")})
add("_latest_message_preview", {"persistence": None, "session_id": "s1"}, {})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": raise_script("SystemExit", "bye")})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": "y" * 80})
add("_latest_message_preview", {"persistence": {}, "session_id": "s1"},
    {"latest_dialogue_message": "a\x1cb\x1dc"})

# ── _display_count_field ──
add("_display_count_field", {"persistence": {}, "session_id": "s1"},
    {"display_count": 12})
add("_display_count_field", {"persistence": {}, "session_id": "s1"},
    {"display_count": raise_script("StoreUnavailable", "locked")})
add("_display_count_field", {"persistence": {}, "session_id": "s1"},
    {"display_count": raise_script("TransientStoreError", "busy")})
add("_display_count_field", {"persistence": {}, "session_id": "s1"},
    {"display_count": raise_script("RuntimeError", "other")})
add("_display_count_field", {"persistence": {}, "session_id": "s1"},
    {"display_count": raise_script("SystemExit", "bye")})
add("_display_count_field", {"persistence": None, "session_id": "s1"}, {})
add("_display_count_field", {"persistence": {}, "session_id": 5},
    {"display_count": 0})

# ── _live_count_field ──
add("_live_count_field", {"db": "db1", "session_id": "s1"},
    {"persistence_for": {}, "display_count": 7})
add("_live_count_field", {"db": "db1", "session_id": "s1"},
    {"persistence_for": None})
add("_live_count_field", {"db": "db1", "session_id": "s1"},
    {"persistence_for": {}, "display_count": raise_script("StoreReplaced", "gone")})
add("_live_count_field", {"db": "db1", "session_id": "s1"},
    {"persistence_for": raise_script("RuntimeError", "open")})
add("_live_count_field", {"db": "db1", "session_id": "s1"}, {}, fail_closed=True)
add("_live_count_field", {"db": "db1", "session_id": "s1"},
    {"persistence_for": {}, "display_count": raise_script("PermanentStoreError", "bad")})

# ── _mcp_entry_enabled ──
add("_mcp_entry_enabled", {"entry": {"enabled": True}}, {"mcp_server_enabled": True})
add("_mcp_entry_enabled", {"entry": {"disabled": True}}, {"mcp_server_enabled": False})
add("_mcp_entry_enabled", {"entry": {}},
    {"mcp_server_enabled": raise_script("ValueError", "bad entry")})
add("_mcp_entry_enabled", {"entry": {"url": "u"}}, {"mcp_server_enabled": True})
add("_mcp_entry_enabled", {"entry": {}}, {}, fail_closed=True)

# ── _save_toolset_pin ──
add("_save_toolset_pin", {"enabled": [" b ", "a", "b"]},
    {"save_platform_tools": None, "save_config": None},
    {"cfg": {"platform_toolsets": {"cli": ["old"]}}})
add("_save_toolset_pin", {"enabled": []},
    {"save_config": None},
    {"cfg": {"platform_toolsets": {"cli": ["old"], "web": ["w"]}}})
add("_save_toolset_pin", {"enabled": ["  "]},
    {"save_config": None}, {"cfg": {}})
add("_save_toolset_pin", {"enabled": []},
    {"save_config": None}, {"cfg": {"platform_toolsets": "notdict"}})
add("_save_toolset_pin", {"enabled": ["a"]},
    {"save_platform_tools": None, "save_config": raise_script("OSError", "save")},
    {"cfg": {}})
add("_save_toolset_pin", {"enabled": ["a"]},
    {"save_platform_tools": raise_script("SystemExit", "bye")}, {"cfg": {}})

# ── _save_mcp_toggles ──
add("_save_mcp_toggles", {"enabled": ["a"], "launch_mcp": {}},
    {"write_mcp_server_enabled": None, "save_config": None},
    {"cfg": {"mcp_servers": {"a": {"url": "u"}, "b": {"enabled": True}}}})
add("_save_mcp_toggles", {"enabled": ["new"], "launch_mcp": {"new": {"url": "lu"}}},
    {"write_mcp_server_enabled": None, "save_config": None},
    {"cfg": {}})
add("_save_mcp_toggles", {"enabled": [], "launch_mcp": {}},
    {"save_config": None}, {"cfg": {"mcp_servers": "notdict"}})
add("_save_mcp_toggles", {"enabled": ["a"], "launch_mcp": {}},
    {"write_mcp_server_enabled": None, "save_config": None},
    {"cfg": {"mcp_servers": {"a": {"url": "u"}, "skip": "notdict"}}})
add("_save_mcp_toggles", {"enabled": ["missing"], "launch_mcp": None},
    {}, {"cfg": {}})
add("_save_mcp_toggles", {"enabled": ["a"], "launch_mcp": {}},
    {"write_mcp_server_enabled": raise_script("OSError", "w"), "save_config": None},
    {"cfg": {"mcp_servers": {"a": {}}}})
add("_save_mcp_toggles", {"enabled": ["a", "b"], "launch_mcp": {}},
    {"write_mcp_server_enabled": [None, None], "save_config": None},
    {"cfg": {"mcp_servers": {"a": {}, "b": {}}}})
add("_save_mcp_toggles", {"enabled": ["a"], "launch_mcp": {}},
    {"write_mcp_server_enabled": None, "save_config": raise_script("SystemExit", "bye")},
    {"cfg": {"mcp_servers": {"a": {}}}})

# ── _unlink_asset_files ──
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [True, True, True], "unlink": [None, None, None]})
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [False, False, False]})
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [True, False, True], "unlink": [None, None]})
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [True, raise_script("OSError", "stat")]})
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [True, True, False], "unlink": [None, raise_script("PermissionError", "rm")]})
add("_unlink_asset_files", {"assets_dir": "/h/p1/assets", "asset": "avatar"},
    {"is_file": [True, False, False], "unlink": [raise_script("SystemExit", "bye")]})

# ── _landed ──
add("_landed", {"cfg": {"skills": {"disabled": ["a"]}}, "path": ["skills", "disabled"]},
    {"read_user_config_raw": {"skills": {"disabled": ["a"]}}})
add("_landed", {"cfg": {"skills": {"disabled": ["a"]}}, "path": ["skills", "disabled"]},
    {"read_user_config_raw": {"skills": {"disabled": ["b"]}}})
add("_landed", {"cfg": {}, "path": ["skills", "disabled"]},
    {"read_user_config_raw": {}})
add("_landed", {"cfg": {"mcp_servers": {"a": {"enabled": True}}},
                "path": ["mcp_servers"], "key": "mcp_toggle_state"},
    {"read_user_config_raw": {"mcp_servers": {"a": {"enabled": True}}}})
add("_landed", {"cfg": {"mcp_servers": {"a": {"enabled": True}}},
                "path": ["mcp_servers"], "key": "mcp_toggle_state"},
    {"read_user_config_raw": {"mcp_servers": {"a": {"enabled": True, "disabled": True}}}})
add("_landed", {"cfg": {"a": 1}, "path": ["a"]},
    {"read_user_config_raw": None})
add("_landed", {"cfg": {"a": 1}, "path": ["a"]},
    {"read_user_config_raw": raise_script("RuntimeError", "cfg")})
add("_landed", {"cfg": {"a": "notdict"}, "path": ["a", "b"]},
    {"read_user_config_raw": {"a": {"b": 1}}})

# ── _configure_model ──
add("_configure_model", {"profile_dir": P, "params": {}}, {}, {"applied": {}})
add("_configure_model", {"profile_dir": P,
                         "params": {"model": "m", "provider": "p", "confirm_expensive_model": True}},
    {"import_attr": "w", "write_profile_model": None}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": None,
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": {"message": "expensive model"}}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": {"context_tokens": 9},
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": {}}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": None,
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": raise_script("ValueError", "guard")}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn",
     "profile_group_selection_context": raise_script("RuntimeError", "store"),
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": {"message": "warn"}}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": None,
     "launch_home": "/h/launch", "profile_config_scope": None}, {"applied": {}})
add("_configure_model", {"profile_dir": P,
                         "params": {"model": "m", "provider": "p", "confirm_expensive_model": "yes"}},
    {"import_attr": "w", "write_profile_model": raise_script("OSError", "pin")},
    {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": None,
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": raise_script("SystemExit", "bye")}, {"applied": {}})
add("_configure_model", {"profile_dir": P, "params": {"model": "m", "provider": "p"}},
    {"import_attr": "warn", "profile_group_selection_context": None,
     "launch_home": "/h/launch", "profile_config_scope": {},
     "combined_selection_warning": {"message": ""}}, {"applied": {}})

# ── _configure_ui_meta ──
YAML_OK = {"is_file": True, "read_text": "doc",
           "yaml_safe_load": {"ui_meta": {"a": 1}, "_ui_meta_revisions": {"a": 2}}}
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"a": 5, "b": 2}}},
    dict(YAML_OK, atomic_yaml_write=None), {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"big": "x" * 70000}}},
    {}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P,
                           "params": {"ui_meta": {"a": 1}, "ui_meta_expected_revisions": "nope"}},
    {}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P,
                           "params": {"ui_meta": {"a": 9}, "ui_meta_expected_revisions": {"a": 5}}},
    dict(YAML_OK), {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P,
                           "params": {"ui_meta": {"a": 9}, "ui_meta_expected_revisions": {"a": 2}}},
    dict(YAML_OK, atomic_yaml_write=None), {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"a": None}}},
    {"is_file": True, "read_text": "doc",
     "yaml_safe_load": {"ui_meta": {"a": 1}, "_ui_meta_revisions": {"a": 2}},
     "atomic_yaml_write": None}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P,
                           "params": {"ui_meta": {"hermes-bots": {"title": "New"}}, "name": "p1"}},
    {"is_file": True, "read_text": "doc",
     "yaml_safe_load": {"ui_meta": {"hermes-bots": {"title": "Old"}}},
     "atomic_yaml_write": None, "logger_info": None}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"a": 1}}},
    dict(YAML_OK, atomic_yaml_write=raise_script("OSError", "write")), {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {}}, {}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"a": 1}}},
    {"is_file": True, "read_text": "doc",
     "yaml_safe_load": raise_script("SystemExit", "bye")}, {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P, "params": {"ui_meta": {"a": None}}},
    {"is_file": True, "read_text": "doc",
     "yaml_safe_load": {"ui_meta": {"a": 1}}, "atomic_yaml_write": None},
    {"applied": {}})
add("_configure_ui_meta", {"profile_dir": P,
                           "params": {"ui_meta": {"hermes-bots": {"title": "New"}}}},
    {"is_file": True, "read_text": "doc",
     "yaml_safe_load": {"ui_meta": {"hermes-bots": {"title": "Old"}}},
     "atomic_yaml_write": None, "logger_info": None}, {"applied": {}})

# ── _resurrect_recoverable_canonical ──
RROW = row("s1", archived=True, end_reason="ws_orphan_reap")
RTIP = row("tip1", archived=True, end_reason="agent_close", title="Tip")
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": [RROW, RTIP], "lineage_tip": "tip1", "profile_scope": {},
     "open_persistence": {}, "unarchive_recoverable": True, "writer_close": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": row("s1", archived=False)})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": RROW, "lineage_tip": None, "profile_scope": {},
     "open_persistence": {}, "unarchive_recoverable": False, "writer_close": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": row("s1", archived=True, end_reason="session_reset"),
     "lineage_tip": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": [RROW, RTIP], "lineage_tip": "tip1", "profile_scope": {},
     "open_persistence": {}, "unarchive_recoverable": raise_script("RuntimeError", "unarc"),
     "writer_close": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": [RROW, RTIP], "lineage_tip": "tip1", "profile_scope": {},
     "open_persistence": {}, "unarchive_recoverable": True,
     "writer_close": raise_script("SystemExit", "bye")})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": [RROW, RTIP], "lineage_tip": "tip1", "profile_scope": {},
     "open_persistence": None})
add("_resurrect_recoverable_canonical",
    {"persistence": {}, "profile_path": P, "session_id": "s1"},
    {"sessions_get": RROW, "lineage_tip": raise_script("RuntimeError", "tip"),
     "profile_scope": {}, "open_persistence": {}, "unarchive_recoverable": True,
     "writer_close": None})
add("_resurrect_recoverable_canonical",
    {"persistence": None, "profile_path": P, "session_id": "s1"}, {})

# ── _canonical_session_row ──
CROW = row("s1", title="Bot Chat", started_at=10)
CTIP = row("tip1", title="Tip", started_at=11, last_activity_at=21, message_count=4)
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": CROW, "denied_source": False, "lineage_tip": "tip1",
     "sessions_get": CTIP, "latest_dialogue_message": "hello", "display_count": 4})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": None})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": row("  ")})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": CROW, "denied_source": True})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": row("s1", archived=True), "denied_source": False,
     "resurrect_recoverable_canonical": False})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": row("s1", archived=True), "denied_source": False,
     "resurrect_recoverable_canonical": True, "lineage_tip": None,
     "sessions_get": None, "latest_dialogue_message": "prev", "display_count": 2})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": CROW, "denied_source": False, "lineage_tip": None,
     "sessions_get": None, "latest_dialogue_message": "z" * 120, "display_count": 3})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": CROW, "denied_source": False, "lineage_tip": "tip1",
     "sessions_get": CTIP, "latest_dialogue_message": "hello",
     "display_count": raise_script("StoreUnavailable", "locked")})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": raise_script("RuntimeError", "db")})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": {"session_id": "s1", "source": "web", "archived": False,
                      "started_at": 10}, "denied_source": False})
add("_canonical_session_row", {"persistence": None, "profile_path": P}, {})
add("_canonical_session_row", {"persistence": {}, "profile_path": P},
    {"get_by_title": CROW, "denied_source": False, "lineage_tip": "tip1",
     "sessions_get": CTIP, "latest_dialogue_message": "hello",
     "display_count": raise_script("SystemExit", "bye")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"THIN COVERAGE: {thin}")


if __name__ == "__main__":
    main()