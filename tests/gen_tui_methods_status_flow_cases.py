#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_status_flow.jsonl for the TUI session
live-status / registry FLOW port (src/gateway/tui_methods_status_flow.c).

The task named tui_gateway/methods_status.py (nonexistent); the real defining
module is tui_gateway/session_registry.py. Each case: {"func": <live def name>,
"in": {"args": {...}, "io": {...}, ["state": {...}]}, ["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (USE IT for list-valued
results — a bare list is consumed element-by-element); {"__script__": true,
"raise": {"type", "message"}} raises.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; POSIX paths; integral numbers.
Event/lease/db/persistence doubles use marker dicts ({"__event__": role} etc.);
the _ANY_PROFILE / _detached_ws_transport sentinels are "__ANY_PROFILE__" /
"__DETACHED_WS__". Identity funcs select the registry record via current_sid.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_status_flow.jsonl"
CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False):
    inp = {"args": args or {}, "io": io or {}}
    if state is not None:
        inp["state"] = state
    c = {"func": func, "in": inp}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def S(ret):
    return {"__script__": True, "ret": ret}


def R(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def ev(role):
    return {"__event__": role}


ENV = {"jsonrpc": "2.0", "id": "r1", "error": {"code": 5032, "message": "x"}}
READY = ev("agent_ready")
HREADY = ev("resume_history")

# ── _wait_agent ──
case("_wait_agent", {"session": {}, "rid": "r1"})
case("_wait_agent", {"session": {"agent_ready": READY}, "rid": "r1"},
     {"agent_ready.wait": True})
case("_wait_agent", {"session": {"agent_ready": READY}, "rid": "r1"},
     {"agent_ready.wait": False, "_err": ENV})
case("_wait_agent", {"session": {"agent_error": "boom"}, "rid": "r1"}, {"_err": ENV})
case("_wait_agent", {"session": {"agent_ready": READY}, "rid": "r1"}, {}, fail_closed=True)
case("_wait_agent", {"session": {"agent_ready": READY}, "rid": "r1"},
     {"agent_ready.wait": R("SystemExit", "bye")})
case("_wait_agent", {"session": {"agent_ready": READY, "agent_error": "e"}, "rid": "r1", "timeout": 10},
     {"agent_ready.wait": True, "_err": ENV})

# ── _agent_build_wait_cap ──
case("_agent_build_wait_cap", {}, {"_load_cfg": {"agent": {"build_wait_timeout": 90}}})
case("_agent_build_wait_cap", {}, {"_load_cfg": {}})
case("_agent_build_wait_cap", {}, {"_load_cfg": {"agent": {"build_wait_timeout": "120"}}})
case("_agent_build_wait_cap", {}, {"_load_cfg": {"agent": {"build_wait_timeout": -5}}})
case("_agent_build_wait_cap", {}, {"_load_cfg": {"agent": {"build_wait_timeout": "abc"}}})
case("_agent_build_wait_cap", {}, {"_load_cfg": {"agent": "notadict"}})
case("_agent_build_wait_cap", {}, {"_load_cfg": R("SystemExit", "bye")})
case("_agent_build_wait_cap", {}, {})  # missing _load_cfg -> RuntimeError suppressed -> 600

# ── _bind_build_profile_scopes ──
case("_bind_build_profile_scopes", {"profile_home": "/h/p1"},
     {"_profile_runtime_scope_tokens": "tok"})
case("_bind_build_profile_scopes", {"profile_home": "/h/p1"},
     {"_profile_runtime_scope_tokens": R("ValueError", "x"),
      "scope_for_profile": "sc", "bind_state": "tok2"})
case("_bind_build_profile_scopes", {"profile_home": None},
     {"_profile_runtime_scope_tokens": R("ValueError", "x")})
case("_bind_build_profile_scopes", {"profile_home": "/h/p1"},
     {"_profile_runtime_scope_tokens": R("ValueError", "x"),
      "scope_for_profile": R("ValueError", "y")})
case("_bind_build_profile_scopes", {"profile_home": "/h/p1"},
     {"_profile_runtime_scope_tokens": R("ValueError", "x"),
      "scope_for_profile": "sc", "bind_state": R("ValueError", "z")})
case("_bind_build_profile_scopes", {"profile_home": "/h/p1"},
     {"_profile_runtime_scope_tokens": R("SystemExit", "bye")})
case("_bind_build_profile_scopes", {"profile_home": None}, {}, fail_closed=True)

# ── _release_build_profile_scopes ──
case("_release_build_profile_scopes", {"scopes": "tok"},
     {"_release_profile_runtime_scope_tokens": None})
case("_release_build_profile_scopes", {"scopes": "tok"},
     {"_release_profile_runtime_scope_tokens": R("ValueError", "x")})
case("_release_build_profile_scopes", {"scopes": "tok"},
     {"_release_profile_runtime_scope_tokens": R("SystemExit", "bye")})
case("_release_build_profile_scopes", {"scopes": "tok"}, {}, fail_closed=True)
case("_release_build_profile_scopes", {"scopes": None},
     {"_release_profile_runtime_scope_tokens": None})
case("_release_build_profile_scopes", {"scopes": {"a": 1}},
     {"_release_profile_runtime_scope_tokens": None})

# ── _deferred_build_agent_kwargs ──
KWIO = {"_context_cwd_is_launch_artifact": False, "_session_source": "tui", "_session_cwd": "/w"}
case("_deferred_build_agent_kwargs",
     {"current": {"resume_session_id": "rs", "resume_runtime_overrides": {"model_override": "X", "provider": "p"}},
      "session_db": "db"},
     {**KWIO, "_overrides_have_routable_provider": True})
case("_deferred_build_agent_kwargs",
     {"current": {"resume_runtime_overrides": {"provider": "p"}, "model_override": "M",
                  "create_reasoning_override": {"effort": "high"}, "create_service_tier_override": "flex"},
      "session_db": None},
     {**KWIO, "_overrides_have_routable_provider": False})
case("_deferred_build_agent_kwargs", {"current": {"model_override": "M"}, "session_db": "db"}, KWIO)
case("_deferred_build_agent_kwargs",
     {"current": {"resume_session_id": "rs", "resume_runtime_overrides": {}}, "session_db": "db"}, KWIO)
case("_deferred_build_agent_kwargs", {"current": {}, "session_db": "db"}, {}, fail_closed=True)
case("_deferred_build_agent_kwargs", {"current": {}, "session_db": "db"},
     {"_context_cwd_is_launch_artifact": False, "_session_source": R("SystemExit", "bye")})
case("_deferred_build_agent_kwargs",
     {"current": {"resume_runtime_overrides": ["not", "dict"], "create_service_tier_override": "flex"},
      "session_db": "db"}, KWIO)

# ── _await_resume_history ──
case("_await_resume_history", {"sid": "s1", "current": {}})
case("_await_resume_history",
     {"sid": "s1", "current_sid": "s1"},
     {"resume_history.wait": True}, {"sessions": {"s1": {"resume_history_ready": HREADY}}})
case("_await_resume_history",
     {"sid": "s1", "current": {"resume_history_ready": HREADY}},
     {"resume_history.wait": True}, {"sessions": {"s1": {"session_key": "k"}}})
case("_await_resume_history",
     {"sid": "s1", "current": {"resume_history_ready": HREADY}},
     {"resume_history.wait": False})
case("_await_resume_history",
     {"sid": "s1", "current": {"resume_history_ready": HREADY, "resume_history_error": "bad"}},
     {"resume_history.wait": True})
case("_await_resume_history",
     {"sid": "s1", "current": {"resume_history_ready": HREADY}}, {}, fail_closed=True)
case("_await_resume_history",
     {"sid": "s1", "current": {"resume_history_ready": HREADY}},
     {"resume_history.wait": R("SystemExit", "bye")})

# ── _attach_built_agent ──
ATT_IO = {"_register_session_cwd": None, "_session_todo_state": None, "_config_model_target": "cfg"}
case("_attach_built_agent",
     {"sid": "s1", "current_sid": "s1", "agent": {"model": "m"}},
     ATT_IO, {"sessions": {"s1": {"pending_title": " T ", "session_key": "k"}}})
case("_attach_built_agent",
     {"sid": "s1", "current_sid": "s1", "agent": {"model": "m"}},
     ATT_IO, {"sessions": {"s1": {"session_key": "k"}}})
case("_attach_built_agent",
     {"sid": "s1", "current": {"pending_title": "T"}, "agent": {"model": "m"}},
     {}, {"sessions": {"s1": {"session_key": "k"}}})
case("_attach_built_agent",
     {"sid": "s1", "current_sid": "s1", "agent": {"model": "m"}},
     {}, {"sessions": {"s1": {"pending_title": "T"}}}, fail_closed=True)
case("_attach_built_agent",
     {"sid": "s1", "current_sid": "s1", "agent": {"model": "m"}},
     {"_register_session_cwd": None, "_session_todo_state": None,
      "_config_model_target": R("SystemExit", "bye")},
     {"sessions": {"s1": {"session_key": "k"}}})
case("_attach_built_agent",
     {"sid": "s1", "current_sid": "s1", "agent": {"model": "m"}},
     ATT_IO, {"sessions": {"s1": {"pending_title": "   ", "session_key": "k"}}})

# ── _start_session_services ──
SS_IO = {"_start_notification_poller": "stop2", "_session_source": "tui", "_notify_session_boundary": None}
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {"session_key": "k1"}},
     {"old_stop.set": None, **SS_IO},
     {"sessions": {"s1": {"_notif_stop": ev("old_stop"), "session_key": "k1"}}})
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {"session_key": "k1"}},
     SS_IO, {"sessions": {"s1": {"session_key": "k1"}}})
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {"session_key": "k1"}},
     {"_session_source": "tui", "_notify_session_boundary": None}, {"sessions": {}})
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {}},
     {"_session_source": "tui", "_notify_session_boundary": None},
     {"sessions": {"s1": {"session_key": "k1"}}}, fail_closed=True)
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {}},
     {"_start_notification_poller": "stop2", "_session_source": R("SystemExit", "bye")},
     {"sessions": {"s1": {"session_key": "k1"}}})
