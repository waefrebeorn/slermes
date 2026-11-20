#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_sessions_flow.jsonl -- the scripted-seam
FLOW cases for the tui_gateway/methods_session.py port
(tui_methods_sessions_flow).

Deterministic; >=5 cases per ported func. Each case is
{"func", "in": {"args", "io", "state"}, "fail_closed"?}. io maps seam name ->
script (plain value reused, list consumed per call, or {"__script__": true,
"ret"/"raise"}). A missing script is the fail-closed RuntimeError probe.

Fixture domain (shared with the port header): ASCII strings; absolute POSIX
paths without trailing slash; ints only (no float literals in compared
positions); row/persistence/writer scripts are dicts or null; seam results
that are LIVE lists are scripted one level deeper (a bare array is a SEQUENCE
of results, one per call); unpack scripts are 2/3-element lists (or the
wrong-length / non-iterable probes); json.loads text stays in the strict
Python JSON grammar; _b64 data is ASCII text.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_sessions_flow.jsonl")

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


OK1 = {"jsonrpc": "2.0", "id": "r1", "result": {"ok": True}}
ERR_ENV = {"jsonrpc": "2.0", "id": "r1", "error": {"code": 5036, "message": "e"}}

# -- _session_arg --
add("_session_arg", {"rid": "r1", "params": {"session_id": "s1"}},
    {"resolve": [["sess1", None]], "session_fn": OK1})
add("_session_arg", {"rid": "r1", "params": {}},
    {"resolve": [["sess1", {"error": "denied"}]]})
add("_session_arg", {"rid": "r1", "params": {}},
    {"resolve": [["s", {}]], "session_fn": "ok"})
add("_session_arg", {"rid": "r1", "params": {}},
    {"resolve": rs("ValueError", "bad resolve")})
add("_session_arg", {"rid": "r1", "params": {}},
    {"resolve": rs("SystemExit", "bye")})
add("_session_arg", {"rid": "r1", "params": {}}, {"resolve": [[1, 2, 3]]})
add("_session_arg", {"rid": "r1", "params": {}}, {"resolve": None})
add("_session_arg", {"rid": "r1", "params": {}}, {}, fail_closed=True)
add("_session_arg", {"rid": "r1", "params": {}},
    {"resolve": [["s", None]], "session_fn": rs("RuntimeError", "fn blew up")})
add("_session_arg", {"rid": "r1", "params": {}}, {"resolve": "ab"})

# -- _session_method --
add("_session_method", {"name": "session.x", "live": False, "rid": "r1", "params": {}},
    {"sess_nowait": [["s1", None]], "session_fn": OK1})
add("_session_method", {"name": "session.x", "live": True, "rid": "r1", "params": {}},
    {"sess": [["s1", None]], "session_fn": OK1})
add("_session_method", {"name": "session.x", "live": True, "rid": "r1", "params": {}},
    {"sess": [["s1", {"error": "busy"}]]})
add("_session_method", {"name": "session.x", "live": False, "rid": "r1", "params": {}},
    {"sess_nowait": rs("ValueError", "nope")})
add("_session_method", {"name": "session.x", "live": True, "rid": "r1", "params": {}},
    {"sess": rs("SystemExit", "bye")})
add("_session_method", {"name": "session.x", "live": False, "rid": "r1", "params": {}},
    {}, fail_closed=True)
add("_session_method", {"name": "session.x", "live": False, "rid": "r1", "params": {}},
    {"sess_nowait": [["only-one"]]})

# -- _with_db --
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"db": 1}, "persistence_for": {"sessions": {}}, "db_fn": OK1})
add("_with_db", {"code": 5007, "session_scoped": True, "rid": "r1", "params": {}},
    {"sess_nowait": [["s1", None]], "session_db": {"db": 2},
     "persistence_for": {"sessions": {}}, "db_fn": OK1})
add("_with_db", {"code": 5007, "session_scoped": True, "rid": "r1", "params": {}},
    {"sess_nowait": [["s1", {"error": "denied"}]]})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"db": 1}, "persistence_for": {"cm_enter_none": True},
     "db_unavailable_error": ERR_ENV})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": None})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"db": 1}, "persistence_for": None})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"db": 1}, "persistence_for": rs("ValueError", "store")})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"db": 1}, "persistence_for": {"sessions": {}},
     "db_fn": rs("SystemExit", "bye")})
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {}, fail_closed=True)
add("_with_db", {"code": 5007, "session_scoped": True, "rid": "r1", "params": {}},
    {}, fail_closed=True)
