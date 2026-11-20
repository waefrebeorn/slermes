#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_audit_flow.jsonl -- the scripted-seam
FLOW cases for the tui_gateway/methods_session_control.py port
(tui_methods_audit_flow).

Deterministic; >=5 cases per ported func. Each case is
{"func", "in": {"args", "io", "state"}, "fail_closed"?}. io maps seam name ->
script (plain value reused, list consumed per call, or {"__script__": true,
"ret"/"raise"}). A missing script is the fail-closed RuntimeError probe.

Fixture domain (shared with the port header): ASCII strings; INTS only (the
revision digest is float-repr sensitive); state scripts are dicts or null
(heartbeat pause/resume states are null or NON-EMPTY dicts); list-valued seam
RESULTS are wrapped one level deeper (bare array == per-call queue); locks
script {"__lock__": true}; manager markers {"state": {"subgoals": [...]}}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_audit_flow.jsonl")

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


def ret(value):
    return {"__script__": True, "ret": value}


def ok(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def err(rid, code, message):
    return {"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}}


LOCK = {"__lock__": True}
CONTRACT = {"kind": "tests", "pass": True}
GOAL = {
    "goal": "ship the port", "status": "active", "turns_used": 3, "max_turns": 10,
    "contract": dict(CONTRACT), "subgoals": ["a", "b"],
    "gates": [{"command": "make test", "timeout_seconds": 60, "max_retries": 2,
               "attempts": 1, "last_exit_code": 0}],
    "created_at": 100, "last_turn_at": 200, "paused_reason": "",
    "last_verdict": None, "last_reason": "",
    "waiting_reason": "gate", "waiting_until": 0,
    "waiting_on_session": None, "waiting_on_pid": None,
}
LOOP = {
    "prompt": "tick", "status": "active", "mode": "fixed", "interval_seconds": 60,
    "current_delay": 60, "times": 5, "until": 0, "max_ticks": 100, "ticks_fired": 5,
    "created_at": 90, "last_fired_at": 150, "next_due_at": 210,
    "awaiting_response": False, "paused_reason": "", "last_stop_reason": "",
}
HB = {"prompt": "beat", "status": "active", "interval_seconds": 600,
      "created_at": 80, "last_fired_at": 120, "fire_count": 2}


def goal(**over):
    g = dict(GOAL)
    g.update(over)
    return g


def loop(**over):
    l = dict(LOOP)
    l.update(over)
    return l


def hb(**over):
    h = dict(HB)
    h.update(over)
    return h


def sess(session, e=None):
    return ret([session, e])


NULL_LOADS = {"load_goal": None, "load_loop": None, "load_heartbeat": None}
NULL_LOADS2 = {"load_goal": [None, None], "load_loop": [None, None],
               "load_heartbeat": [None, None]}

# -- _safe_goal_snapshot --
add("_safe_goal_snapshot", {"state": None})
add("_safe_goal_snapshot", {"state": {"status": "cleared"}})
add("_safe_goal_snapshot", {"state": GOAL}, {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": goal(waiting_until=500)},
    {"contract_to_dict": CONTRACT, "time_time": 100})
add("_safe_goal_snapshot", {"state": goal(waiting_on_session="s9")},
    {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": goal(waiting_on_pid=4242)},
    {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": {"status": "active"}})
add("_safe_goal_snapshot", {"state": "x"})
add("_safe_goal_snapshot", {"state": goal(contract=None)})
add("_safe_goal_snapshot", {"state": goal(gates=None)}, {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": GOAL}, {"contract_to_dict": rs("ValueError", "bad contract")})
add("_safe_goal_snapshot", {"state": GOAL}, {"contract_to_dict": rs("SystemExit", "bye")})
add("_safe_goal_snapshot",
    {"state": goal(created_at=0, last_turn_at=0, paused_reason="",
                   last_verdict=None, last_reason="")},
    {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": goal(subgoals="ab")}, {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot",
    {"state": goal(gates=[{"command": "c", "timeout_seconds": 1, "max_retries": 0,
                           "attempts": 0}])},
    {"contract_to_dict": CONTRACT})
add("_safe_goal_snapshot", {"state": goal(waiting_reason=None, waiting_until=100)},
    {"contract_to_dict": CONTRACT, "time_time": 50})

# -- _extract_wait_barrier --
BSTATE = {"waiting_reason": "r", "waiting_until": 100,
          "waiting_on_session": None, "waiting_on_pid": None}
add("_extract_wait_barrier", {"state": BSTATE}, {"time_time": 50})
add("_extract_wait_barrier",
    {"state": dict(BSTATE, waiting_on_session="s9")}, {"time_time": 200})
add("_extract_wait_barrier",
    {"state": dict(BSTATE, waiting_until=0, waiting_on_pid=4242)})
add("_extract_wait_barrier", {"state": dict(BSTATE, waiting_until=0)})
add("_extract_wait_barrier", {"state": dict(BSTATE, waiting_reason=None, waiting_until=0)})
add("_extract_wait_barrier", {"state": {"waiting_until": 0, "waiting_on_session": None,
                                        "waiting_on_pid": None}})
add("_extract_wait_barrier", {"state": BSTATE}, {"time_time": rs("RuntimeError", "clock")})
add("_extract_wait_barrier", {"state": dict(BSTATE, waiting_until="x")}, {"time_time": 50})
add("_extract_wait_barrier", {"state": BSTATE}, {"time_time": None})
add("_extract_wait_barrier", {"state": BSTATE}, {"time_time": 100})

# -- _safe_loop_snapshot --
add("_safe_loop_snapshot", {"state": None, "deferred_by_goal": False})
add("_safe_loop_snapshot", {"state": {"status": "cleared"}, "deferred_by_goal": True})
add("_safe_loop_snapshot",
    {"state": loop(paused_reason="wait", last_stop_reason="done"), "deferred_by_goal": True})
add("_safe_loop_snapshot", {"state": LOOP, "deferred_by_goal": False})
add("_safe_loop_snapshot", {"state": {"status": "active"}, "deferred_by_goal": False})
add("_safe_loop_snapshot", {"state": 5, "deferred_by_goal": False})
add("_safe_loop_snapshot",
    {"state": {k: v for k, v in LOOP.items() if k != "next_due_at"},
     "deferred_by_goal": False})

# -- _safe_heartbeat_snapshot --
add("_safe_heartbeat_snapshot", {"state": None})
add("_safe_heartbeat_snapshot", {"state": {"status": "cleared"}})
add("_safe_heartbeat_snapshot", {"state": HB})
add("_safe_heartbeat_snapshot", {"state": {"status": "active"}})
add("_safe_heartbeat_snapshot", {"state": "x"})
add("_safe_heartbeat_snapshot",
    {"state": {k: v for k, v in HB.items() if k != "fire_count"}})

# -- _snapshot_control --
add("_snapshot_control", {"session_key": "sk"}, dict(NULL_LOADS))
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": GOAL, "load_loop": LOOP, "load_heartbeat": HB,
     "contract_to_dict": CONTRACT, "goal_blocks_loop_tick": True})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": GOAL, "load_loop": LOOP, "load_heartbeat": HB,
     "contract_to_dict": CONTRACT, "goal_blocks_loop_tick": False})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": GOAL, "load_loop": loop(status="paused"), "load_heartbeat": HB,
     "contract_to_dict": CONTRACT})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": None, "load_loop": LOOP, "load_heartbeat": HB})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": rs("ValueError", "disk"), "load_loop": None, "load_heartbeat": None})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": GOAL, "load_loop": LOOP, "load_heartbeat": HB,
     "contract_to_dict": CONTRACT, "goal_blocks_loop_tick": rs("RuntimeError", "tick")})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": None, "load_loop": None, "load_heartbeat": rs("SystemExit", "bye")})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": goal(status="cleared"), "load_loop": LOOP, "load_heartbeat": HB})