case("_start_session_services",
     {"sid": "s1", "key": "k1", "current": {"source": "cli"}},
     {"old_stop.set": None, **SS_IO}, {"sessions": {"s1": {"_notif_stop": ev("old_stop")}}})

# ── _finish_agent_build ──
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current_sid": "s1", "notify_registered": True,
      "scopes": "tok", "session_db": {"__db__": True}, "persistence": {"__persistence__": True}},
     {"_release_build_profile_scopes": None, "_transfer_db_to_agent": False,
      "db.close": None, "_release_persistence": None},
     {"sessions": {"s1": {"agent": {"model": "m"}, "session_key": "k1"}}})
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current": {"session_key": "k1"}, "notify_registered": True,
      "scopes": None, "session_db": {"__db__": True}},
     {"unregister_gateway_notify": None, "_transfer_db_to_agent": True},
     {"sessions": {"s1": {"session_key": "k1"}}})
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current_sid": "s1", "notify_registered": False,
      "scopes": None, "session_db": {"__db__": True}},
     {"_transfer_db_to_agent": True},
     {"sessions": {"s1": {"agent": {"model": "m"}}}})
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current_sid": "s1", "notify_registered": False,
      "scopes": None, "session_db": None},
     {}, {"sessions": {"s1": {"session_key": "k1"}}})
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current_sid": "s1", "scopes": "tok", "session_db": None},
     {}, {"sessions": {"s1": {}}}, fail_closed=True)
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current_sid": "s1", "notify_registered": False,
      "scopes": None, "session_db": {"__db__": True}},
     {"_transfer_db_to_agent": False, "db.close": R("SystemExit", "bye")},
     {"sessions": {"s1": {"agent": {"model": "m"}}}})