add("_with_db", {"code": 5006, "session_scoped": False, "rid": "r1", "params": {}},
    {"profile_db": {"cm_enter_none": True}, "persistence_for": {"sessions": {}},
     "db_fn": OK1})

# -- _new_runtime_ids --
add("_new_runtime_ids", {"params": {"source": "web"}},
    {"uuid4_hex": "abcdef1234567890", "str_param": "web",
     "resolve_session_source": "web"})
add("_new_runtime_ids", {"params": {}},
    {"uuid4_hex": "abcdef1234567890", "str_param": "",
     "resolve_session_source": "default"})
add("_new_runtime_ids", {"params": {}}, {"uuid4_hex": rs("RuntimeError", "uuid")})
add("_new_runtime_ids", {"params": {}},
    {"uuid4_hex": "abcdef12", "str_param": rs("ValueError", "sp")})
add("_new_runtime_ids", {"params": {}}, {"uuid4_hex": None})
add("_new_runtime_ids", {"params": {}}, {}, fail_closed=True)
add("_new_runtime_ids", {"params": {}},
    {"uuid4_hex": "abc", "str_param": "tool", "resolve_session_source": "tool"})
add("_new_runtime_ids", {"params": {}},
    {"uuid4_hex": "abcdef12", "str_param": "web",
     "resolve_session_source": rs("SystemExit", "bye")})

# -- _profile_build_scope --
add("_profile_build_scope", {"profile_home": "/h/p"},
    {"session_profile_runtime_scope": {"scope": 1}})
add("_profile_build_scope", {"profile_home": None},
    {"session_profile_runtime_scope": {"scope": 2}})
add("_profile_build_scope", {"profile_home": ""},
    {"session_profile_runtime_scope": None})
add("_profile_build_scope", {"profile_home": 5},
    {"session_profile_runtime_scope": {"scope": 3}})
add("_profile_build_scope", {"profile_home": "/h/p"},
    {"session_profile_runtime_scope": rs("ValueError", "scope")})
add("_profile_build_scope", {"profile_home": "/h/p"}, {}, fail_closed=True)
add("_profile_build_scope", {"profile_home": True},
    {"session_profile_runtime_scope": rs("SystemExit", "bye")})

# -- _make_agent_in_context --
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {"set_session_context": "tok", "make_agent": {"agent": 1},
     "clear_session_context": None})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1",
                               "kwargs": {"cwd_override": "/w", "model": "m"}},
    {"set_session_context": "tok", "make_agent": {"agent": 1},
     "clear_session_context": None})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {"set_session_context": "tok", "make_agent": rs("ValueError", "build"),
     "clear_session_context": None})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {"set_session_context": "tok", "make_agent": rs("SystemExit", "bye"),
     "clear_session_context": None})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {"set_session_context": "tok", "make_agent": {"agent": 1},
     "clear_session_context": rs("RuntimeError", "clear")})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {"set_session_context": rs("ValueError", "ctx")})
add("_make_agent_in_context", {"sid": "rt1", "key": "k1", "kwargs": {}},
    {}, fail_closed=True)

# -- _cwd_info --
add("_cwd_info", {"session": {"agent": {"m": 1}}, "cwd": "/w"},
    {"session_info": {"cwd": "/w", "lazy": False}})
add("_cwd_info", {"session": {}, "cwd": "/w"},
    {"git_branch": "main", "project_info_for_cwd": {"name": "p"}})
add("_cwd_info", {"session": {}, "cwd": "/w", "branch": "dev"},
    {"project_info_for_cwd": None})
add("_cwd_info", {"session": {"agent": None}, "cwd": "/w"},
    {"git_branch": None, "project_info_for_cwd": {"name": "p"}})
add("_cwd_info", {"session": {}, "cwd": "/w"},
    {"git_branch": rs("OSError", "git")})
add("_cwd_info", {"session": None, "cwd": "/w"}, {})
add("_cwd_info", {"session": {}, "cwd": "/w"}, {}, fail_closed=True)
add("_cwd_info", {"session": {}, "cwd": "/w"},
    {"git_branch": "main", "project_info_for_cwd": rs("SystemExit", "bye")})

# -- _session_row_summary --
add("_session_row_summary", {"row": {"id": "s1"}})
add("_session_row_summary",
    {"row": {"id": "s1", "title": "T", "started_at": 10, "source": "web",
             "_lineage_root_id": "root1"},
     "tip_row": {"preview": "pv", "message_count": 4}, "resolved_id": "tip1",
     "persistence": {"sessions": {}}},
    {"display_count_field": {"live_message_count": 5}})