add("_snapshot_control", {"session_key": "sk"},
    {"load_goal": None, "load_loop": "x", "load_heartbeat": None})

# -- _snapshot_revision --
add("_snapshot_revision", {})
add("_snapshot_revision", {"goal": {"a": 1}})
add("_snapshot_revision",
    {"goal": {"z": 1, "a": {"y": 2, "b": [1, 2]}},
     "loop": {"status": "active", "mode": "fixed"},
     "heartbeat": {"prompt": "beat", "fire_count": 2}})
add("_snapshot_revision", {"loop": {"prompt": "tick"}})
add("_snapshot_revision", {"heartbeat": {"fire_count": 0}})
add("_snapshot_revision", {"goal": {"s": 'q"u\\o\nte'}})
add("_snapshot_revision", {"goal": {}, "loop": {}, "heartbeat": {}})
add("_snapshot_revision", {"goal": {"l": [3, 1, 2], "b": True, "n": None}})

# -- _snapshot_updated_at --
add("_snapshot_updated_at", {})
add("_snapshot_updated_at", {"goal_state": {"created_at": 10, "last_turn_at": 20}})
add("_snapshot_updated_at",
    {"goal_state": {"created_at": 0}, "loop_state": {"created_at": 50},
     "heartbeat_state": {"last_fired_at": 30}})