case("_finish_agent_build",
     {"sid": "s1", "key": "k1", "current": {"session_key": "k1"}, "notify_registered": True,
      "scopes": None, "session_db": {"__db__": True}},
     {"unregister_gateway_notify": R("ValueError", "x"), "_transfer_db_to_agent": True},
     {"sessions": {"s1": {"session_key": "k1"}}})

# ── _sess_nowait ──
case("_sess_nowait", {"params": {"session_id": "s1"}, "rid": "r1"},
     {}, {"sessions": {"s1": {"session_key": "k"}}})
case("_sess_nowait", {"params": {"session_id": "nope"}, "rid": 7},
     {"_current_rpc_method.get": "session.status", "log.warning": None, "_err": {"E": 4001}},
     {"sessions": {}})
case("_sess_nowait", {"params": {"session_id": "nope"}, "rid": "r1"},
     {"_current_rpc_method.get": None, "log.warning": None, "_err": {"E": 4001}},
     {"sessions": {}})
case("_sess_nowait", {"params": {"session_id": "nope"}, "rid": "r1"},
     {}, {"sessions": {}}, fail_closed=True)
case("_sess_nowait", {"params": {"session_id": "nope"}, "rid": "r1"},
     {"_current_rpc_method.get": "m", "log.warning": R("SystemExit", "bye")},
     {"sessions": {}})
