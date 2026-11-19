#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_config_flow.jsonl -- the scripted-seam
FLOW cases for the tui_gateway/methods_config.py port (tui_methods_config_flow).

Deterministic; >=5 cases per ported func. Each case is
{"func", "in": {"args", "io", "state"}, "fail_closed"?}. io maps seam name ->
script (plain value reused, list consumed per call, or {"__script__": true,
"ret"/"raise"}). A missing script is the fail-closed RuntimeError probe.

Fixture domain (shared with the port header): ASCII strings; absolute POSIX
paths without trailing slash; ints only except wait_seconds (4.0/12.0/0.5) and
int()-coercion inputs; unpack seams script 2-lists or null/int (never str);
record/session scripts are dicts or null; list-valued seam RESULTS are wrapped
one level deeper (bare array == per-call queue).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_config_flow.jsonl")

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


POLICY = {"enabled": True, "roots": ["/r"], "exclude_paths": []}
TREE = {"projects": [{"id": "p1", "name": "P1"}, {"id": "p2"}],
        "scoped_session_ids": ["s1", "s2"]}

# -- _display_raw --
add("_display_raw", {}, {"load_cfg": {"display": {"skin": "dark"}}})
add("_display_raw", {}, {"load_cfg": {"display": {}}})
add("_display_raw", {}, {"load_cfg": {}})
add("_display_raw", {}, {"load_cfg": {"display": None}})
add("_display_raw", {}, {"load_cfg": None})
add("_display_raw", {}, {"load_cfg": rs("ValueError", "cfg")})
add("_display_raw", {}, {"load_cfg": rs("SystemExit", "bye")})
add("_display_raw", {}, {}, fail_closed=True)

# -- _display_word --
DW = {"key": "thinking_mode", "default": "collapsed",
      "allowed": ["collapsed", "truncated", "full"]}
add("_display_word", DW, {"load_cfg": {"display": {"thinking_mode": " FULL "}}})
add("_display_word", DW, {"load_cfg": {"display": {"thinking_mode": "bogus"}}})
add("_display_word", DW, {"load_cfg": {"display": {}}})
add("_display_word", DW, {"load_cfg": {"display": {"thinking_mode": None}}})
add("_display_word", DW, {"load_cfg": {"display": {"thinking_mode": 5}}})
add("_display_word", DW, {"load_cfg": rs("OSError", "disk")})
add("_display_word", DW, {}, fail_closed=True)

# -- _safe_client_label --
add("_safe_client_label", {"label": "../../etc pass"})
add("_safe_client_label", {"label": "a..b...c"})
add("_safe_client_label", {"label": "  .hello world  "})
add("_safe_client_label", {"label": "x" * 100})
add("_safe_client_label", {"label": 'bad/\\:*?"<>|chars'})
add("_safe_client_label", {"label": ""})
add("_safe_client_label", {"label": "..."})
add("_safe_client_label", {"label": "a b (c)_d-e.f"})

# -- _redact_client_text --
add("_redact_client_text", {"text": "short", "cap": 10},
    {"redact_debug_support_text": "REDACTED"})
add("_redact_client_text", {"text": "aaaaaaaa\nbbbbcccc", "cap": 4},
    {"redact_debug_support_text": "R1"})
add("_redact_client_text", {"text": "aa\nbbbbbbbbbbbb", "cap": 4},
    {"redact_debug_support_text": "R2"})
add("_redact_client_text", {"text": "a" * 13 + "\n", "cap": 4},
    {"redact_debug_support_text": "R3"})
add("_redact_client_text", {"text": "a" * 20, "cap": 4},
    {"redact_debug_support_text": "R4"})
add("_redact_client_text", {"text": "t", "cap": 4},
    {"redact_debug_support_text": rs("ValueError", "redact")})
add("_redact_client_text", {"text": None, "cap": 4}, {})
add("_redact_client_text", {"text": "t", "cap": 4}, {}, fail_closed=True)

# -- _cfg_get_provider --
add("_cfg_get_provider", {"params": {}},
    {"resolve_model": "nous/hermes", "normalize_provider": "Nous",
     "list_available_providers": [["nous", "openai"]]})
add("_cfg_get_provider", {"params": {}},
    {"resolve_model": "bare", "list_available_providers": [[]]})
add("_cfg_get_provider", {"params": {}},
    {"resolve_model": "", "list_available_providers": [[]]})
add("_cfg_get_provider", {"params": {}}, {"resolve_model": rs("ValueError", "model")})
add("_cfg_get_provider", {"params": {}},
    {"resolve_model": "p/m", "normalize_provider": rs("KeyError", "p")})