add("_snapshot_updated_at",
    {"goal_state": {"created_at": 0, "last_turn_at": None},
     "loop_state": {"created_at": 0, "last_fired_at": 0},
     "heartbeat_state": {"created_at": None, "last_fired_at": 0}})
add("_snapshot_updated_at", {"heartbeat_state": {"last_fired_at": 99}})
add("_snapshot_updated_at", {"goal_state": "x", "loop_state": {"created_at": 5}})
add("_snapshot_updated_at", {"goal_state": {}, "loop_state": {}, "heartbeat_state": {}})
add("_snapshot_updated_at", {"goal_state": {"created_at": None, "last_turn_at": 7}})

# -- _load_goal_state --
add("_load_goal_state", {"session_key": "sk"}, {"load_goal": GOAL})
add("_load_goal_state", {"session_key": "sk"}, {"load_goal": None})
add("_load_goal_state", {"session_key": "sk"}, {"load_goal": rs("ValueError", "disk")})
add("_load_goal_state", {"session_key": "sk"}, {"load_goal": rs("SystemExit", "bye")})
add("_load_goal_state", {"session_key": "sk"}, {"load_goal": rs("KeyError", "k")})
add("_load_goal_state", {"session_key": "sk"}, {}, fail_closed=True)

# -- _load_loop_state --
add("_load_loop_state", {"session_key": "sk"}, {"load_loop": LOOP})
add("_load_loop_state", {"session_key": "sk"}, {"load_loop": None})
add("_load_loop_state", {"session_key": "sk"}, {"load_loop": rs("OSError", "disk")})
add("_load_loop_state", {"session_key": "sk"}, {"load_loop": rs("KeyboardInterrupt", "")})
add("_load_loop_state", {"session_key": "sk"}, {}, fail_closed=True)

# -- _load_heartbeat_state --
add("_load_heartbeat_state", {"session_key": "sk"}, {"load_heartbeat": HB})
add("_load_heartbeat_state", {"session_key": "sk"}, {"load_heartbeat": None})
add("_load_heartbeat_state", {"session_key": "sk"}, {"load_heartbeat": rs("RuntimeError", "x")})
add("_load_heartbeat_state", {"session_key": "sk"}, {"load_heartbeat": rs("GeneratorExit", "")})
add("_load_heartbeat_state", {"session_key": "sk"}, {}, fail_closed=True)

# -- _goal_blocks_loop_tick --
add("_goal_blocks_loop_tick", {"session_key": "sk"}, {"goal_blocks_loop_tick": True})
add("_goal_blocks_loop_tick", {"session_key": "sk"}, {"goal_blocks_loop_tick": False})
add("_goal_blocks_loop_tick", {"session_key": "sk"}, {"goal_blocks_loop_tick": None})
add("_goal_blocks_loop_tick", {"session_key": "sk"}, {"goal_blocks_loop_tick": 1})
add("_goal_blocks_loop_tick", {"session_key": "sk"},
    {"goal_blocks_loop_tick": rs("RuntimeError", "tick")})
add("_goal_blocks_loop_tick", {"session_key": "sk"}, {}, fail_closed=True)

# -- _session_publish_lock --
add("_session_publish_lock", {}, {"new_lock": LOCK}, state={"session": {}})
add("_session_publish_lock", {}, {}, state={"session": {"control_publish_lock": LOCK}})
add("_session_publish_lock", {}, {"new_lock": LOCK},
    state={"session": {"control_publish_lock": None}})
add("_session_publish_lock", {}, {}, state={})
add("_session_publish_lock", {}, {}, state={"session": "x"})
add("_session_publish_lock", {}, {"new_lock": rs("RuntimeError", "lock")},
    state={"session": {}})