case("_sess_nowait", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_current_rpc_method.get": "m", "log.warning": None, "_err": {"E": 4001}},
     {"sessions": {"s1": {}}})  # empty record is falsy -> not-found branch

# ── _sess_building ──
case("_sess_building", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_nowait": S([{"session_key": "k"}, None]), "_start_agent_build": None})
case("_sess_building", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_nowait": S([None, {"E": 4001}])})
case("_sess_building", {"params": {"session_id": "s1"}, "rid": "r1"}, {}, fail_closed=True)
case("_sess_building", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_nowait": S([{"session_key": "k"}, None]), "_start_agent_build": R("SystemExit", "bye")})
case("_sess_building", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_nowait": S([{"session_key": "k"}, None]), "_start_agent_build": R("ValueError", "x")})
case("_sess_building", {"params": {}, "rid": "r1"},
     {"_sess_nowait": S([{"session_key": "k"}, None]), "_start_agent_build": None})

# ── _sess ──
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_building": S([{"session_key": "k"}, None]), "_wait_agent": None})
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_building": S([{"session_key": "k"}, None]), "_wait_agent": {"E": 5032}})
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_building": S([None, {"E": 4001}])})
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"}, {}, fail_closed=True)
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_building": S([{"session_key": "k"}, None]), "_wait_agent": R("SystemExit", "bye")})
case("_sess", {"params": {"session_id": "s1"}, "rid": "r1"},
     {"_sess_building": S([{"session_key": "k"}, {"E": 1}])})

# ── _new_session_key ──
case("_new_session_key", {}, {"new_session_id": "20261118_120000_abc123"})
case("_new_session_key", {}, {"new_session_id": "20261118_120000_def456"})
case("_new_session_key", {}, {}, fail_closed=True)
case("_new_session_key", {}, {"new_session_id": R("SystemExit", "bye")})
case("_new_session_key", {}, {"new_session_id": R("ValueError", "x")})
case("_new_session_key", {}, {"new_session_id": ""})

# ── _is_server_minted_key ──
case("_is_server_minted_key", {"value": "20261118_120000_abc123"})
case("_is_server_minted_key", {"value": "20261118_120000_ABC123"})
case("_is_server_minted_key", {"value": "20261118_120000_abc12"})
case("_is_server_minted_key", {"value": None})
case("_is_server_minted_key", {"value": ""})
case("_is_server_minted_key", {"value": "abcd1234"})
case("_is_server_minted_key", {"value": "20261118_120000_000000"})

# ── _any_live_session_claims_key ──
case("_any_live_session_claims_key", {"target": "k1"},
     {}, {"sessions": {"s1": {"session_key": "k1"}}})
case("_any_live_session_claims_key", {"target": "kX"},
     {}, {"sessions": {"s1": {"session_key": "k1"}}})
case("_any_live_session_claims_key", {"target": "k1"}, {}, {"sessions": {}})
case("_any_live_session_claims_key", {"target": "k1"},
     {}, {"sessions": {"s1": "notadict", "s2": {"session_key": "k1"}}})
case("_any_live_session_claims_key", {"target": ""},
     {}, {"sessions": {"s1": {"session_key": None}}})
case("_any_live_session_claims_key", {"target": "k2"},
     {}, {"sessions": {"s1": {"session_key": "k1"}, "s2": {"session_key": "k2"}}})

# ── _live_profile_matches ──
case("_live_profile_matches", {"session": {"profile_home": "/h/p1"}, "profile_home": "__ANY_PROFILE__"})
case("_live_profile_matches", {"session": {"profile_home": "/h/p1"}, "profile_home": "/h/p1"})
case("_live_profile_matches", {"session": {"profile_home": "/h/p1"}, "profile_home": "/h/p2"})
case("_live_profile_matches", {"session": {"profile_home": None}, "profile_home": None})
case("_live_profile_matches", {"session": {"profile_home": None}, "profile_home": "/h/p1"})
case("_live_profile_matches", {"session": {"profile_home": "/h/p1"}, "profile_home": None})
case("_live_profile_matches", {"session": {}, "profile_home": "__ANY_PROFILE__"})

# ── _session_lookup_key ──
case("_session_lookup_key", {"session": {"agent": {"session_id": "A"}, "session_key": "K"}, "fallback": "F"})
case("_session_lookup_key", {"session": {"session_key": "K"}, "fallback": "F"})
case("_session_lookup_key", {"session": {}, "fallback": "F"})
case("_session_lookup_key", {"session": {}, "fallback": ""})
case("_session_lookup_key", {"session": {"agent": {}, "session_key": "K"}, "fallback": "F"})
case("_session_lookup_key", {"session": {"agent": None, "session_key": "K"}, "fallback": "F"})

# ── _find_live_session_by_key ──
case("_find_live_session_by_key", {"session_key": "k1"},
     {}, {"sessions": {"s1": {"session_key": "k1", "agent": {"session_id": "k1"}}}})
case("_find_live_session_by_key", {"session_key": "kX"},
     {}, {"sessions": {"s1": {"session_key": "k1"}}})
case("_find_live_session_by_key", {"session_key": "k1"},
     {}, {"sessions": {"s1": {"session_key": "k1", "_finalized": True}}})
case("_find_live_session_by_key", {"session_key": "k1", "profile_home": "/h/p2"},
     {}, {"sessions": {"s1": {"session_key": "k1", "profile_home": "/h/p1"}}})
case("_find_live_session_by_key", {"session_key": "k1", "profile_home": "/h/p1"},
     {}, {"sessions": {"s1": {"session_key": "k1", "profile_home": "/h/p1"}}})
case("_find_live_session_by_key", {"session_key": "A"},
     {}, {"sessions": {"s1": {"session_key": "k1", "agent": {"session_id": "A"}}}})

# ── _claim_parked_runtimes ──
PARK_REC = {"session_key": "k1", "transport": "__DETACHED_WS__", "agent": {"session_id": "k1"}}
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {"_cancel_ws_orphan_reap": None, "_pop_session_by_id": S({"session_key": "k1"})},
     {"sessions": {"old1": dict(PARK_REC), "old2": {"session_key": "k1", "transport": "other"}}})
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {}, {"sessions": {"old1": {"session_key": "k1", "transport": "other"}}})
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "old1"},
     {}, {"sessions": {"old1": dict(PARK_REC)}})
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {"_cancel_ws_orphan_reap": None, "_pop_session_by_id": None},
     {"sessions": {"old1": dict(PARK_REC)}})
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {}, {"sessions": {"old1": dict(PARK_REC)}}, fail_closed=True)
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {"_cancel_ws_orphan_reap": None, "_pop_session_by_id": R("SystemExit", "bye")},
     {"sessions": {"old1": dict(PARK_REC)}})