add("_cfg_get_provider", {"params": {}}, {"resolve_model": 5})
add("_cfg_get_provider", {"params": {}}, {"resolve_model": rs("SystemExit", "bye")})
add("_cfg_get_provider", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_project --
add("_cfg_get_project", {"params": {"cwd": " /x "}},
    {"completion_cwd": "/x", "git_branch": "main"})
add("_cfg_get_project", {"params": {}},
    {"load_cfg": {"terminal": {"cwd": "/t"}}, "completion_cwd": "/t", "git_branch": "dev"})
add("_cfg_get_project", {"params": {"cwd": ""}},
    {"load_cfg": {"terminal": None}, "completion_cwd": "", "git_branch": None})
add("_cfg_get_project", {"params": {}},
    {"load_cfg": {}, "completion_cwd": "", "git_branch": ""})
add("_cfg_get_project", {"params": {}}, {"load_cfg": None})
add("_cfg_get_project", {"params": {"cwd": None, "profile": "p"}},
    {"load_cfg": {"terminal": {"cwd": "/t"}}, "completion_cwd": "/t", "git_branch": "main"})
add("_cfg_get_project", {"params": {"cwd": "/x"}},
    {"completion_cwd": rs("OSError", "cwd")})
add("_cfg_get_project", {"params": {"cwd": 5}},
    {"completion_cwd": "5", "git_branch": "main"})
add("_cfg_get_project", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_personality --
add("_cfg_get_personality", {"params": {}},
    {"load_cfg": {}, "active_personality_name": "zen"})
add("_cfg_get_personality", {"params": {}},
    {"load_cfg": {}, "active_personality_name": None})
add("_cfg_get_personality", {"params": {}},
    {"load_cfg": {}, "active_personality_name": ""})
add("_cfg_get_personality", {"params": {}}, {"load_cfg": rs("ValueError", "cfg")})
add("_cfg_get_personality", {"params": {}},
    {"load_cfg": {}, "active_personality_name": rs("SystemExit", "bye")})
add("_cfg_get_personality", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_reasoning --
add("_cfg_get_reasoning", {"params": {"session_id": "s"}},
    {"load_cfg": {"display": {}},
     "sessions_get": {"create_reasoning_override": {"enabled": True, "effort": "high"}}})
add("_cfg_get_reasoning", {"params": {"session_id": "s"}},
    {"load_cfg": {}, "sessions_get": {"create_reasoning_override": {"enabled": False}}})
add("_cfg_get_reasoning", {"params": {"session_id": "s"}},
    {"load_cfg": {}, "sessions_get": {"create_reasoning_override": {"enabled": 0}}})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"agent": {"reasoning_effort": "low"}}, "sessions_get": None})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"agent": {"reasoning_effort": False}}, "sessions_get": None})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"agent": {"reasoning_effort": {"enabled": False, "effort": "high"}}},
     "sessions_get": None, "parse_reasoning_effort": {"enabled": False, "effort": "high"}})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"agent": {"reasoning_effort": {"enabled": True}}},
     "sessions_get": None, "parse_reasoning_effort": {"enabled": True, "effort": "low"}})
add("_cfg_get_reasoning", {"params": {"session_id": "s"}},
    {"load_cfg": {}, "sessions_get": {"agent": {"x": 1}},
     "session_runtime_view": {"reasoning_config": {"effort": "high"}}})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"display": {"show_reasoning": False}}, "sessions_get": None})
add("_cfg_get_reasoning", {"params": {}},
    {"load_cfg": {"display": {"show_reasoning": None}}, "sessions_get": None})
add("_cfg_get_reasoning", {"params": {}}, {"load_cfg": {}, "sessions_get": rs("OSError", "db")})
add("_cfg_get_reasoning", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_fast --
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"agent": {"service_tier": "priority"}}})
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"agent": {"service_tier": "ultrafast"}}})
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"agent": {}}, "load_service_tier": "priority"})
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"create_service_tier_override": "ultrafast"}})
add("_cfg_get_fast", {"params": {}}, {"sessions_get": None, "load_service_tier": None})
add("_cfg_get_fast", {"params": {}}, {"sessions_get": None, "load_service_tier": "bogus"})
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"create_service_tier_override": {}}})
add("_cfg_get_fast", {"params": {"session_id": "s"}},
    {"sessions_get": {"agent": "str"}, "load_service_tier": "priority"})