add("_session_publish_lock", {}, {"new_lock": None}, state={"session": {}})
add("_session_publish_lock", {}, {},
    state={"session": {"control_publish_lock": LOCK, "other": 1}})

# -- _publish_session_control_snapshot --
PSESS = {"session": {"session_key": "sk1"}}
add("_publish_session_control_snapshot", {"sid": "s1"}, {}, state={})
add("_publish_session_control_snapshot", {"sid": "s1"}, {}, state={"session": {}})
add("_publish_session_control_snapshot", {"sid": "s1"}, {},
    state={"session": {"session_key": ""}})
add("_publish_session_control_snapshot", {"sid": "s1"},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}, emit=None),
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1", "only_if_present": True},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}),
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1", "only_if_changed": True},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}),
    state={"session": {"session_key": "sk1", "control_revision": ""}})
add("_publish_session_control_snapshot", {"sid": "s1", "only_if_changed": True},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}, emit=None),
    state={"session": {"session_key": "sk1", "control_revision": "old"}})
add("_publish_session_control_snapshot", {"sid": "s1"},
    {"new_lock": LOCK, "session_profile_runtime_scope": "x", "logger_debug": None},
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1"},
    {"new_lock": LOCK, "session_profile_runtime_scope": {},
     "load_goal": rs("ValueError", "disk"), "logger_debug": None},
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1"},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={},
         emit=rs("RuntimeError", "wire"), logger_debug=None),
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1"},
    {"new_lock": LOCK, "session_profile_runtime_scope": {},
     "load_goal": rs("SystemExit", "bye")},
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1"}, {},
    state={"session": {"session_key": 0}})
add("_publish_session_control_snapshot", {"sid": "s1"},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}, emit=None),
    state={"session": {"session_key": 7}})
add("_publish_session_control_snapshot", {"sid": "s1"}, {}, state={"session": "x"})
add("_publish_session_control_snapshot", {"sid": "s1"},
    {"new_lock": None, "logger_debug": None},
    state={"session": {"session_key": "sk1", "control_publish_lock": None}})
add("_publish_session_control_snapshot", {"sid": "s1"},
    dict(NULL_LOADS, session_profile_runtime_scope={}, emit=None),
    state={"session": {"session_key": "sk1", "control_publish_lock": LOCK}})
add("_publish_session_control_snapshot", {"sid": 5},
    dict(NULL_LOADS, new_lock=LOCK, session_profile_runtime_scope={}, emit=None),
    state=PSESS)
add("_publish_session_control_snapshot", {"sid": "s1"},
    {"new_lock": rs("RuntimeError", "lock"), "logger_debug": None},
    state=PSESS)

# -- _h_control_read --
add("_h_control_read", {"rid": "r1", "params": {"session_id": "s1"}},
    {"sess_nowait": sess(None, err("r1", 5031, "no session"))})
add("_h_control_read", {"rid": "r1", "params": {}},
    {"sess_nowait": sess({"session_key": ""}),
     "err": err("r1", 4001, "session has no stored key")})
add("_h_control_read", {"rid": "r1", "params": {"session_id": "s1"}},
    dict(NULL_LOADS, sess_nowait=sess({"session_key": "sk1"}),
         ok=ok("r1", {"control": {"goal": None, "loop": None, "heartbeat": None,
                                  "revision": "", "updated_at": 0}})))
add("_h_control_read", {"rid": "r1", "params": {}}, {"sess_nowait": sess(None)})
add("_h_control_read", {"rid": "r1", "params": {}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "load_goal": rs("ValueError", "disk"),
     "logger_debug": None, "err": err("r1", 5031, "session.control.read failed: disk")})
add("_h_control_read", {"rid": "r1", "params": {}},
    dict(NULL_LOADS, sess_nowait=sess({"session_key": "sk1"}),
         ok=rs("RuntimeError", "wire"), logger_debug=None,
         err=err("r1", 5031, "session.control.read failed: wire")))
add("_h_control_read", {"rid": "r1", "params": {}},
    {"sess_nowait": sess({"session_key": "sk1"}), "load_goal": rs("SystemExit", "bye")})
add("_h_control_read", {"rid": "r1", "params": {}},
    {"sess_nowait": rs("RuntimeError", "sess")})