add("_session_row_summary", {"row": {"id": "s1", "title": "T"}, "tip_row": {}})
add("_session_row_summary",
    {"row": {"id": "s1"}, "resolved_id": None, "persistence": {"sessions": {}}},
    {"display_count_field": {"live_message_count": 0}})
add("_session_row_summary", {"row": None})
add("_session_row_summary", {"row": {"title": "no id"}})
add("_session_row_summary",
    {"row": {"id": "s1"}, "persistence": {"sessions": {}}},
    {"display_count_field": rs("RuntimeError", "count")})
add("_session_row_summary",
    {"row": {"id": "s1", "title": "", "started_at": 0, "source": ""},
     "tip_row": {"preview": "", "message_count": 0}})
add("_session_row_summary", {"row": {"id": "s1"}, "persistence": None})

# -- _row_of --
add("_row_of", {"record": None})
add("_row_of", {"record": {"rec": 1}}, {"as_session_row": {"id": "s"}})
add("_row_of", {"record": {"rec": 1}}, {"as_session_row": None})
add("_row_of", {"record": "str"})
add("_row_of", {"record": {"rec": 1}}, {"as_session_row": rs("ValueError", "row")})
add("_row_of", {"record": {}}, {}, fail_closed=True)
add("_row_of", {"record": {"rec": 1}}, {"as_session_row": rs("SystemExit", "bye")})

# -- _listing_rows --
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": [[{"e": 1}, {"e": 2}]],
     "as_list_row": [{"id": "a", "source": "web"}, {"id": "b", "source": "kanban"}],
     "denied_source": [False, True]})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": [[]]})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": None})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": [["x"]]})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": [[{"e": 1}]], "as_list_row": {"id": "a"},
     "denied_source": rs("RuntimeError", "deny")})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 5,
                      "kwargs": {"include_hidden": True, "include_subagents": False}},
    {"list_sessions": [[]]})
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {}, fail_closed=True)
add("_listing_rows", {"persistence": {"sessions": {}}, "limit": 10},
    {"list_sessions": [[{"e": 1}]], "as_list_row": rs("SystemExit", "bye")})

# -- _snapshot_sessions --
add("_snapshot_sessions", {"rid": "r1"},
    {"sessions_items": [[["s1", {"a": 1}]]]})
add("_snapshot_sessions", {"rid": "r1"},
    {"sessions_items": rs("RuntimeError", "boom"), "err": ERR_ENV})
add("_snapshot_sessions", {"rid": "r1"}, {"sessions_items": None, "err": ERR_ENV})
add("_snapshot_sessions", {"rid": "r1"},
    {"sessions_items": rs("KeyError", "k"), "err": ERR_ENV})
add("_snapshot_sessions", {"rid": "r1"}, {"sessions_items": rs("SystemExit", "bye")})
add("_snapshot_sessions", {"rid": "r1"},
    {"sessions_items": rs("RuntimeError", "boom"), "err": rs("ValueError", "err")})
add("_snapshot_sessions", {"rid": "r1"}, {}, fail_closed=True)
add("_snapshot_sessions", {"rid": "r1"}, {"sessions_items": [[]]})

# -- _pet_display_cfg --
add("_pet_display_cfg", {}, {"load_config": {"display": {"pet": {"enabled": True}}}})
add("_pet_display_cfg", {}, {"load_config": {"display": "nope"}})
add("_pet_display_cfg", {}, {"load_config": {"display": {"pet": "nope"}}})
add("_pet_display_cfg", {}, {"load_config": {}})
add("_pet_display_cfg", {}, {"load_config": None})
add("_pet_display_cfg", {}, {"load_config": rs("ValueError", "cfg")})
add("_pet_display_cfg", {}, {"load_config": rs("SystemExit", "bye")})
add("_pet_display_cfg", {}, {"load_config": {"display": {"pet": {"scale": 2}}}})

# -- _pet_emit --
add("_pet_emit", {"event": "pet.progress", "payload": {"p": 1}, "what": "gen"},
    {"emit": None})
add("_pet_emit", {"event": "e", "payload": {}, "what": "gen"},
    {"emit": rs("ValueError", "boom"), "logger_debug": None})
add("_pet_emit", {"event": "e", "payload": {}, "what": "gen"},
    {"emit": rs("SystemExit", "bye")})
add("_pet_emit", {"event": "e", "payload": {}, "what": "gen"},
    {"emit": rs("ValueError", "boom"), "logger_debug": rs("RuntimeError", "log")})