add("_cfg_get_fast", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_thinking_mode --
add("_cfg_get_thinking_mode", {"params": {}},
    {"load_cfg": {"display": {"thinking_mode": "truncated"}}})
add("_cfg_get_thinking_mode", {"params": {}},
    {"load_cfg": [{"display": {}}, {"display": {"details_mode": "expanded"}}]})
add("_cfg_get_thinking_mode", {"params": {}},
    {"load_cfg": [{"display": {}}, {"display": {"details_mode": "hidden"}}]})
add("_cfg_get_thinking_mode", {"params": {}},
    {"load_cfg": {"display": {"thinking_mode": "bogus", "details_mode": "expanded"}}})
add("_cfg_get_thinking_mode", {"params": {}}, {"load_cfg": rs("ValueError", "cfg")})
add("_cfg_get_thinking_mode", {"params": {}}, {}, fail_closed=True)

# -- _cfg_get_mtime --
MH = {"hermes_home": "/h/p1", "params": {}}
add("_cfg_get_mtime", MH,
    {"config_version_or_none": 123, "version_token": "v123", "compute_mcp_rev": "rev1"})
add("_cfg_get_mtime", MH,
    {"config_version_or_none": None, "version_token": None, "compute_mcp_rev": "r"})
add("_cfg_get_mtime", MH, {"config_version_or_none": rs("ValueError", "ver")})
add("_cfg_get_mtime", MH, {"config_version_or_none": 1, "version_token": rs("OSError", "tok")})
add("_cfg_get_mtime", MH, {"config_version_or_none": 1, "version_token": rs("SystemExit", "bye")})
add("_cfg_get_mtime", MH,
    {"config_version_or_none": 1, "version_token": "v", "compute_mcp_rev": rs("RuntimeError", "mcp")})
add("_cfg_get_mtime", MH, {}, fail_closed=True)

# -- _config_getter (the lambda table, one dispatch per key) --
add("_config_getter", {"key": "profile", "hermes_home": "/h/p1", "params": {}},
    {"display_hermes_home": "H"})
add("_config_getter", {"key": "full", "params": {}}, {"load_cfg": {"a": 1}})
add("_config_getter", {"key": "prompt", "params": {}}, {"load_cfg": {"custom_prompt": "hi"}})
add("_config_getter", {"key": "prompt", "params": {}}, {"load_cfg": {}})
add("_config_getter", {"key": "skin", "params": {}}, {"load_cfg": {"display": {"skin": "dark"}}})
add("_config_getter", {"key": "skin", "params": {}}, {"load_cfg": {}})
add("_config_getter", {"key": "indicator", "params": {}},
    {"load_cfg": {"display": {"tui_status_indicator": "ascii"}}})
add("_config_getter", {"key": "indicator", "params": {}},
    {"load_cfg": {"display": {"tui_status_indicator": "garbage"}}})
add("_config_getter", {"key": "daybreak", "params": {}}, {"load_cfg": {"agent": {"daybreak": True}}})
add("_config_getter", {"key": "daybreak", "params": {}}, {"load_cfg": {"agent": {"daybreak": 1}}})
add("_config_getter", {"key": "busy", "params": {}}, {"load_busy_input_mode": "queue"})
add("_config_getter", {"key": "approval_mode", "params": {}}, {"load_approval_mode": "auto"})
add("_config_getter", {"key": "approvals.mode", "params": {}}, {"load_approval_mode": "manual"})
add("_config_getter", {"key": "details_mode", "params": {}},
    {"load_cfg": {"display": {"details_mode": "expanded"}}})
add("_config_getter", {"key": "density", "params": {}}, {"load_cfg": {"display": {"tui_compact": True}}})
add("_config_getter", {"key": "density", "params": {}}, {"load_cfg": {}})
add("_config_getter", {"key": "theme", "params": {}}, {"load_cfg": {"display": {"tui_theme": "dark"}}})
add("_config_getter", {"key": "statusbar", "params": {}},
    {"display_cfg": {"tui_statusbar": "bottom"}, "coerce_statusbar": "bottom"})
add("_config_getter", {"key": "statusbar", "params": {}},
    {"display_cfg": {}, "coerce_statusbar": "top"})
add("_config_getter", {"key": "focus", "params": {}},
    {"display_cfg": {"focus_view": True}, "load_tool_progress_mode": "compact"})
add("_config_getter", {"key": "mouse", "params": {}},
    {"load_cfg": {"display": {"mouse": True}}, "display_mouse_tracking": "on"})
add("_config_getter", {"key": "mtime", "hermes_home": "/h/p1", "params": {}},
    {"config_version_or_none": 1, "version_token": "v", "compute_mcp_rev": "r"})

# -- _h_config_get --
add("_h_config_get", {"rid": "r1", "params": {"key": "busy"}},
    {"load_busy_input_mode": "queue", "ok": ok("r1", {"value": "queue"})})
add("_h_config_get", {"rid": "r1", "params": {"key": "nope"}},
    {"err": err("r1", 4002, "unknown config key: nope")})
add("_h_config_get", {"rid": "r1", "params": {"key": "provider"}},
    {"resolve_model": rs("ValueError", "model"), "err": err("r1", 5013, "model")})
add("_h_config_get", {"rid": "r1", "params": {"key": "approval_mode"}},
    {"load_approval_mode": rs("OSError", "ap"), "err": err("r1", 5001, "ap")})
add("_h_config_get", {"rid": "r1", "params": {"key": "busy"}},
    {"load_busy_input_mode": rs("RuntimeError", "busy")})
add("_h_config_get", {"rid": "r1", "params": {"key": "provider"}},
    {"resolve_model": "m", "list_available_providers": [[]],
     "ok": rs("ValueError", "okfail"), "err": err("r1", 5013, "okfail")})
add("_h_config_get", {"rid": "r1", "params": {"key": None}},
    {"err": err("r1", 4002, "unknown config key: None")})
add("_h_config_get", {"rid": "r1", "params": {"key": 5}},
    {"err": err("r1", 4002, "unknown config key: 5")})
add("_h_config_get", {"rid": "r1", "params": {"key": "busy"}},
    {"load_busy_input_mode": rs("SystemExit", "bye")})
add("_h_config_get", {"rid": "r1", "params": {"key": "busy"}}, {}, fail_closed=True)

# -- _reconcile_repo_discovery --
RA = {"conn": "c1", "policy": POLICY, "policy_key": "k1"}
add("_reconcile_repo_discovery", RA,
    {"repo_discovery_policy_is_default": True, "reconcile_discovered_repos_policy": None})
add("_reconcile_repo_discovery", RA,
    {"repo_discovery_policy_is_default": False, "reconcile_discovered_repos_policy": None})
add("_reconcile_repo_discovery", RA, {"repo_discovery_policy_is_default": rs("ValueError", "p")})
add("_reconcile_repo_discovery", RA,
    {"repo_discovery_policy_is_default": True, "reconcile_discovered_repos_policy": rs("OSError", "db")})
add("_reconcile_repo_discovery", RA,
    {"repo_discovery_policy_is_default": True, "reconcile_discovered_repos_policy": rs("SystemExit", "bye")})
add("_reconcile_repo_discovery", RA, {}, fail_closed=True)

# -- _stamped_project_tree --
SA = {"db": "db1", "params": {"profile": "p"}, "preview_limit": 3, "hydrate": False,
      "session_limit": 2000, "include_discovered": True}
add("_stamped_project_tree", SA,
    {"build_project_tree": [[TREE, "p1"]], "response_profile_name": "launch", "stamp_profile": None})
add("_stamped_project_tree", SA, {"build_project_tree": None})
add("_stamped_project_tree", SA, {"build_project_tree": [[TREE]]})
add("_stamped_project_tree", SA,
    {"build_project_tree": [[{"scoped_session_ids": []}, "p1"]]})
add("_stamped_project_tree", SA,
    {"build_project_tree": [[TREE, "p1"]], "response_profile_name": rs("ValueError", "name")})
add("_stamped_project_tree", SA,
    {"build_project_tree": [[TREE, "p1"]], "response_profile_name": "launch",
     "stamp_profile": rs("OSError", "stamp")})
add("_stamped_project_tree", SA, {}, fail_closed=True)

# -- _h_projects_discover_repos --
DR_OK = ok("r1", {"repos": ["r1"], "discovery_policy": POLICY})
add("_h_projects_discover_repos", {"rid": "r1", "params": {"scan": True}},
    {"profile_db": "db1", "repo_discovery_policy": POLICY, "connect_closing": "c1",
     "repo_discovery_policy_key": "k1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None, "scan_discovered_repos_remote": True,
     "discover_repos_payload": [["r1"]], "ok": DR_OK})
add("_h_projects_discover_repos", {"rid": "r1", "params": {}},
    {"profile_db": "db1", "repo_discovery_policy": POLICY, "connect_closing": "c1",
     "repo_discovery_policy_key": "k1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None,
     "discover_repos_payload": [["r1"]], "ok": DR_OK})
add("_h_projects_discover_repos", {"rid": "r1", "params": {"scan": True}},
    {"profile_db": "db1", "repo_discovery_policy": {"enabled": False, "roots": [], "exclude_paths": []},
     "connect_closing": "c1", "repo_discovery_policy_key": "k1",
     "repo_discovery_policy_is_default": False, "reconcile_discovered_repos_policy": None,
     "discover_repos_payload": [[]],
     "ok": ok("r1", {"repos": [], "discovery_policy": {"enabled": False, "roots": [], "exclude_paths": []}})})
add("_h_projects_discover_repos", {"rid": "r1", "params": {"scan": True}},
    {"profile_db": "db1", "repo_discovery_policy": {"roots": []}, "connect_closing": "c1",
     "repo_discovery_policy_key": "k1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None})
add("_h_projects_discover_repos", {"rid": "r1", "params": {}},
    {"profile_db": "db1", "repo_discovery_policy": {"roots": []}, "connect_closing": "c1",
     "repo_discovery_policy_key": "k1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None})
add("_h_projects_discover_repos", {"rid": "r1", "params": {}},
    {"profile_db": rs("OSError", "db")})
add("_h_projects_discover_repos", {"rid": "r1", "params": {}},
    {"profile_db": "db1", "repo_discovery_policy": POLICY, "connect_closing": "c1",
     "repo_discovery_policy_key": "k1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None, "discover_repos_payload": [[]],
     "ok": rs("SystemExit", "bye")})
add("_h_projects_discover_repos", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _h_projects_record_repos --
RR_PAIRS = [["a", None], ["/r", "L"]]
RR_RES = {"repos": ["r1"], "accepted": True, "discovery_policy": POLICY}
add("_h_projects_record_repos",
    {"rid": "r1", "params": {"discovery_policy": POLICY,
                             "repos": ["a", {"root": "/r", "label": "L"}, {"root": ""}, 5, {"noroot": 1}]}},
    {"repo_discovery_policy": [POLICY, POLICY], "repo_discovery_policy_key": ["k1", "k1"],
     "connect_closing": "c1", "repo_discovery_policy_is_default": True,
     "reconcile_discovered_repos_policy": None, "record_discovered_repos": None,
     "profile_db": "db1", "discover_repos_payload": [["r1"]], "ok": ok("r1", RR_RES)})
add("_h_projects_record_repos", {"rid": "r1", "params": {"repos": None}},
    {"repo_discovery_policy": POLICY, "repo_discovery_policy_key": "k1",
     "repo_discovery_policy_is_default": True, "connect_closing": "c1",
     "reconcile_discovered_repos_policy": None, "record_discovered_repos": None,
     "profile_db": "db1", "discover_repos_payload": [["r1"]], "ok": ok("r1", RR_RES)})
add("_h_projects_record_repos", {"rid": "r1", "params": {}},
    {"repo_discovery_policy": {"enabled": False, "roots": [], "exclude_paths": []},
     "repo_discovery_policy_key": "k0", "repo_discovery_policy_is_default": False,
     "connect_closing": "c1", "reconcile_discovered_repos_policy": None,
     "clear_discovered_repos": None, "profile_db": "db1", "discover_repos_payload": [[]],
     "ok": ok("r1", {"repos": [], "accepted": False,
                     "discovery_policy": {"enabled": False, "roots": [], "exclude_paths": []}})})
add("_h_projects_record_repos", {"rid": "r1", "params": {"discovery_policy": {"enabled": True}}},
    {"repo_discovery_policy": [POLICY, {"enabled": True, "roots": [], "exclude_paths": []}],
     "repo_discovery_policy_key": ["k1", "k2"], "connect_closing": "c1",
     "repo_discovery_policy_is_default": True, "reconcile_discovered_repos_policy": None,
     "profile_db": "db1", "discover_repos_payload": [["r1"]],
     "ok": ok("r1", {"repos": ["r1"], "accepted": False, "discovery_policy": POLICY})})
add("_h_projects_record_repos", {"rid": "r1", "params": {}},
    {"repo_discovery_policy": {"roots": []}, "repo_discovery_policy_key": "k1",
     "repo_discovery_policy_is_default": True})
add("_h_projects_record_repos", {"rid": "r1", "params": {}},
    {"repo_discovery_policy": POLICY, "repo_discovery_policy_key": "k1",
     "repo_discovery_policy_is_default": True, "connect_closing": "c1",
     "reconcile_discovered_repos_policy": None, "record_discovered_repos": rs("OSError", "rec")})
add("_h_projects_record_repos", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _h_projects_tree --
PT_RES = {"projects": TREE["projects"], "active_id": "p1",
          "scoped_session_ids": TREE["scoped_session_ids"]}
add("_h_projects_tree", {"rid": "r1", "params": {}},
    {"profile_db": None, "ok": ok("r1", {"projects": [], "active_id": None, "scoped_session_ids": []})})
add("_h_projects_tree", {"rid": "r1", "params": {"profile": "p"}},
    {"profile_db": "db1", "build_project_tree": [[TREE, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None, "ok": ok("r1", PT_RES)})
add("_h_projects_tree", {"rid": "r1", "params": {"preview_limit": "5"}},
    {"profile_db": "db1", "build_project_tree": [[TREE, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None, "ok": ok("r1", PT_RES)})
add("_h_projects_tree", {"rid": "r1", "params": {"preview_limit": "x"}},
    {"profile_db": "db1"})
add("_h_projects_tree", {"rid": "r1", "params": {"preview_limit": 5.7}},
    {"profile_db": "db1", "build_project_tree": [[TREE, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None, "ok": ok("r1", PT_RES)})
add("_h_projects_tree", {"rid": "r1", "params": {}},
    {"profile_db": "db1", "build_project_tree": None})
add("_h_projects_tree", {"rid": "r1", "params": {}},
    {"profile_db": "db1", "build_project_tree": [[{"projects": []}, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None})
add("_h_projects_tree", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _h_projects_project_sessions --
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": ""}},
    {"err": err("r1", 5063, "project_id required")})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1"}},
    {"profile_db": None, "ok": ok("r1", {"project": None})})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1"}},
    {"profile_db": "db1", "build_project_tree": [[TREE, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None,
     "ok": ok("r1", {"project": {"id": "p1", "name": "P1"}})})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p9"}},
    {"profile_db": "db1", "build_project_tree": [[TREE, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None,
     "ok": ok("r1", {"project": None})})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1"}},
    {"profile_db": "db1",
     "build_project_tree": [[{"projects": [{"name": "x"}], "scoped_session_ids": []}, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1"}},
    {"profile_db": "db1",
     "build_project_tree": [[{"projects": "nope", "scoped_session_ids": []}, "p1"]],
     "response_profile_name": "launch", "stamp_profile": None})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1", "session_limit": "x"}},
    {"profile_db": "db1"})
add("_h_projects_project_sessions", {"rid": "r1", "params": {"project_id": "p1"}},
    {}, fail_closed=True)

# -- _readiness_cleared --
RC_KEY = ["status", ""]
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False, "future_exception": None},
    state={"inflight": [[RC_KEY, "f1"]]})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False, "future_exception": None},
    state={"inflight": [[RC_KEY, "f2"]]})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": True}, state={"inflight": [[RC_KEY, "f1"]]})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False,
     "future_exception": {"__raise__": "ValueError", "message": "boom"},
     "logger_debug": None},
    state={"inflight": [[RC_KEY, "f1"]]})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False, "future_exception": rs("OSError", "exc")})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False,
     "future_exception": {"__raise__": "ValueError", "message": "boom"},
     "logger_debug": rs("RuntimeError", "log")})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"},
    {"future_cancelled": False, "future_exception": None})
add("_readiness_cleared", {"key": RC_KEY, "future_id": "f1"}, {}, fail_closed=True)

# -- _readiness_share --
SH_ARGS = {"rid": "r1", "key": ["status", ""], "wait_seconds": 12.0}
add("_readiness_share", SH_ARGS,
    {"pool_submit": {"id": "f1"}, "run_probe": "probe-ret", "add_done_callback": None,
     "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_share", SH_ARGS,
    {"future_result": {"ok": True}, "ok": ok("r1", {"ok": True})},
    state={"inflight": [[["status", ""], "f9"]]})
add("_readiness_share", SH_ARGS,
    {"pool_submit": {"id": "f1"}, "run_probe": None, "add_done_callback": None,
     "future_result": rs("TimeoutError", ""), "logger_warning": None,
     "err": err("r1", 5097, "readiness check still in progress; retrying next tick")})
add("_readiness_share", SH_ARGS,
    {"future_result": rs("TimeoutError", ""), "logger_warning": None,
     "err": err("r1", 5097, "readiness check still in progress; retrying next tick")},
    state={"inflight": [[["status", ""], "f9"]]})
add("_readiness_share", SH_ARGS,
    {"pool_submit": {"id": "f1"}, "run_probe": None, "add_done_callback": None,
     "future_result": rs("ValueError", "probe")})
add("_readiness_share", SH_ARGS,
    {"pool_submit": {"id": "f1"}, "run_probe": None, "add_done_callback": None,
     "future_result": rs("SystemExit", "bye")})
add("_readiness_share", SH_ARGS, {"pool_submit": rs("RuntimeError", "pool")})
add("_readiness_share", SH_ARGS,
    {"pool_submit": None, "run_probe": None})
add("_readiness_share", SH_ARGS, {}, fail_closed=True)

# -- _readiness_check --
CK_ARGS = {"rid": "r1", "params": {}, "probe_key": "status", "wait_seconds": 4.0}
add("_readiness_check", CK_ARGS,
    {"pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "probe": {"ok": True}, "add_done_callback": None,
     "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_check", {"rid": "r1", "params": {"profile": "p1"},
                         "probe_key": "status", "wait_seconds": 4.0},
    {"profile_exists": True, "profile_home": "/h/p1", "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "probe": {"ok": True},
     "add_done_callback": None, "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_check", {"rid": "r1", "params": {"profile": "p1"},
                         "probe_key": "status", "wait_seconds": 4.0},
    {"profile_exists": False,
     "ok": ok("r1", {"ok": False, "profile": "p1",
                     "error": "Profile 'p1' does not exist on this backend."})})
add("_readiness_check", {"rid": "r1", "params": None, "probe_key": "status", "wait_seconds": 4.0},
    {"pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "probe": {"ok": True}, "add_done_callback": None,
     "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_check", {"rid": "r1", "params": {"profile": "  "},
                         "probe_key": "status", "wait_seconds": 4.0},
    {"pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "probe": {"ok": True}, "add_done_callback": None,
     "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_check", CK_ARGS,
    {"pool_submit": {"id": "f1"}, "session_profile_runtime_scope": None,
     "add_done_callback": None, "future_result": {"ok": True}, "ok": ok("r1", {"ok": True})})
add("_readiness_check", {"rid": "r1", "params": {"profile": "p1"},
                         "probe_key": "status", "wait_seconds": 4.0},
    {"profile_exists": rs("OSError", "prof")})
add("_readiness_check", {"rid": "r1", "params": {"profile": "p1"},
                         "probe_key": "status", "wait_seconds": 4.0},
    {"profile_exists": rs("OSError", "prof")})
add("_readiness_check", CK_ARGS, {}, fail_closed=True)

# -- _h_setup_status --
REC = {"provider_configured": True, "free_tier_account": "acct",
       "free_tier_route": "route", "other_providers": [], "inference_provider": "nous"}
ST_PAYLOAD = {"provider_configured": True, "ready": True, "free_tier_account": "acct",
              "free_tier_route": "route", "other_providers": [], "inference_provider": "nous"}
add("_h_setup_status", {"rid": "r1", "params": {}},
    {"wait_for_record": REC, "failure_fields": {}, "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "add_done_callback": None,
     "future_result": ST_PAYLOAD, "ok": ok("r1", ST_PAYLOAD)})
add("_h_setup_status", {"rid": "r1", "params": {}},
    {"wait_for_record": [None, {"free_tier_account": "a"}],
     "has_any_provider_configured": True, "free_tier_route": "r",
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None,
     "future_result": {"provider_configured": True, "ready": True,
                       "free_tier_account": "a", "free_tier_route": "r"},
     "ok": ok("r1", {"provider_configured": True, "ready": True,
                     "free_tier_account": "a", "free_tier_route": "r"})})
add("_h_setup_status", {"rid": "r1", "params": {}},
    {"wait_for_record": [None, None], "has_any_provider_configured": False,
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None, "future_result": {"provider_configured": False},
     "ok": ok("r1", {"provider_configured": False})})
add("_h_setup_status", {"rid": "r1", "params": {"profile": "p1"}},
    {"profile_exists": True, "profile_home": "/h/p1",
     "wait_for_record": {"free_tier_account": "a"},
     "has_any_provider_configured": True, "free_tier_route": "r",
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None,
     "future_result": {"provider_configured": True, "ready": True,
                       "free_tier_account": "a", "free_tier_route": "r", "profile": "p1"},
     "ok": ok("r1", {"provider_configured": True, "ready": True,
                     "free_tier_account": "a", "free_tier_route": "r", "profile": "p1"})})
add("_h_setup_status", {"rid": "r1", "params": {}},
    {"wait_for_record": rs("ValueError", "rec"), "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "add_done_callback": None,
     "future_result": rs("ValueError", "rec"), "err": err("r1", 5016, "rec")})
add("_h_setup_status", {"rid": "r1", "params": {}},
    {"wait_for_record": rs("SystemExit", "bye"), "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "add_done_callback": None,
     "future_result": rs("SystemExit", "bye")})
add("_h_setup_status", {"rid": "r1", "params": {"profile": "p1"}},
    {"profile_exists": False,
     "ok": ok("r1", {"ok": False, "profile": "p1",
                     "error": "Profile 'p1' does not exist on this backend."})})
add("_h_setup_status", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _h_setup_runtime_check --
RT_OK = {"provider": "nous", "source": "env", "base_url": "https://welcome"}
RC_PAYLOAD = {"ok": True, "provider": "nous", "model": "m", "source": "env",
              "free_tier_route": True}
add("_h_setup_runtime_check", {"rid": "r1", "params": {}},
    {"resolve_startup_runtime": [["m", "nous"]],
     "resolve_agent_model_runtime": [["m", RT_OK]],
     "has_any_provider_configured": True, "load_cfg": {"model": {"provider": "nous"}},
     "runtime_has_credentials": True, "route_is_welcome_host": True,
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None, "future_result": RC_PAYLOAD, "ok": ok("r1", RC_PAYLOAD)})
add("_h_setup_runtime_check", {"rid": "r1", "params": {"provider": "openai"}},
    {"resolve_startup_runtime": [["m", "nous"]],
     "resolve_runtime_provider": {"provider": "openai", "source": "env"},
     "has_any_provider_configured": True, "load_cfg": {},
     "runtime_has_credentials": True,
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None,
     "future_result": {"ok": True, "provider": "openai", "model": "m",
                       "source": "env", "free_tier_route": False},
     "ok": ok("r1", {"ok": True, "provider": "openai", "model": "m",
                     "source": "env", "free_tier_route": False})})
add("_h_setup_runtime_check", {"rid": "r1", "params": {}},
    {"resolve_startup_runtime": [["m", None]],
     "resolve_agent_model_runtime": [["m", {"provider": "bedrock", "source": "iam-role"}]],
     "has_any_provider_configured": False, "load_cfg": {},
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None,
     "future_result": {"ok": False, "provider": "bedrock", "model": "m",
                       "source": "iam-role", "error": "No Hermes provider is configured."},
     "ok": ok("r1", {"ok": False, "provider": "bedrock", "model": "m",
                     "source": "iam-role", "error": "No Hermes provider is configured."})})
add("_h_setup_runtime_check", {"rid": "r1", "params": {}},
    {"resolve_startup_runtime": [["m", "nous"]],
     "resolve_agent_model_runtime": [["m", {"provider": "nous", "source": "env"}]],
     "has_any_provider_configured": True, "load_cfg": {},
     "runtime_has_credentials": False, "missing_credentials_suffix": " Set X.",
     "pool_submit": {"id": "f1"}, "session_profile_runtime_scope": {},
     "add_done_callback": None,
     "future_result": {"ok": False, "provider": "nous", "model": "m", "source": "env",
                       "error": "No usable credentials found for nous. Set X."},
     "ok": ok("r1", {"ok": False, "provider": "nous", "model": "m", "source": "env",
                     "error": "No usable credentials found for nous. Set X."})})
add("_h_setup_runtime_check", {"rid": "r1", "params": {}},
    {"resolve_startup_runtime": None, "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "add_done_callback": None,
     "future_result": rs("TypeError", "cannot unpack non-iterable NoneType object"),
     "readiness_failure": {"ok": False, "error": "readiness failure"},
     "ok": ok("r1", {"ok": False, "error": "readiness failure"})})
add("_h_setup_runtime_check", {"rid": "r1", "params": None},
    {"readiness_failure": {"ok": False, "error": "no params"},
     "ok": ok("r1", {"ok": False, "error": "no params"})})
add("_h_setup_runtime_check", {"rid": "r1", "params": {}},
    {"resolve_startup_runtime": rs("SystemExit", "bye"), "pool_submit": {"id": "f1"},
     "session_profile_runtime_scope": {}, "add_done_callback": None,
     "future_result": rs("SystemExit", "bye")})
add("_h_setup_runtime_check", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _h_share_nous --
add("_h_share_nous", {"rid": "r1", "params": {}},
    {"collect_share_bundle": {"logs.txt": "x"}, "build_nous_bundle": "bundle-marker",
     "share_to_nous": {"viewUrl": "u", "id": "i", "expiresAt": "e"},
     "ok": ok("r1", {"ok": True, "view_url": "u", "upload_id": "i", "expires_at": "e"})})
add("_h_share_nous", {"rid": "r1", "params": {"error_context": "  secret  "}},
    {"collect_share_bundle": {}, "redact_debug_support_text": "REDACTED",
     "build_nous_bundle": "bm", "share_to_nous": {"view_url": "u2", "id": "i2"},
     "ok": ok("r1", {"ok": True, "view_url": "u2", "upload_id": "i2", "expires_at": None})})
add("_h_share_nous", {"rid": "r1", "params": {"extra_files": {
        "a..b": "text", "bad/label": "t2", "ok": "  ", "n": 5, "x": "y"}}},
    {"collect_share_bundle": {}, "redact_debug_support_text": ["R1", "R2"],
     "build_nous_bundle": "bm", "share_to_nous": {"id": "i3"},
     "ok": ok("r1", {"ok": True, "view_url": None, "upload_id": "i3", "expires_at": None})})
add("_h_share_nous", {"rid": "r1", "params": {}},
    {"collect_share_bundle": {}, "build_nous_bundle": "bm", "share_to_nous": {},
     "ok": ok("r1", {"ok": False, "error": "upload succeeded but returned no view URL or id"})})
add("_h_share_nous", {"rid": "r1", "params": {}},
    {"collect_share_bundle": rs("ValueError", "boom"),
     "redact_debug_support_text": "REDACTED",
     "ok": ok("r1", {"ok": False, "error": "REDACTED"})})
add("_h_share_nous", {"rid": "r1", "params": {}},
    {"collect_share_bundle": {}, "build_nous_bundle": "bm", "share_to_nous": None,
     "redact_debug_support_text": "R",
     "ok": ok("r1", {"ok": False, "error": "R"})})
add("_h_share_nous", {"rid": "r1", "params": {"log_lines": 5}},
    {"collect_share_bundle": {}, "build_nous_bundle": "bm",
     "share_to_nous": {"id": "i"},
     "ok": ok("r1", {"ok": True, "view_url": None, "upload_id": "i", "expires_at": None})})
add("_h_share_nous", {"rid": "r1", "params": {"log_lines": 50}},
    {"collect_share_bundle": {}, "build_nous_bundle": "bm",
     "share_to_nous": {"id": "i"},
     "ok": ok("r1", {"ok": True, "view_url": None, "upload_id": "i", "expires_at": None})})
add("_h_share_nous", {"rid": "r1", "params": {}},
    {"collect_share_bundle": rs("ValueError", "boom"),
     "redact_debug_support_text": rs("SystemExit", "bye")})
add("_h_share_nous", {"rid": "r1", "params": {}}, {}, fail_closed=True)

# -- _projects_handler --
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {"a": 1}},
    {"method": None, "profile_scoped": None, "projects_fn": {"ok": 1}})
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {}},
    {"method": None, "profile_scoped": None, "projects_fn": rs("ValueError", "boom"),
     "err": err("r1", 5061, "boom")})
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {}},
    {"method": None, "profile_scoped": None, "projects_fn": rs("SystemExit", "bye")})
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {}},
    {"method": rs("RuntimeError", "reg")})
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {}},
    {"method": None, "profile_scoped": None, "projects_fn": rs("ValueError", "boom"),
     "err": rs("OSError", "errfail")})
add("_projects_handler", {"name": "projects.x", "rid": "r1", "params": {}},
    {}, fail_closed=True)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")


if __name__ == "__main__":
    main()