add("_h_control_read", {"rid": "r1", "params": {}}, {"sess_nowait": ret(None)})
add("_h_control_read", {"rid": "r1", "params": {}},
    {"sess_nowait": sess({"session_key": 0}),
     "err": err("r1", 4001, "session has no stored key")})

# -- _h_control --
add("_h_control", {"rid": "r1", "params": {}},
    {"err": err("r1", 4004, "action is required")})
add("_h_control", {"rid": "r1", "params": {"action": 5}},
    {"err": err("r1", 4004, "action is required")})
add("_h_control", {"rid": "r1", "params": {"action": "   "}},
    {"err": err("r1", 4004, "action is required")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.gate.add"}},
    {"err": err("r1", 4004, "gate actions are not allowed through session.control")})
add("_h_control", {"rid": "r1", "params": {"action": "bogus"}},
    {"err": err("r1", 4004, "unknown action: bogus")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "args": 5}},
    {"err": err("r1", 4004, "args must be an object")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "args": None}},
    {"err": err("r1", 4004, "args must be an object")})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.add", "args": {}}},
    {"err": err("r1", 4004, "subgoal text is required")})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.remove", "args": {"index": 0}}},
    {"err": err("r1", 4004, "subgoal index must be >= 1")})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.remove", "args": {"index": True}}},
    {"err": err("r1", 4004, "subgoal index must be an integer")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}), "methods_get": {"id": "h1"},
     "dispatch_handler": {"result": {"type": "exec", "output": "paused"}},
     **NULL_LOADS,
     "ok": ok("r1", {"control": {"goal": None, "loop": None, "heartbeat": None,
                                 "revision": "", "updated_at": 0},
                     "dispatch": {"type": "exec", "output": "paused", "notice": None,
                                  "message": None, "display": None}})})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.clear", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "goal_manager_new": {"state": {"subgoals": ["a", "b"]}},
     "manager_clear_subgoals": 2, **NULL_LOADS2,
     "new_lock": LOCK, "session_profile_runtime_scope": {}, "emit": None,
     "ok": ok("r1", {"control": {"goal": None, "loop": None, "heartbeat": None,
                                 "revision": "", "updated_at": 0},
                     "dispatch": {"type": "exec", "output": "Cleared", "notice": None,
                                  "message": None, "display": None}})})
add("_h_control",
    {"rid": "r1", "params": {"action": "subgoal.add", "args": {"text": "  new criterion  "},
                             "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "goal_manager_new": {"state": {"subgoals": ["a"]}},
     "manager_add_subgoal": "new criterion", **NULL_LOADS2,
     "new_lock": LOCK, "session_profile_runtime_scope": {}, "emit": None,
     "ok": ok("r1", {"ok": True})})
add("_h_control", {"rid": "r1", "params": {"action": "heartbeat.pause"}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "heartbeat_manager_new": {"id": "hm"},
     "manager_pause": {"prompt": "beat", "interval_seconds": 600}, **NULL_LOADS2,
     "new_lock": LOCK, "session_profile_runtime_scope": {}, "emit": None,
     "ok": ok("r1", {"ok": True})})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}), "methods_get": {"id": "h1"},
     "dispatch_handler": rs("ValueError", "bad dispatch"), "logger_debug": None,
     "err": err("r1", 4004, "goal.pause: bad dispatch")})
add("_h_control",
    {"rid": "r1", "params": {"action": "subgoal.remove", "args": {"index": 1}}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "goal_manager_new": {"state": {"subgoals": []}},
     "manager_remove_subgoal": rs("IndexError", "index out of range (1..0)"),
     "err": err("r1", 4004, "/subgoal remove: index out of range (1..0)")})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.clear"}},
    {"sess_nowait": sess({"session_key": "sk1"}),
     "goal_manager_new": rs("TypeError", "manager")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}), "methods_get": {"id": "h1"},
     "dispatch_handler": {"error": {"code": 5031, "message": "nope"}}})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}), "methods_get": {"id": "h1"},
     "dispatch_handler": {"result": {"type": "exec", "output": "ok"}},
     "load_goal": rs("ValueError", "disk"), "logger_debug": None,
     "err": err("r1", 5031, "session.control snapshot failed: disk")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.pause"}},
    {"sess_nowait": sess({"session_key": ""}),
     "err": err("r1", 4001, "session has no stored key")})