add("_pet_emit", {"event": "e", "payload": {}, "what": "hatch"},
    {"emit": rs("KeyError", "k"), "logger_debug": None})
add("_pet_emit", {"event": "e", "payload": {}, "what": "gen"}, {}, fail_closed=True)

# -- _pet_gen_abort --
add("_pet_gen_abort", {"rid": "r1", "token": "t1", "code": 4004, "message": "bad"},
    {"pet_cancel_release": None, "err": ERR_ENV})
add("_pet_gen_abort", {"rid": "r1", "token": "t1", "code": 5031, "message": "x"},
    {"pet_cancel_release": rs("RuntimeError", "release")})
add("_pet_gen_abort", {"rid": "r1", "token": "t1", "code": 5031, "message": "x"},
    {"pet_cancel_release": rs("SystemExit", "bye")})
add("_pet_gen_abort", {"rid": "r1", "token": "t1", "code": 5031, "message": "x"},
    {"pet_cancel_release": None, "err": rs("ValueError", "err")})
add("_pet_gen_abort", {"rid": "r1", "token": "t1", "code": 5031, "message": "x"},
    {}, fail_closed=True)
add("_pet_gen_abort", {"rid": "r2", "token": "t2", "code": 4009, "message": "busy"},
    {"pet_cancel_release": None, "err": OK1})

# -- _pet_method --
add("_pet_method", {"name": "pet.cells", "slug": False, "rid": "r1", "params": {}},
    {"pet_fn": OK1})
add("_pet_method", {"name": "pet.select", "slug": True, "rid": "r1",
                    "params": {"slug": "cat"}},
    {"str_param": "cat", "pet_fn": OK1})
add("_pet_method", {"name": "pet.select", "slug": True, "rid": "r1", "params": {}},
    {"str_param": "", "err": ERR_ENV})
add("_pet_method", {"name": "pet.cells", "slug": False, "rid": "r1", "params": {},
                    "fail_open": {"enabled": False}},
    {"pet_fn": rs("ValueError", "boom"), "logger_debug": None, "ok": OK1})
add("_pet_method", {"name": "pet.thumb", "slug": True, "rid": "r1",
                    "params": {"slug": "cat"},
                    "fail_open": {"__callable__": True}},
    {"str_param": "cat", "pet_fn": rs("ValueError", "boom"),
     "logger_debug": None, "fail_open_fn": {"ok": False}, "ok": OK1})
add("_pet_method", {"name": "pet.cells", "slug": False, "rid": "r1", "params": {}},
    {"pet_fn": rs("ValueError", "boom"), "logger_debug": None, "err": ERR_ENV})
add("_pet_method", {"name": "pet.cells", "slug": False, "rid": "r1", "params": {}},
    {"pet_fn": rs("SystemExit", "bye")})
add("_pet_method", {"name": "pet.select", "slug": True, "rid": "r1", "params": {},
                    "fail_open": {"enabled": False}},
    {"str_param": rs("RuntimeError", "sp"), "logger_debug": None, "ok": OK1})
add("_pet_method", {"name": "pet.cells", "slug": False, "rid": "r1", "params": {}},
    {}, fail_closed=True)
add("_pet_method", {"name": "pet.thumb", "slug": False, "rid": "r1", "params": {},
                    "fail_open": {"__callable__": True}},
    {"pet_fn": rs("ValueError", "boom"), "logger_debug": None,
     "fail_open_fn": rs("RuntimeError", "fo")})
add("_pet_method", {"name": "pet.select", "slug": True, "rid": "r1", "params": {}},
    {"str_param": "  ", "pet_fn": OK1})

# -- _active_pet --
add("_active_pet", {}, {"pet_active_selection": [[True, {"exists": True, "slug": "c"}, 2]]})
add("_active_pet", {}, {"pet_active_selection": [[False, {}, 1]]})
add("_active_pet", {}, {"pet_active_selection": [[True, None, 1]]})
add("_active_pet", {}, {"pet_active_selection": [[True, {"exists": False}, 1]]})
add("_active_pet", {}, {"pet_active_selection": [[True, {"slug": "c"}, 1]]})
add("_active_pet", {}, {"pet_active_selection": [[True, {"exists": True}]]})
add("_active_pet", {}, {"pet_active_selection": rs("RuntimeError", "sel")})
add("_active_pet", {}, {"pet_active_selection": rs("SystemExit", "bye")})
add("_active_pet", {}, {}, fail_closed=True)