case("_claim_parked_runtimes", {"session_key": "k1", "keep_sid": "keep"},
     {}, {"sessions": {"old1": {**PARK_REC, "_finalized": True}}})

# ── _finalize_superseded_runtimes ──
case("_finalize_superseded_runtimes", {"stale": []})
case("_finalize_superseded_runtimes", {"stale": [["s1", {"session_key": "k"}]]},
     {"_teardown_popped_session": None})
case("_finalize_superseded_runtimes", {"stale": [["s1", {"session_key": "k"}]]},
     {"_teardown_popped_session": R("ValueError", "x"), "log.exception": None})
case("_finalize_superseded_runtimes", {"stale": [["s1", {"session_key": "k"}]]},
     {"_teardown_popped_session": R("SystemExit", "bye")})
case("_finalize_superseded_runtimes", {"stale": [["s1", {"session_key": "k"}]]},
     {"log.exception": None}, fail_closed=True)
case("_finalize_superseded_runtimes",
     {"stale": [["s1", {"a": 1}], ["s2", {"b": 2}]]},
     {"_teardown_popped_session": [None, R("ValueError", "x")], "log.exception": None})

# ── _claim_or_reuse_live ──
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": "/h/p1"}, "lease": {"__lease__": True}},
     {"_find_live_session_by_key": S(["s1", {"session_key": "k1"}]), "lease.release": None})
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": "/h/p1"}, "lease": None},
     {"_find_live_session_by_key": S(["s1", {"session_key": "k1"}])})
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": "/h/p1"}},
     {"_find_live_session_by_key": None, "_register_session_cwd": None,
      "_cancel_ws_orphan_reap": None, "_claim_parked_runtimes": S([]),
      "_finalize_superseded_runtimes": None},
     {"sessions": {}})
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": "/h/p1"}},
     {}, {"sessions": {}}, fail_closed=True)
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": "/h/p1"}},
     {"_find_live_session_by_key": None, "_register_session_cwd": R("SystemExit", "bye")},
     {"sessions": {}})