add("_h_control", {"rid": "r1", "params": {"action": "subgoal.add", "args": {"text": "   "}}},
    {"err": err("r1", 4004, "subgoal text is required")})
add("_h_control", {"rid": "r1", "params": {"action": "goal.gate"}},
    {"err": err("r1", 4004, "gate actions are not allowed through session.control")})
add("_h_control", {"rid": "r1", "params": {"action": "loop.stop", "session_id": "s1"}},
    {"sess_nowait": sess({"session_key": "sk1"}), "methods_get": None,
     "err": err("r1", 5031, "command.dispatch unavailable")})

# -- _validate_action_args --
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": {"text": " x "}})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": {"text": 5}},
    {"err": err("r1", 4004, "subgoal text is required")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": {"text": ""}},
    {"err": err("r1", 4004, "subgoal text is required")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": {"text": "   "}},
    {"err": err("r1", 4004, "subgoal text is required")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.remove", "args": {"index": 2}})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.remove", "args": {"index": 0}},
    {"err": err("r1", 4004, "subgoal index must be >= 1")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.remove", "args": {"index": True}},
    {"err": err("r1", 4004, "subgoal index must be an integer")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.remove", "args": {"index": "2"}},
    {"err": err("r1", 4004, "subgoal index must be an integer")})
add("_validate_action_args", {"rid": "r1", "action": "goal.pause", "args": {}})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": None})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.add", "args": {}},
    {"err": rs("RuntimeError", "wire")})
add("_validate_action_args", {"rid": "r1", "action": "subgoal.remove", "args": {"index": -3}},
    {"err": err("r1", 4004, "subgoal index must be >= 1")})

# -- _dispatch_command --
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": None, "err": err("r1", 5031, "command.dispatch unavailable")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": {"id": "h1"},
     "dispatch_handler": {"result": {"type": "exec", "output": "ok"}}})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": {"id": "h1"}, "dispatch_handler": rs("ValueError", "bad"),
     "logger_debug": None, "err": err("r1", 5031, "dispatch failed: bad")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": {"id": "h1"}, "dispatch_handler": rs("SystemExit", "bye")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": rs("RuntimeError", "registry")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": 5, "logger_debug": None,
     "err": err("r1", 5031, "dispatch failed: 'int' object is not callable")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": {"id": "h1"}, "dispatch_handler": rs("ValueError", "bad"),
     "logger_debug": rs("RuntimeError", "log")})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {"methods_get": {"id": "h1"}, "dispatch_handler": None})
add("_dispatch_command", {"rid": "r1", "session_id": "s1", "name": "goal", "arg": "pause"},
    {}, fail_closed=True)

# -- _execute_manager_action --
add("_execute_manager_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "t"}},
    {"goal_manager_new": {"state": {"subgoals": ["a"]}}, "manager_add_subgoal": "t"})
add("_execute_manager_action", {"session_key": "sk", "action": "heartbeat.clear", "args": {}},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_clear": True})
add("_execute_manager_action", {"session_key": "sk", "action": "subgoal.clear", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": []}}, "manager_clear_subgoals": 0})
add("_execute_manager_action", {"session_key": "sk", "action": "heartbeat.pause", "args": {}},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_pause": None})
add("_execute_manager_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "t"}},
    {"goal_manager_new": rs("RuntimeError", "no goal")})
add("_execute_manager_action", {"session_key": "sk", "action": "heartbeat.resume", "args": {}},
    {"heartbeat_manager_new": {"id": "hm"},
     "manager_resume": {"prompt": "beat", "interval_seconds": 600},
     "format_interval": "10m"})

# -- _execute_subgoal_action --
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "b"}},
    {"goal_manager_new": {"state": {"subgoals": ["a"]}}, "manager_add_subgoal": "b"})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.remove", "args": {"index": 1}},
    {"goal_manager_new": {"state": {"subgoals": ["a", "b"]}},
     "manager_remove_subgoal": "a"})