# -- _b64 --
add("_b64", {"data": "hello"})
add("_b64", {"data": ""})
add("_b64", {"data": "a"})
add("_b64", {"data": "ab"})
add("_b64", {"data": "abc"})
add("_b64", {"data": "Hello, World!"})
add("_b64", {"data": None})

# -- _pet_config_followup --
add("_pet_config_followup", {"what": "pet.remove", "args": ["cat"]},
    {"followup_fn": None})
add("_pet_config_followup", {"what": "pet.remove", "args": ["cat"]},
    {"followup_fn": rs("ValueError", "x"), "logger_debug": None})
add("_pet_config_followup", {"what": "pet.rename", "args": ["a", "b"]},
    {"followup_fn": rs("SystemExit", "bye")})
add("_pet_config_followup", {"what": "pet.rename", "args": ["a", "b"]},
    {"followup_fn": None})
add("_pet_config_followup", {"what": "pet.remove", "args": []},
    {"followup_fn": rs("ValueError", "x"), "logger_debug": rs("RuntimeError", "log")})
add("_pet_config_followup", {"what": "pet.remove", "args": ["cat"]},
    {}, fail_closed=True)

# -- _pet_pick_provider --
add("_pet_pick_provider", {"params": {"provider": "nous"}, "require_references": False},
    {"str_param": "nous", "resolve_provider": {"provider": "nous"}})
add("_pet_pick_provider", {"params": {}, "require_references": True},
    {"str_param": ""})
add("_pet_pick_provider", {"params": {}, "require_references": False},
    {"str_param": rs("ValueError", "sp")})
add("_pet_pick_provider", {"params": {"provider": "x"}, "require_references": True},
    {"str_param": "x", "resolve_provider": rs("GenerationError", "no backend")})
add("_pet_pick_provider", {"params": {"provider": "nous"}, "require_references": True},
    {"str_param": "nous", "resolve_provider": None})
add("_pet_pick_provider", {"params": {}, "require_references": False},
    {}, fail_closed=True)
add("_pet_pick_provider", {"params": {"provider": "n"}, "require_references": False},
    {"str_param": "n", "resolve_provider": rs("SystemExit", "bye")})

# -- _billing_call --
add("_billing_call", {"rid": "r1"}, {"billing_fn": {"x": 1}, "ok": OK1})
add("_billing_call", {"rid": "r1", "extra": {"e": 1}},
    {"billing_fn": rs("BillingError", "declined"),
     "serialize_billing_error": {"ok": False, "error": "billing", "message": "declined"},
     "ok": OK1})
add("_billing_call", {"rid": "r1", "extra": {"e": 1}},
    {"billing_fn": rs("ValueError", "boom"), "ok": OK1})
add("_billing_call", {"rid": "r1"}, {"billing_fn": rs("SystemExit", "bye")})
add("_billing_call", {"rid": "r1", "extra": None},
    {"billing_fn": rs("RuntimeError", "m"), "ok": OK1})
add("_billing_call", {"rid": "r1"},
    {"billing_fn": rs("KeyError", "k"), "ok": OK1})
add("_billing_call", {"rid": "r1"},
    {"billing_fn": rs("BillingError", "d"),
     "serialize_billing_error": rs("RuntimeError", "ser")})
add("_billing_call", {"rid": "r1"}, {}, fail_closed=True)
add("_billing_call", {"rid": "r1"},
    {"billing_fn": rs("BillingError", "d"),
     "serialize_billing_error": {"ok": False, "code": "x"}, "ok": OK1})

# -- _billing_invalid --
add("_billing_invalid", {"rid": "r1", "message": "bad"}, {"ok": OK1})
add("_billing_invalid", {"rid": "r1", "message": "bad", "error": "invalid_charge_id"},
    {"ok": OK1})
add("_billing_invalid", {"rid": "r1", "message": "bad"}, {"ok": rs("ValueError", "ok")})
add("_billing_invalid", {"rid": "r1", "message": "bad"}, {"ok": rs("SystemExit", "bye")})
add("_billing_invalid", {"rid": "r1", "message": "bad"}, {}, fail_closed=True)
add("_billing_invalid", {"rid": "r1", "message": ""}, {"ok": OK1})

# -- _billing_pending_change --
add("_billing_pending_change", {"result": {"message": "m", "x": 1}})
add("_billing_pending_change", {"result": {}})
add("_billing_pending_change", {"result": None})
add("_billing_pending_change", {"result": "s"})
add("_billing_pending_change", {"result": {"message": None}})
add("_billing_pending_change", {"result": {"message": "m", "nested": {"a": [1, 2]}}})