case("_claim_or_reuse_live",
     {"sid": "s2", "session_key": "k1", "record": {"profile_home": None}},
     {"_find_live_session_by_key": None, "_register_session_cwd": None,
      "_cancel_ws_orphan_reap": None, "_claim_parked_runtimes": S([["old", {"x": 1}]]),
      "_finalize_superseded_runtimes": None},
     {"sessions": {}})

# ── _session_pending_kind ──
case("_session_pending_kind", {"sid": "s1"}, {"pending_kind": "approval"})
case("_session_pending_kind", {"sid": "s1"}, {"pending_kind": ""})
case("_session_pending_kind", {"sid": "s1"}, {}, fail_closed=True)
case("_session_pending_kind", {"sid": "s1"}, {"pending_kind": R("SystemExit", "bye")})
case("_session_pending_kind", {"sid": "s1"}, {"pending_kind": R("ValueError", "x")})
case("_session_pending_kind", {"sid": "s2"}, {"pending_kind": "connection"})

# ── _session_live_status ──
case("_session_live_status", {"sid": "s1", "session": {}}, {"_session_pending_kind": "approval"})
case("_session_live_status", {"sid": "s1", "session": {"running": True}}, {"_session_pending_kind": ""})
case("_session_live_status", {"sid": "s1", "session": {"running": False}}, {"_session_pending_kind": ""})
case("_session_live_status",
     {"sid": "s1", "session": {"agent_ready": READY, "agent_build_started": True}},
     {"_session_pending_kind": "", "agent_ready.is_set": False})
case("_session_live_status",
     {"sid": "s1", "session": {"agent_ready": READY, "agent_build_started": True, "running": True}},
     {"_session_pending_kind": "", "agent_ready.is_set": True})
case("_session_live_status", {"sid": "s1", "session": {}}, {}, fail_closed=True)
case("_session_live_status",
     {"sid": "s1", "session": {"agent_ready": READY}},
     {"_session_pending_kind": "", "agent_ready.is_set": R("SystemExit", "bye")})

# ── _session_live_title ──
case("_session_live_title", {"session": {"pending_title": "  hi  "}, "key": "k1"},
     {"session_db": "db", "persistence_for": {"__persistence__": True}, "get_title": "  Stored Title "})
case("_session_live_title", {"session": {"pending_title": "hi"}, "key": "k1"},
     {"session_db": "db", "persistence_for": None})
case("_session_live_title", {"session": {"pending_title": "hi"}, "key": "k1"},
     {"session_db": "db", "persistence_for": {"__persistence__": True}, "get_title": None})
case("_session_live_title", {"session": {"pending_title": "hi"}, "key": "k1"},
     {"session_db": R("ValueError", "x")})
case("_session_live_title", {"session": {"pending_title": "hi"}, "key": "k1"},
     {"session_db": "db", "persistence_for": {"__persistence__": True}, "get_title": R("SystemExit", "bye")})
case("_session_live_title", {"session": {"pending_title": "hi"}, "key": "k1"}, {}, fail_closed=True)
case("_session_live_title", {"session": {"pending_title": None}, "key": "k1"},
     {"session_db": "db", "persistence_for": {"__persistence__": True}, "get_title": "T"})