add("_execute_subgoal_action", {"session_key": "sk", "action": "subgoal.clear", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": ["a", "b"]}}, "manager_clear_subgoals": 2})
add("_execute_subgoal_action", {"session_key": "sk", "action": "subgoal.clear", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": ["a"]}}, "manager_clear_subgoals": 1})
add("_execute_subgoal_action", {"session_key": "sk", "action": "subgoal.clear", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": []}}, "manager_clear_subgoals": 0})
add("_execute_subgoal_action", {"session_key": "sk", "action": "subgoal.add", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": []}}})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "b"}},
    {"goal_manager_new": None})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "b"}},
    {"goal_manager_new": rs("RuntimeError", "no goal")})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "b"}},
    {"goal_manager_new": {"state": {"subgoals": ["a"]}},
     "manager_add_subgoal": rs("ValueError", "subgoal text is empty")})
add("_execute_subgoal_action", {"session_key": "sk", "action": "subgoal.bogus", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": ["a", "b", "c"]}},
     "manager_clear_subgoals": 3})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.remove", "args": {}},
    {"goal_manager_new": {"state": {"subgoals": []}}})
add("_execute_subgoal_action",
    {"session_key": "sk", "action": "subgoal.add", "args": {"text": "t"}},
    {"goal_manager_new": {}, "manager_add_subgoal": "t"})

# -- _execute_heartbeat_action --
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.pause"},
    {"heartbeat_manager_new": {"id": "hm"},
     "manager_pause": {"prompt": "beat", "interval_seconds": 600}})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.pause"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_pause": None})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.resume"},
    {"heartbeat_manager_new": {"id": "hm"},
     "manager_resume": {"prompt": "beat", "interval_seconds": 600},
     "format_interval": "10m"})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.resume"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_resume": None})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.clear"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_clear": True})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.clear"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_clear": False})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.bogus"},
    {"heartbeat_manager_new": {"id": "hm"},
     "err": err(None, 4004, "unknown heartbeat action: heartbeat.bogus")})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.pause"},
    {"heartbeat_manager_new": None})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.pause"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_pause": {"interval_seconds": 60}})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.resume"},
    {"heartbeat_manager_new": {"id": "hm"},
     "manager_resume": {"prompt": "beat", "interval_seconds": 600},
     "format_interval": rs("ValueError", "interval")})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.clear"},
    {"heartbeat_manager_new": rs("SystemExit", "bye")})
add("_execute_heartbeat_action", {"session_key": "sk", "action": "heartbeat.resume"},
    {"heartbeat_manager_new": {"id": "hm"}, "manager_resume": {"prompt": "beat"}})

# -- _manager_error_message --
add("_manager_error_message",
    {"action": "subgoal.add", "exc": {"__raise__": "ValueError", "message": "boom"}})
add("_manager_error_message",
    {"action": "subgoal.remove", "exc": {"__raise__": "IndexError", "message": "boom"}})
add("_manager_error_message",
    {"action": "subgoal.clear", "exc": {"__raise__": "RuntimeError", "message": "boom"}})
add("_manager_error_message",
    {"action": "goal.pause", "exc": {"__raise__": "ValueError", "message": "boom"}})
add("_manager_error_message",
    {"action": "heartbeat.clear", "exc": {"__raise__": "RuntimeError", "message": "boom"}})
add("_manager_error_message", {"action": "goal.pause", "exc": "raw"})
add("_manager_error_message",
    {"action": "subgoal.add", "exc": {"__raise__": "KeyError", "message": "'k'"}})

# -- _dispatch_envelope --
add("_dispatch_envelope",
    {"response": {"result": {"type": "exec", "output": "ok", "notice": "n",
                             "message": "m", "display": "d"}}})
add("_dispatch_envelope", {"response": {"result": {}}})
add("_dispatch_envelope", {"response": {"result": None}})
add("_dispatch_envelope", {"response": {}})
add("_dispatch_envelope", {"response": None})
add("_dispatch_envelope", {"response": {"result": "x"}})
add("_dispatch_envelope", {"response": {"result": {"type": "exec"}}})
add("_dispatch_envelope", {"response": {"result": 0}})

# -- register --
add("register", {}, {"bind_module": None})
add("register", {}, {"bind_module": rs("RuntimeError", "bind")})
add("register", {}, {}, fail_closed=True)
add("register", {}, {"bind_module": {"bound": True}})
add("register", {}, {"bind_module": rs("SystemExit", "bye")})


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
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"THIN coverage: {thin}")


if __name__ == "__main__":
    main()