# -- _billing_route --
add("_billing_route", {"name": "subscription.change", "rid": "r1",
                       "params": {"subscription_type_id": "t1"}},
    {"route_call": {"ok": True, "message": "scheduled"}, "ok": OK1})
add("_billing_route", {"name": "subscription.change", "rid": "r1", "params": {},
                       "invalid": {"__callable__": True},
                       "message": "subscription_type_id or cancel is required"},
    {"invalid_fn": True, "ok": OK1})
add("_billing_route", {"name": "subscription.change", "rid": "r1",
                       "params": {"cancel": True},
                       "invalid": {"__callable__": True}, "message": "req"},
    {"invalid_fn": False, "route_call": {"ok": True}, "ok": OK1})
add("_billing_route", {"name": "subscription.upgrade", "rid": "r1",
                       "params": {"idempotency_key": "k0", "subscription_type_id": "t"},
                       "idempotent": True},
    {"route_call": {"ok": True, "status": "ok"}, "ok": OK1})
add("_billing_route", {"name": "billing.charge", "rid": "r1",
                       "params": {"amount_usd": 5}, "idempotent": True},
    {"new_idempotency_key": "k1", "route_call": {"ok": True, "chargeId": "c"},
     "ok": OK1})
add("_billing_route", {"name": "billing.charge", "rid": "r1", "params": {},
                       "idempotent": True},
    {"new_idempotency_key": "k1", "route_call": rs("BillingError", "declined"),
     "serialize_billing_error": {"ok": False, "error": "billing"}, "ok": OK1})
add("_billing_route", {"name": "billing.auto_reload", "rid": "r1", "params": {}},
    {"route_call": rs("ValueError", "boom"), "ok": OK1})
add("_billing_route", {"name": "subscription.resume", "rid": "r1", "params": {}},
    {"route_call": "not-a-dict", "ok": OK1})
add("_billing_route", {"name": "subscription.change", "rid": "r1", "params": {},
                       "invalid": {"__callable__": True}, "message": "req"},
    {"invalid_fn": rs("RuntimeError", "iv")})
add("_billing_route", {"name": "subscription.resume", "rid": "r1", "params": {}},
    {"route_call": rs("SystemExit", "bye")})
add("_billing_route", {"name": "subscription.resume", "rid": "r1", "params": {}},
    {}, fail_closed=True)
add("_billing_route", {"name": "billing.charge_status", "rid": "r1", "params": {},
                       "invalid": {"__callable__": True},
                       "message": "charge_id is required", "error": "invalid_charge_id"},
    {"invalid_fn": True, "ok": OK1})

# -- _unarchive_recoverable --
add("_unarchive_recoverable",
    {"persistence": {"read_only": False, "sessions": {}}, "session_id": "s1"},
    {"unarchive_recoverable": True})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"},
                     "sessions": {}}, "session_id": "s1"},
    {"open_persistence": {"w": 1}, "writer_unarchive": True, "writer_close": None})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": rs("RuntimeError", "writer"), "logger_warning": None})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": {"w": 1}, "writer_unarchive": rs("ValueError", "ua"),
     "writer_close": None})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": {"w": 1}, "writer_unarchive": True,
     "writer_close": rs("RuntimeError", "close")})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": {"w": 1}, "writer_unarchive": True,
     "writer_close": rs("SystemExit", "bye")})
add("_unarchive_recoverable", {"persistence": None, "session_id": "s1"}, {})
add("_unarchive_recoverable", {"persistence": {"sessions": {}}, "session_id": "s1"}, {})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": None})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True, "scope": {"profile_dir": "/h/p"}},
     "session_id": "s1"},
    {"open_persistence": rs("SystemExit", "bye")})
add("_unarchive_recoverable",
    {"persistence": {"read_only": True}, "session_id": "s1"}, {})
add("_unarchive_recoverable",
    {"persistence": {"read_only": False, "sessions": {}}, "session_id": "s1"},
    {}, fail_closed=True)

# -- _session_list_by_title --
P_FULL = {"sessions": {}, "lineage": {}}
ROW1 = {"id": "s1", "title": "T", "archived": False, "source": "web",
        "started_at": 10, "preview": "pv", "message_count": 2}
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": ROW1, "denied_source": False,
     "lineage_tip": None, "display_count_field": {"live_message_count": 2}, "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1},
     "as_session_row": [ROW1, {"id": "tip1", "preview": "tp", "message_count": 9}],
     "denied_source": False, "lineage_tip": "tip1", "sessions_get": {"rec": 2},
     "display_count_field": {"live_message_count": 9}, "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL,
                               "title_lookup": "Other"},
    {"get_by_title": {"rec": 1},
     "as_session_row": {"id": "s1", "archived": True}, "ok": OK1})
