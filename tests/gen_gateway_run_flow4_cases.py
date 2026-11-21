#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_run_flow4.jsonl — differential cases for
the gateway/run.py fourth-wave FLOW port (src/gateway/gateway_run_flow4.c).

Deterministic, >=6 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), real
exception kinds (Exception swallowed where live swallows; BaseException
passes through), and the documented marker/scalar domains.

Run: python tests/gen_gateway_run_flow4_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_run_flow4.jsonl")
CASES = []


def C(fn, io=None, **inp):
    CASES.append({"func": fn, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


ST2 = {"t1": {"turn": {"agent": "a1"}}, "t2": {"turn": {"agent": None}}}
ST3 = {"t1": {"turn": {"agent": "a1"}}, "t2": {"turn": {"agent": "a2"}}}

# ── status_action_label ──
C("status_action_label", {}, state={"_restart_requested": True})
C("status_action_label", {}, state={"_restart_requested": False})
C("status_action_label", {}, state={"_restart_requested": 1})
C("status_action_label", {}, state={"_restart_requested": 0})
C("status_action_label", {}, state={"_restart_requested": None})
C("status_action_label", {}, state={})

# ── status_action_gerund ──
C("status_action_gerund", {"t": "restarting"}, state={"_restart_requested": True})
C("status_action_gerund", {"t": "shutting"}, state={"_restart_requested": False})
C("status_action_gerund", {"t": None}, state={"_restart_requested": True})
C("status_action_gerund", {"t": R("RuntimeError", "t down")},
  state={"_restart_requested": True})
C("status_action_gerund", {"t": R("SystemExit")}, state={"_restart_requested": True})
C("status_action_gerund", {}, state={"_restart_requested": True})

# ── sessions_map ──
C("sessions_map", {}, state={})
C("sessions_map", {}, state={"_sessions": None})
C("sessions_map", {}, state={"_sessions": {}})
C("sessions_map", {}, state={"_sessions": {"t1": {"k": 1}}})
C("sessions_map", {}, state={"_sessions": {"t1": 1, "t2": 2}})
C("sessions_map", {}, state={"_sessions": "notadict"})

# ── peek_session_state ──
C("peek_session_state", {}, state={"_sessions": ST3}, session_key="t1")
C("peek_session_state", {}, state={"_sessions": ST3}, session_key="missing")
C("peek_session_state", {}, state={"_sessions": {}}, session_key="t1")
C("peek_session_state", {}, state={"_sessions": None}, session_key="t1")
C("peek_session_state", {}, state={"_sessions": "notadict"}, session_key="t1")
C("peek_session_state", {}, state={"_sessions": ST3})

# ── session_state ──
C("session_state", {"_sessions_map": ST3}, state={}, session_key="t1",
  runner={"_sessions_map": True})
C("session_state", {"_sessions_map": ST2, "SessionState": {"turn": {"agent": None}}},
  state={}, session_key="t2", runner={"_sessions_map": True})
C("session_state", {"_sessions_map": {}, "SessionState": {"turn": {"agent": "x"}}},
  state={}, session_key="new", runner={"_sessions_map": True})
C("session_state", {}, state={}, session_key="t1",
  runner={"_sessions_map": True})  # fail-closed _sessions_map
C("session_state", {"_sessions_map": R("RuntimeError", "map down")},
  state={}, session_key="t1", runner={"_sessions_map": True})
C("session_state", {"_sessions_map": ST3, "SessionState": R("SystemExit")},
  state={}, session_key="new", runner={"_sessions_map": True})

# ── is_session_running ──
C("is_session_running", {"_peek_session_state": ST3["t1"]}, session_key="t1",
  runner={"_peek_session_state": True})
C("is_session_running", {"_peek_session_state": ST2["t2"]}, session_key="t2",
  runner={"_peek_session_state": True})
C("is_session_running", {"_peek_session_state": None}, session_key="t1",
  runner={"_peek_session_state": True})
C("is_session_running", {"_peek_session_state": {"x": 1}}, session_key="t1",
  runner={"_peek_session_state": True})
C("is_session_running", {"_peek_session_state": R("RuntimeError", "peek")},
  session_key="t1", runner={"_peek_session_state": True})
C("is_session_running", {}, session_key="t1",
  runner={"_peek_session_state": True})  # fail-closed peek

# ── running_agent_items ──
C("running_agent_items", {"_sessions_map": ST3}, runner={"_sessions_map": True})
C("running_agent_items", {"_sessions_map": ST2}, runner={"_sessions_map": True})
C("running_agent_items", {"_sessions_map": {}}, runner={"_sessions_map": True})
C("running_agent_items", {"_sessions_map": {"t1": {"x": 1}}},
  runner={"_sessions_map": True})
C("running_agent_items", {"_sessions_map": R("RuntimeError", "map")},
  runner={"_sessions_map": True})
C("running_agent_items", {}, runner={"_sessions_map": True})  # fail-closed

# ── running_agent_count ──
C("running_agent_count", {}, state={"_running_agents": ["a", "b"]})
C("running_agent_count", {}, state={"_running_agents": []})
C("running_agent_count", {}, state={"_running_agents": {"a": 1, "b": 2, "c": 3}})
C("running_agent_count", {}, state={"_running_agents": "abcd"})
C("running_agent_count", {}, state={"_running_agents": None})
C("running_agent_count", {}, state={})

# ── resolve_session_key_or_none ──
C("resolve_session_key_or_none", {}, session_key="given", source={"platform": "telegram"})
C("resolve_session_key_or_none", {}, session_key=None, source=None)
C("resolve_session_key_or_none", {"_session_key_for_source": "derived"},
  session_key="", source={"platform": "telegram"},
  runner={"_session_key_for_source": True})
C("resolve_session_key_or_none",
  {"_session_key_for_source": R("RuntimeError", "cannot derive")},
  session_key="", source={"platform": "telegram"},
  runner={"_session_key_for_source": True})
C("resolve_session_key_or_none", {"_session_key_for_source": R("SystemExit")},
  session_key="", source={"platform": "telegram"},
  runner={"_session_key_for_source": True})
C("resolve_session_key_or_none", {}, session_key="", source={"platform": "telegram"},
  runner={"_session_key_for_source": True})

# ── get_cached_session_source ──
CACHE = {"s1": {"platform": "telegram", "user_id": "u1"}}
C("get_cached_session_source", {"move_to_end": None},
  state={"_session_sources": CACHE}, session_key="s1")
C("get_cached_session_source", {}, state={"_session_sources": CACHE}, session_key="missing")
C("get_cached_session_source", {}, state={"_session_sources": {}}, session_key="s1")
C("get_cached_session_source", {}, state={}, session_key="s1")
C("get_cached_session_source", {"move_to_end": R("RuntimeError", "ordered down")},
  state={"_session_sources": CACHE}, session_key="s1")
C("get_cached_session_source", {"move_to_end": R("SystemExit")},
  state={"_session_sources": CACHE}, session_key="s1")
C("get_cached_session_source", {}, state={"_session_sources": CACHE}, session_key="")

# ── stop_pool ──
C("stop_pool", {"shutdown": None}, executor={"_threads": ["t1", "t2"]})
C("stop_pool", {"shutdown": None}, executor={"_threads": []})
C("stop_pool", {"shutdown": [R("TypeError", "no cancel_futures"), None]},
  executor={"_threads": ["t1"]})
C("stop_pool", {"shutdown": R("ValueError", "bad")}, executor={"_threads": ["t1"]})
C("stop_pool", {"shutdown": R("SystemExit")}, executor={"_threads": ["t1"]})
C("stop_pool", {}, executor={"_threads": ["t1"]})  # fail-closed shutdown
C("stop_pool", {"shutdown": None}, executor=None)

# ── get_or_create_pool ──
C("get_or_create_pool",
  {"lock.enter": None, "lock.exit": None},
  state={"_executor_lock": "lk", "_executor": {"tag": "pool1"}},
  runner={}, attr="_executor")
C("get_or_create_pool",
  {"lock.enter": None, "lock.exit": None},
  state={"_executor_lock": "lk", "_executor_closing": True},
  runner={}, attr="_executor")
C("get_or_create_pool",
  {"Lock": "lk2", "lock.enter": None, "lock.exit": None, "make_pool": "pool2"},
  state={}, runner={}, attr="_executor")
C("get_or_create_pool",
  {"lock.enter": R("RuntimeError", "enter down")},
  state={"_executor_lock": "lk"}, runner={}, attr="_executor")
C("get_or_create_pool",
  {"lock.enter": None, "lock.exit": R("OSError", "exit down")},
  state={"_executor_lock": "lk", "_executor": {"tag": "p"}}, runner={}, attr="_executor")
C("get_or_create_pool",
  {"Lock": "lk2", "lock.enter": None, "lock.exit": None},
  state={"_executor": {"_shutdown": True}}, runner={}, attr="_executor")  # fail-closed make_pool

# ── update_runtime_status ──
RUNNER_ACTIVE = {"_describe_active_work": True, "_active_work_count": True}
C("update_runtime_status",
  {"_describe_active_work": {"turn": 1}, "_active_work_count": 2,
   "_write_runtime_status_quiet": None},
  state={"_startup_cleanup_only": False, "_restart_requested": True},
  runner=RUNNER_ACTIVE, gateway_state="draining", exit_reason="restart")
C("update_runtime_status",
  {"_active_work_count": 0, "_write_runtime_status_quiet": None},
  state={"_startup_cleanup_only": False, "_restart_requested": False},
  runner=RUNNER_ACTIVE, gateway_state="stopping", exit_reason=None)
C("update_runtime_status", {},
  state={"_startup_cleanup_only": True, "_restart_requested": False},
  runner=RUNNER_ACTIVE, gateway_state="draining", exit_reason=None)
C("update_runtime_status",
  {"_describe_active_work": R("RuntimeError", "work down")},
  state={"_startup_cleanup_only": False, "_restart_requested": True},
  runner=RUNNER_ACTIVE, gateway_state="draining", exit_reason=None)
C("update_runtime_status",
  {"_active_work_count": R("RuntimeError", "count down")},
  state={"_startup_cleanup_only": False, "_restart_requested": True},
  runner=RUNNER_ACTIVE, gateway_state=None, exit_reason=None)
C("update_runtime_status",
  {"_active_work_count": 1, "_write_runtime_status_quiet": R("SystemExit")},
  state={"_startup_cleanup_only": False, "_restart_requested": True},
  runner=RUNNER_ACTIVE, gateway_state=None, exit_reason="bye")

# ── session_db_get ──
C("session_db_get", {}, state={"_session_db_pinned": None}, runner={})
C("session_db_get", {}, state={"_session_db_pinned": {"tag": "fake"}}, runner={})
C("session_db_get", {"_open_session_db_for_active_scope": "handle1"},
  state={"_session_db_pinned": {"__unpinned__": True}},
  runner={"_open_session_db_for_active_scope": True})
C("session_db_get", {"_open_session_db_for_active_scope": R("RuntimeError", "open")},
  state={"_session_db_pinned": {"__unpinned__": True}},
  runner={"_open_session_db_for_active_scope": True})
C("session_db_get", {"_open_session_db_for_active_scope": R("SystemExit")},
  state={"_session_db_pinned": {"__unpinned__": True}},
  runner={"_open_session_db_for_active_scope": True})
C("session_db_get", {}, state={"_session_db_pinned": {"__unpinned__": True}},
  runner={"_open_session_db_for_active_scope": True})  # fail-closed
C("session_db_get", {}, state={}, runner={})  # missing pinned attr


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert len(counts) == 14, counts
    for f, n in sorted(counts.items()):
        assert n >= 6, (f, n)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    print("wrote %s: %d cases over %d funcs" % (OUT, len(CASES), len(counts)))
    for f, n in sorted(counts.items()):
        print("  %-40s %d" % (f, n))


if __name__ == "__main__":
    main()