# ── _durable_ids ──
case("_durable_ids", {"msg": {"message_uid": "u1", "_row_id": 5}})
case("_durable_ids", {"msg": {"message_uid": "u1", "_absorbed_message_uids": ["u2", "u3"], "_row_id": 2}})
case("_durable_ids", {"msg": "notadict"})
case("_durable_ids", {"msg": {"_row_id": True}})
case("_durable_ids", {"msg": {}})
case("_durable_ids", {"msg": {"message_uid": "", "_row_id": 7}})
case("_durable_ids", {"msg": {"_row_id": 7.5}})
case("_durable_ids", {"msg": {"message_uid": "u1", "_absorbed_message_uids": ["u1", "u2"]}})

# ── _legacy_display_anchor ──
case("_legacy_display_anchor",
     {"display": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}],
      "memory": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}]},
     {"_coerce_message_text": ["a", "b", "a", "b"]})
case("_legacy_display_anchor",
     {"display": [{"role": "user", "content": "a"}], "memory": [{"role": "user", "content": "z"}]},
     {"_coerce_message_text": ["a", "z"]})
case("_legacy_display_anchor",
     {"display": [], "memory": [{"role": "user", "content": "a"}]},
     {"_coerce_message_text": ["a"]})
case("_legacy_display_anchor",
     {"display": [{"role": "user", "content": "a"}], "memory": ["x", {"role": "user", "content": "a"}]},
     {"_coerce_message_text": ["a", "a"]})
case("_legacy_display_anchor",
     {"display": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}, {"role": "user", "content": "c"}],
      "memory": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}, {"role": "user", "content": "c"}]},
     {"_coerce_message_text": ["a", "b", "c", "a", "b", "c"]})
case("_legacy_display_anchor",
     {"display": [{"role": "user", "content": "a"}], "memory": [{"role": "user", "content": "a"}]},
     {}, fail_closed=True)

# ── _reconcile_display_with_live ──
case("_reconcile_display_with_live", {"db_display": [], "in_memory": [{"role": "user", "content": "a"}]})
case("_reconcile_display_with_live", {"db_display": [{"role": "user", "content": "a"}], "in_memory": []})
case("_reconcile_display_with_live",
     {"db_display": [{"role": "user", "content": "a", "_row_id": 1}],
      "in_memory": [{"role": "user", "content": "a", "_row_id": 1}, {"role": "assistant", "content": "b", "_row_id": 2}]})
case("_reconcile_display_with_live",
     {"db_display": [{"role": "user", "content": "a", "_row_id": 1}],
      "in_memory": [{"role": "user", "content": "a", "_row_id": 9}]})
case("_reconcile_display_with_live",
     {"db_display": [{"role": "user", "content": "a"}],
      "in_memory": [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}]},
     {"_coerce_message_text": ["a", "a"]})
case("_reconcile_display_with_live",
     {"db_display": [{"role": "user", "content": "a"}], "in_memory": [{"role": "user", "content": "z"}]},
     {"_coerce_message_text": ["a", "z"]})
case("_reconcile_display_with_live",
     {"db_display": [{"role": "user", "content": "a"}], "in_memory": [{"role": "user", "content": "a"}]},
     {}, fail_closed=True)

# ── _main_runtime_from_agent ──
case("_main_runtime_from_agent",
     {"agent": {"provider": " p ", "model": "m", "base_url": "u", "api_key": "k",
                "api_mode": "chat", "auth_mode": "oauth", "session_id": "s"}})
case("_main_runtime_from_agent", {"agent": None})
case("_main_runtime_from_agent", {"agent": {}})
case("_main_runtime_from_agent", {"agent": {"model": " m1 ", "api_key": {"__callable__": True}}})
case("_main_runtime_from_agent", {"agent": {"model": "   ", "provider": "\t"}})
case("_main_runtime_from_agent", {"agent": {"model": 5, "provider": None, "session_id": "s"}})
case("_main_runtime_from_agent", {"agent": {"api_key": {"__callable__": True}}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [f for f, n in counts.items() if n < 5]
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if thin:
        raise SystemExit(f"THIN COVERAGE (<5): {thin}")


if __name__ == "__main__":
    main()