add("_session_list_by_title",
    {"rid": "r1",
     "persistence": {"sessions": {}, "lineage": {}, "read_only": False},
     "title_lookup": "Bot Chat"},
    {"get_by_title": {"rec": 1},
     "as_session_row": [{"id": "s1", "archived": True, "source": "web"},
                        {"id": "s1", "archived": False, "source": "web",
                         "title": "Bot Chat"}],
     "unarchive_recoverable": True, "sessions_get": {"rec": 2},
     "denied_source": False, "lineage_tip": None,
     "display_count_field": {}, "ok": OK1})
add("_session_list_by_title",
    {"rid": "r1",
     "persistence": {"sessions": {}, "read_only": False},
     "title_lookup": "Bot Chat"},
    {"get_by_title": {"rec": 1}, "as_session_row": {"id": "s1", "archived": True},
     "unarchive_recoverable": False, "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": ROW1, "denied_source": True,
     "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": None, "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": rs("RuntimeError", "db")})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": ROW1, "denied_source": False,
     "lineage_tip": rs("ValueError", "tip"),
     "display_count_field": {}, "ok": OK1})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": ROW1, "denied_source": False,
     "lineage_tip": rs("SystemExit", "bye")})
add("_session_list_by_title",
    {"rid": "r1", "persistence": {"sessions": {}, "read_only": False},
     "title_lookup": "Bot Chat"},
    {"get_by_title": {"rec": 1}, "as_session_row": {"id": "s1", "archived": True},
     "unarchive_recoverable": rs("RuntimeError", "ua")})
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {}, fail_closed=True)
add("_session_list_by_title", {"rid": "r1", "persistence": P_FULL, "title_lookup": "T"},
    {"get_by_title": {"rec": 1}, "as_session_row": [ROW1, None],
     "denied_source": False, "lineage_tip": "tip1", "sessions_get": {"rec": 2},
     "display_count_field": {}, "ok": OK1})

# -- _find_live_unpersisted --
add("_find_live_unpersisted", {"needle": "k1", "home": None},
    {"sessions_items": [[["l1", {"session_key": "k1"}]]]})
add("_find_live_unpersisted", {"needle": "T", "home": None},
    {"sessions_items": [[["l1", {"pending_title": "T"}]]]})
add("_find_live_unpersisted", {"needle": "k1", "home": "/h/p"},
    {"sessions_items": [[["l1", {"session_key": "k1", "profile_home": "/h/p"}],
                         ["l2", {"session_key": "k1"}]]]})
add("_find_live_unpersisted", {"needle": "nope", "home": None},
    {"sessions_items": [[["l1", {"session_key": "k1"}]]]})
add("_find_live_unpersisted", {"needle": "k", "home": None},
    {"sessions_items": [[["l1", "str"], ["l2", {"session_key": "k"}]]]})
add("_find_live_unpersisted", {"needle": "k", "home": None},
    {"sessions_items": rs("RuntimeError", "items")})
add("_find_live_unpersisted", {"needle": "k", "home": None},
    {"sessions_items": [[[1, 2, 3]]]})
add("_find_live_unpersisted", {"needle": "k1", "home": 5},
    {"sessions_items": [[["l1", {"session_key": "k1", "profile_home": "5"}]]]})
add("_find_live_unpersisted", {"needle": "k", "home": None}, {}, fail_closed=True)
add("_find_live_unpersisted", {"needle": "5", "home": None},
    {"sessions_items": [[["l1", {"session_key": 5}]]]})
add("_find_live_unpersisted", {"needle": "5", "home": None},
    {"sessions_items": [[["l1", {"pending_title": 5}]]]})

# -- _status_row --
add("_status_row", {"session": {}, "params": {}, "key": ""})
add("_status_row", {"session": {"s": 1}, "params": {}, "key": "k1"},
    {"session_db": {"d": 1}, "persistence_for": {"sessions": {}},
     "sessions_get": {"rec": 1}, "as_session_row": {"id": "s"}})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"cm_enter_none": True}, "profile_db": {"d": 2},
     "persistence_for": {"sessions": {}}, "sessions_get": {"rec": 1},
     "as_session_row": {"id": "s"}})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"cm_enter_none": True}, "profile_db": {"cm_enter_none": True}})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"cm_enter_none": True}, "profile_db": None})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"d": 1}, "persistence_for": rs("ValueError", "store")})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"d": 1}, "persistence_for": {"sessions": {}},
     "sessions_get": rs("RuntimeError", "get")})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"d": 1}, "persistence_for": {"sessions": {}},
     "sessions_get": {"rec": 1}, "as_session_row": {}})
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": None})
add("_status_row", {"session": {}, "params": {}, "key": "k1"}, {}, fail_closed=True)
add("_status_row", {"session": {}, "params": {}, "key": "k1"},
    {"session_db": {"d": 1}, "persistence_for": {"sessions": {}},
     "sessions_get": rs("SystemExit", "bye")})

# -- _try_get_session --
add("_try_get_session", {"db": {"d": 1}, "key": "k1"},
    {"persistence_for": {"sessions": {}}, "sessions_get": {"rec": 1},
     "as_session_row": {"id": "s"}})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"}, {"persistence_for": None})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"},
    {"persistence_for": {"cm_enter_none": True}})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"}, {"persistence_for": {}})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"},
    {"persistence_for": {"sessions": {}}, "sessions_get": {"rec": 1},
     "as_session_row": None})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"},
    {"persistence_for": {"sessions": {}}, "sessions_get": rs("SystemExit", "bye")})
add("_try_get_session", {"db": {"d": 1}, "key": "k1"}, {})
add("_try_get_session", {"db": None, "key": "k1"},
    {"persistence_for": {"sessions": {}}, "sessions_get": {"rec": 1},
     "as_session_row": {"id": "s"}})

# -- _title_read --
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "T"}, state={"session": {}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": ""}, state={"session": {}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "T", "set_title": True},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "", "set_title": False, "sessions_get": {"rec": 1},
     "as_session_row": {"title": " F "}},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "", "set_title": False, "sessions_get": {"rec": 1},
     "as_session_row": {"title": "X"}},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "T", "set_title": False, "sessions_get": {"rec": 1},
     "as_session_row": {"title": "X"}},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": rs("RuntimeError", "gt")},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "", "set_title": rs("ValueError", "st")},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": rs("SystemExit", "bye")}, state={"session": {}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "T"}, state={"session": None})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {"get_title": "", "set_title": False, "sessions_get": {"rec": 1},
     "as_session_row": {"title": 5}},
    state={"session": {"pending_title": "F"}})
add("_title_read", {"persistence": {"sessions": {}}, "key": "k1"},
    {}, state={"session": {}})

# -- _auto_reload --
add("_auto_reload", {"p": {"enabled": True, "threshold": 5, "top_up_amount": 10}},
    {"patch_auto_top_up": None})
add("_auto_reload", {"p": {}}, {"patch_auto_top_up": None})
add("_auto_reload", {"p": None}, {})
add("_auto_reload", {"p": {"enabled": True}},
    {"patch_auto_top_up": rs("ValueError", "patch")})
add("_auto_reload", {"p": {}}, {"patch_auto_top_up": rs("SystemExit", "bye")})
add("_auto_reload", {"p": {}}, {}, fail_closed=True)
add("_auto_reload", {"p": {"enabled": "yes", "threshold": 0}},
    {"patch_auto_top_up": None})

# -- _legacy_spawn_tree_entry --
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77},
     "read_text": "{\"session_id\": \"s9\", \"subagents\": [1, 2], "
                  "\"finished_at\": 5, \"started_at\": 3, \"label\": \"L\"}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "{}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": rs("FileNotFoundError", "gone")})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": rs("ValueError", "bad")})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": rs("OSError", "read")})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "{bad"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "null"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77},
     "read_text": "{\"finished_at\": 0, \"subagents\": \"abc\"}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {}, "read_text": "{}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": rs("SystemExit", "bye")})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {}, fail_closed=True)
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "{\"a\": 01}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "{\"a\": 1,}"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": " \t{\"session_id\": \"s\"}\n"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "{\"a\": 1} x"})
add("_legacy_spawn_tree_entry", {"path": "/h/p/snap.json", "session_dir_name": "d1"},
    {"stat": {"st_mtime": 77}, "read_text": "[1, 2]"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            counts[case["func"]] = counts.get(case["func"], 0) + 1
            f.write(json.dumps(case, ensure_ascii=True) + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, c in counts.items() if c < 5]
    if thin:
        raise SystemExit(f"THIN COVERAGE: {thin}")


if __name__ == "__main__":
    main()