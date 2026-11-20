#!/usr/bin/env python3
"""Generate tests/fixtures/delegate_child_run_flow2.jsonl — the fixture corpus
for the delegate_child_run_flow2 FLOW oracle (17 unclaimed funcs of
tools/delegate_tool_child_run.py; >=5 cases per func).

Case shape: {"func": <live name>, "in": {..., "io": {seam: script}, "state": {...}}}
Script semantics (shared with the C harness dispatcher): a single value is
reused for every call; a BARE array is a per-call queue; a list RESULT must be
wrapped {"__script__": true, "ret": [...]}; {"__raise__": kind, "message": m}
raises (real builtin classes; SystemExit/KeyboardInterrupt/GeneratorExit pass
every except Exception). Absent/exhausted scripts fail closed with a
RuntimeError "no script for <seam>" raise spec recorded BEFORE propagation.

Domain (see the oracle header): ASCII fixture text; _diag_sizes tools_schema is
plain JSON (no floats / non-serializable objects); _register_child child carries
scalar attrs; _delegate_depth / current_turn_user_idx avoid integral floats in
isinstance-int positions; int()/float() string coercion is ASCII-ws-padded
signed digits; _stale_stop_seconds is int/float.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "delegate_child_run_flow2.jsonl"


def child(cid, **attrs):
    return {"__attr__": dict(attrs), "__id__": cid}


def agent(**attrs):
    return {"__attr__": dict(attrs)}


def cb(seam):
    return {"__callable__": seam}


def wrap(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, msg):
    return {"__raise__": kind, "message": msg}


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


# ---- _is_image_url (6) ----
add("_is_image_url", {"ref": "http://x/y.png"})
add("_is_image_url", {"ref": "https://x/y.png"})
add("_is_image_url", {"ref": "data:image/png;base64,AAA"})
add("_is_image_url", {"ref": "ftp://x/y.png"})
add("_is_image_url", {"ref": ""})
add("_is_image_url", {"ref": 5})  # AttributeError startswith

# ---- _budget_warning_text (6) ----
add("_budget_warning_text", {"idle_seconds": 0.0, "child_timeout": 30.0})
add("_budget_warning_text", {"idle_seconds": 450.5, "child_timeout": 1200.0})
add("_budget_warning_text", {"idle_seconds": 0.5, "child_timeout": 1.5})  # half-even
add("_budget_warning_text", {"idle_seconds": True, "child_timeout": False})
add("_budget_warning_text", {"idle_seconds": "x", "child_timeout": 30})  # ValueError
add("_budget_warning_text", {"idle_seconds": None, "child_timeout": 30})  # TypeError

# ---- _format_thread_stack (6) ----
add("_format_thread_stack", {"frame": {"f": 1}, "indent": "  ",
     "io": {"traceback.format_stack": wrap(["  File a\n    line\n"])}})
add("_format_thread_stack", {"frame": None, "indent": "",
     "io": {"traceback.format_stack": wrap(["line1\nline2", "  tail  \n"])}})
add("_format_thread_stack", {"frame": None, "indent": "  ",
     "io": {"traceback.format_stack": wrap([])}})
add("_format_thread_stack", {"frame": None, "indent": "  ",
     "io": {"traceback.format_stack": 5}})  # TypeError not iterable
add("_format_thread_stack", {"frame": None, "indent": "  ",
     "io": {"traceback.format_stack": wrap([5])}})  # AttributeError rstrip
add("_format_thread_stack", {"frame": None, "indent": "  ",
     "io": {"traceback.format_stack": rs("RuntimeError", "boom")}})

# ---- _diag_section (7) ----
add("_diag_section", {"label": "system_prompt: <error: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": wrap(["a", "b"])}})
add("_diag_section", {"label": "L: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": "ab"}})  # list(str) -> chars
add("_diag_section", {"label": "L: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": {"k": 1, "j": 2}}})  # list(dict) -> keys
add("_diag_section", {"label": "tool_schema: <error: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": 5}})  # TypeError caught -> label line
add("_diag_section", {"label": "L: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": rs("RuntimeError", "boom")}})  # caught -> label line
add("_diag_section", {"label": "L: ", "produce": cb("diag.produce"),
     "io": {"diag.produce": rs("SystemExit", "1")}})  # BaseException passes
add("_diag_section", {"label": "L: ", "produce": "notcallable"})  # TypeError caught

# ---- _diag_sizes (6) ----
add("_diag_sizes", {"state": {"child": child("c", system_prompt="hello", tools=[{"name": "terminal"}])}})
add("_diag_sizes", {"state": {"child": child("c", ephemeral_system_prompt="eph", system_prompt="sys", tools=None)}})
add("_diag_sizes", {"state": {"child": child("c", tools={"a": 1, "b": True})}})
add("_diag_sizes", {"state": {"child": child("c", tools=5)}})  # len TypeError -> label line
add("_diag_sizes", {"state": {"child": child("c")}})  # defaults: prompt "", tools None
add("_diag_sizes", {"state": {"child": child("c", system_prompt=5, tools=["x"])}})  # non-str prompt -> n/a

# ---- _warn_child_budget (6) ----
add("_warn_child_budget", {"idle_seconds": 10.0, "child_timeout": 30.0,
     "state": {"child": child("c", steer=cb("child.steer"))}, "io": {"child.steer": None}})
add("_warn_child_budget", {"idle_seconds": 10.0, "child_timeout": 30.0,
     "state": {"child": child("c", steer=cb("child.steer"))},
     "io": {"child.steer": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_warn_child_budget", {"idle_seconds": 10.0, "child_timeout": 30.0,
     "state": {"child": child("c", steer=cb("child.steer"))},
     "io": {"child.steer": rs("SystemExit", "1")}})  # passes
add("_warn_child_budget", {"idle_seconds": 10.0, "child_timeout": 30.0,
     "state": {"child": child("c", steer="notcallable")}})  # not callable -> return
add("_warn_child_budget", {"idle_seconds": 10.0, "child_timeout": 30.0,
     "state": {"child": child("c")}})  # no steer
add("_warn_child_budget", {"idle_seconds": "x", "child_timeout": 30.0,
     "state": {"child": child("c", steer=cb("child.steer"))},
     "io": {"logger.debug": None}})  # budget text ValueError -> caught

# ---- _child_last_event_age (8) ----
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"last_activity_ts": 90.0}, "time.time": 100.0}})
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"last_activity_ts": None}, "time.time": 100.0}})
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": None}})  # or {} -> ts None
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": rs("RuntimeError", "boom")}})  # -> None
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": rs("GeneratorExit", "")}})  # passes
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"last_activity_ts": "95.5"}, "time.time": 100.0}})
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"last_activity_ts": [1]}, "time.time": 100.0}})  # float TypeError -> None
add("_child_last_event_age", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"last_activity_ts": 200.0}, "time.time": 100.0}})  # max(0,..) -> 0

# ---- _merge_late_steer (6) ----
add("_merge_late_steer", {"subagent_id": "s1", "state": {"result": {"pending_steer": "old"}, "child": child("c")},
     "io": {"_close_subagent_steering": "late"}})
add("_merge_late_steer", {"subagent_id": "s1", "state": {"result": {}, "child": child("c")},
     "io": {"_close_subagent_steering": "late"}})
add("_merge_late_steer", {"subagent_id": None, "state": {"result": {}, "child": child("c")}})  # no seam
add("_merge_late_steer", {"subagent_id": "s1", "state": {"result": {}, "child": child("c")},
     "io": {"_close_subagent_steering": None}})  # late falsy
add("_merge_late_steer", {"subagent_id": "s1", "state": {"result": {"pending_steer": 5}, "child": child("c")},
     "io": {"_close_subagent_steering": "late"}})  # existing non-str
add("_merge_late_steer", {"subagent_id": "s1", "state": {"result": {}, "child": child("c")},
     "io": {"_close_subagent_steering": rs("RuntimeError", "boom")}})

# ---- _reserve_child_transcript (8) ----
def rchild(**extra):
    base = dict(_session_db={"db": 1}, _persist_disabled=False, _parent_session_id="ps1",
                session_id="sid1", persistence=agent(sessions=agent(create=cb("child.persistence.sessions.create"))),
                close=cb("child.close"))
    base.update(extra)
    return child("c", **base)


add("_reserve_child_transcript", {"state": {"child": child("c", _session_db=None)}})  # db None
add("_reserve_child_transcript", {"state": {"child": child("c", _session_db={"db": 1}, _persist_disabled=True)}})
add("_reserve_child_transcript", {"state": {"child": child("c", _session_db={"__attr__": {}, "__class__": "RuntimeSessionStore"}, _persist_disabled=False)}})
add("_reserve_child_transcript", {"state": {"child": rchild()},
     "io": {"NewSession": {"__new_session__": True}, "child.persistence.sessions.create": None}})
add("_reserve_child_transcript", {"state": {"child": rchild()},
     "io": {"NewSession": {"__new_session__": True}, "child.persistence.sessions.create": rs("RuntimeError", "boom"), "child.close": None}})
add("_reserve_child_transcript", {"state": {"child": rchild()},
     "io": {"NewSession": {"__new_session__": True}, "child.persistence.sessions.create": rs("SystemExit", "1"), "child.close": None}})
add("_reserve_child_transcript", {"state": {"child": child("c", _session_db={"db": 1}, _persist_disabled=False, _parent_session_id="ps1", close=cb("child.close"))},
     "io": {"child.close": None}})  # missing persistence -> AttributeError + close
add("_reserve_child_transcript", {"state": {"child": rchild()},
     "io": {"NewSession": {"__new_session__": True}, "child.persistence.sessions.create": rs("RuntimeError", "boom"), "child.close": rs("ValueError", "closefail")}})

# ---- _register_child (8) ----
def regchild(**extra):
    base = dict(_subagent_id="s1", _parent_subagent_id="p1", _delegate_depth=2,
                _delegation_id="d1", model="m1", _parent_session_id="ps1")
    base.update(extra)
    return child("c", **base)


add("_register_child", {"goal": "g", "state": {"child": child("c"), "parent_agent": agent()}})  # no sid
add("_register_child", {"goal": "g", "state": {"child": child("c", _subagent_id=5), "parent_agent": agent()}})  # non-str
add("_register_child", {"goal": "g", "owner_session_id": "os1", "owner_transport": "t1", "owner_session_record": "r1",
     "state": {"child": regchild(), "parent_agent": agent(session_id="ss1")},
     "io": {"_str_or_none": ["p1", "d1", "m1"], "time.time": 100.0, "_register_subagent": None}})
add("_register_child", {"goal": "g", "state": {"child": regchild(), "parent_agent": agent(session_id="ss1")},
     "io": {"quiet": None, "get_session_env": "env_sid", "_capture_gateway_steer_authority": wrap(["t", "r"]),
            "_str_or_none": ["p1", "d1", "m1"], "time.time": 100.0, "_register_subagent": None}})
add("_register_child", {"goal": "g", "owner_session_id": "os1",
     "state": {"child": regchild(_delegate_depth=True), "parent_agent": agent()},
     "io": {"_capture_gateway_steer_authority": wrap([None, None]),
            "_str_or_none": [None, None, None], "time.time": 100.0, "_register_subagent": None}})
add("_register_child", {"goal": "g", "owner_session_id": "os1", "owner_transport": "t1", "owner_session_record": "r1",
     "state": {"child": regchild(_delegate_depth="x"), "parent_agent": agent()},
     "io": {"_str_or_none": ["p1", "d1", "m1"], "time.time": 100.0, "_register_subagent": rs("RuntimeError", "boom")}})
add("_register_child", {"goal": "g", "owner_session_id": "os1", "owner_transport": "t1", "owner_session_record": "r1",
     "state": {"child": regchild(), "parent_agent": agent()},
     "io": {"_str_or_none": [rs("ValueError", "bad")]}})
add("_register_child", {"goal": "g", "state": {"child": regchild(), "parent_agent": agent()},
     "io": {"quiet": None, "get_session_env": "", "_str_or_none": ["p1", "d1", "m1"], "time.time": 100.0, "_register_subagent": None}})

# ---- _create_isolated_worktree (7) ----
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()}, "io": {"_get_worktree_isolation": False}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": None, "local_backend_active": False, "logger.debug": None}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": [None, None], "local_backend_active": True,
            "get_session_cwd": "/parent", "create_subagent_worktree": {"path": "/wt"}}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": [None, None], "local_backend_active": True,
            "get_session_cwd": rs("RuntimeError", "boom"), "_resolve_workspace_hint": "/hint",
            "create_subagent_worktree": {"path": "/wt"}}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": [None, None], "local_backend_active": True,
            "get_session_cwd": "/parent", "create_subagent_worktree": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": "s1",
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": [None, None], "local_backend_active": True,
            "get_session_cwd": "/parent", "create_subagent_worktree": rs("SystemExit", "1")}})
add("_create_isolated_worktree", {"parent_task_id": "pt1", "subagent_id": None,
     "state": {"parent_agent": agent()},
     "io": {"_get_worktree_isolation": True, "quiet": [None, None], "local_backend_active": True,
            "get_session_cwd": None, "_resolve_workspace_hint": None, "create_subagent_worktree": None}})

# ---- _lease_child_credential (8) ----
def pool_child(entries_script, **extra):
    pool = agent(acquire_lease=cb("pool.acquire_lease"), entries=cb("pool.entries"),
                 release_lease=cb("pool.release_lease"))
    base = dict(_credential_pool=pool, base_url="http://ep", _swap_credential=cb("child._swap_credential"))
    base.update(extra)
    return child("c", **base)


E1 = {"__attr__": {"id": "e1", "last_status": "ok"}, "__id__": "e1"}
E2 = {"__attr__": {"id": "e2", "last_status": "dead"}, "__id__": "e2"}
E3 = {"__attr__": {"id": "e3", "last_status": "ok"}, "__id__": "e3"}
add("_lease_child_credential", {"state": {"child": child("c")}})  # no pool
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": None, "pool.acquire_lease": None}})
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": None, "pool.acquire_lease": "e1", "quiet": None,
            "pool.entries": wrap([E1]), "credential_pool_entry_serves_endpoint": True, "child._swap_credential": None}})
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": None, "pool.acquire_lease": ["e1", "e3"], "quiet": None,
            "pool.entries": [wrap([E1]), wrap([E2, E3])],
            "credential_pool_entry_serves_endpoint": [False, True],
            "pool.release_lease": None, "child._swap_credential": None}})
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": None, "pool.acquire_lease": "e1", "quiet": None,
            "pool.entries": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": None, "pool.acquire_lease": rs("SystemExit", "1")}})
add("_lease_child_credential", {"state": {"child": pool_child(None)},
     "io": {"child_profile_scope": rs("RuntimeError", "scopefail")}})
add("_lease_child_credential", {"state": {"child": pool_child(None, _swap_credential="notcallable")},
     "io": {"child_profile_scope": None, "pool.acquire_lease": "e1", "quiet": None,
            "pool.entries": wrap([E1]), "credential_pool_entry_serves_endpoint": True, "logger.debug": None}})

# ---- _build_tool_trace (8) ----
add("_build_tool_trace", {"messages": "notlist"})
add("_build_tool_trace", {"messages": [
     {"role": "assistant", "tool_calls": [{"id": "t1", "function": {"name": "terminal", "arguments": "{\"cmd\":\"ls\"}"}}]},
     {"role": "tool", "tool_call_id": "t1", "content": "ok"}],
     "io": {"_summarize_tool_arguments": {"cmd": "ls"}, "_stringify_tool_content": "ok", "_looks_like_error_output": False}})
add("_build_tool_trace", {"messages": [
     {"role": "assistant", "tool_calls": [{"function": {"name": "read", "arguments": "x"}}]},
     {"role": "tool", "content": "Error: bad"}],
     "io": {"_summarize_tool_arguments": {}, "_stringify_tool_content": "Error: bad", "_looks_like_error_output": True}})
add("_build_tool_trace", {"messages": [
     {"role": "assistant", "tool_calls": [{"id": "a", "function": {"name": "f1", "arguments": ""}},
                                           {"id": "b", "function": {"name": "f2", "arguments": "yy"}}]},
     {"role": "tool", "tool_call_id": "b", "content": "r2"},
     {"role": "tool", "tool_call_id": "a", "content": "r1"}],
     "io": {"_summarize_tool_arguments": [{}, {}], "_stringify_tool_content": ["r2", "r1"], "_looks_like_error_output": [False, False]}})
add("_build_tool_trace", {"messages": [
     {"role": "assistant", "tool_calls": [{"id": "t1", "function": {"name": "f", "arguments": "a"}}]},
     "notdict",
     {"role": "tool", "tool_call_id": "t1", "content": "c"}],
     "io": {"_summarize_tool_arguments": {}, "_stringify_tool_content": "c", "_looks_like_error_output": False}})
add("_build_tool_trace", {"messages": [
     {"role": "assistant", "tool_calls": [{"id": "t1", "function": {"name": "f", "arguments": "a"}}]}],
     "io": {"_summarize_tool_arguments": rs("RuntimeError", "boom")}})
add("_build_tool_trace", {"messages": [{"role": "tool", "content": "c"}],
     "io": {"_stringify_tool_content": rs("ValueError", "bad")}})
add("_build_tool_trace", {"messages": []})

# ---- _validate_child_output_schema (8) ----
def vchild(**extra):
    base = dict(_delegate_output_schema={"type": "object"}, session_id="sid",
                run_conversation=cb("child.run_conversation"))
    base.update(extra)
    return child("c", **base)


add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": child("c"), "result": {}}})  # no schema
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "{\"a\":1}"}},
     "io": {"validate_output": wrap([True, []])}})
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": ""}},
     "io": {"validate_output": wrap([False, ["missing"]])}})  # empty text -> 0 retries
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "x", "interrupted": True}},
     "io": {"validate_output": wrap([False, ["e"]])}})
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "bad", "messages": [{"role": "user", "content": "u"}]}},
     "io": {"validate_output": [wrap([False, ["e1"]]), wrap([True, []])],
            "child_profile_scope": None, "delegated_child_context": None,
            "build_retry_message": "retry msg", "append_output_contract": "contract msg",
            "child.run_conversation": {"final_response": "good", "api_calls": 2,
                                        "messages": [{"role": "user", "content": "r"}, {"role": "assistant", "content": "good"}],
                                        "current_turn_user_idx": 0}}})
add("_validate_child_output_schema", {"task_index": 1, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "bad"}},
     "io": {"validate_output": wrap([False, ["e1"]]),
            "child_profile_scope": None, "delegated_child_context": None,
            "build_retry_message": "rm", "append_output_contract": "cm",
            "child.run_conversation": rs("RuntimeError", "retryfail"), "logger.warning": None}})
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "bad"}},
     "io": {"validate_output": wrap([False, ["e1"]]),
            "child_profile_scope": None, "delegated_child_context": None,
            "build_retry_message": "rm", "append_output_contract": "cm",
            "child.run_conversation": rs("SystemExit", "1")}})  # passes
add("_validate_child_output_schema", {"task_index": 0, "child_task_id": "ct",
     "state": {"child": vchild(), "result": {"final_response": "bad", "api_calls": 1,
                                              "messages": [{"role": "user", "content": "u"}]}},
     "io": {"validate_output": [wrap([False, ["e1"]]), wrap([False, ["e2"]])],
            "child_profile_scope": None, "delegated_child_context": None,
            "build_retry_message": "rm", "append_output_contract": "cm",
            "child.run_conversation": {"final_response": "still bad", "api_calls": 1,
                                        "messages": [{"role": "assistant", "content": "x"}, {"role": "user", "content": "r"}]}}})

# ---- _build_result_entry (9) ----
def bchild(**extra):
    base = dict(model="m1", session_prompt_tokens=10, session_completion_tokens=20,
                session_estimated_cost_usd=0.123456789, session_cost_status="estimated", _delegate_role="worker")
    base.update(extra)
    return child("c", **base)


SCHEMA_NONE = {"schema": None, "valid": None, "errors": [], "retries": 0}
add("_build_result_entry", {"task_index": 0, "duration": 1.5, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "done", "completed": True, "api_calls": 3}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None,
            "_summarize_tool_arguments": [], "_stringify_tool_content": [], "_looks_like_error_output": []}})
add("_build_result_entry", {"task_index": 1, "duration": 2.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "err text", "failed": True, "error": "boom",
                                              "failure_reason": "rate_limit", "failure_resets_at": 123.5,
                                              "usage_limit": {"kind": "quota"}}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "", "completed": False}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None}})  # max_iterations, failed (no summary)
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "Operation interrupted.", "interrupted": True,
                                              "messages": [{"role": "assistant", "content": "real partial"}]}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None, "flatten_message_text": "real partial"}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "Operation interrupted.", "interrupted": True,
                                              "_stale_stop_seconds": 450.0, "messages": []}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0,
     "schema": {"schema": {"type": "object"}, "valid": False, "errors": ["missing field"], "retries": 1},
     "state": {"child": bchild(), "result": {"final_response": "raw text", "completed": True}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "done", "completed": True, "pending_steer": "go left"}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": None}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(_delegate_route={"backend": "x"}), "result": {"final_response": "done", "completed": True}},
     "io": {"_str_or_none": "m1", "_num": [10, 20], "_route_report": {"backend": "x", "service_tier": "normal"}}})
add("_build_result_entry", {"task_index": 0, "duration": 1.0, "schema": SCHEMA_NONE,
     "state": {"child": bchild(), "result": {"final_response": "done", "completed": True}},
     "io": {"_str_or_none": "m1", "_num": [rs("ValueError", "bad")]}})

# ---- _build_child_goal_message (8) ----
add("_build_child_goal_message", {"goal": "do it", "images": [], "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": "native",
            "build_native_content_parts": wrap([[], []])}})
add("_build_child_goal_message", {"goal": "do it", "images": ["http://x/a.png", "/local/b.png"],
     "state": {"child": child("c", provider="p", model="m")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": "native",
            "build_native_content_parts": wrap([[{"type": "text", "text": "do it"},
                                                  {"type": "image_url", "image_url": {"url": "http://x/a.png"}}], []])}})
add("_build_child_goal_message", {"goal": "do it", "images": ["data:image/png;base64,AAA", "http://x/a.png"],
     "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": "native",
            "build_native_content_parts": wrap([None, []])}})
add("_build_child_goal_message", {"goal": "do it", "images": ["/local/b.png", "http://x/a.png"],
     "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": "hint",
            "os.path.isfile": True}})
add("_build_child_goal_message", {"goal": "do it", "images": ["/missing.png"],
     "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": "hint",
            "os.path.isfile": False, "logger.warning": None}})
add("_build_child_goal_message", {"goal": "do it", "images": ["http://x/a.png"],
     "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": rs("RuntimeError", "cfgfail"),
            "decide_image_input_mode": "hint"}})
add("_build_child_goal_message", {"goal": "do it", "images": ["http://x/a.png"],
     "state": {"child": child("c")},
     "io": {"quiet": None, "load_config_readonly": {}, "decide_image_input_mode": rs("RuntimeError", "boom"),
            "logger.warning": None}})
add("_build_child_goal_message", {"goal": "do it", "images": [5],
     "state": {"child": child("c")}, "io": {"logger.warning": None}})  # non-str -> except -> goal

# ---- _checkpointing (7) ----
add("_checkpointing", {"inner": cb("inner.cb"), "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.completed", "args": [], "kwargs": {}}],
     "io": {"inner.cb": None, "quiet": None, "checkpoint.cb": None}})
add("_checkpointing", {"inner": cb("inner.cb"), "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.started", "args": ["a"], "kwargs": {"k": 1}}],
     "io": {"inner.cb": None}})  # non-tool event: no checkpoint
add("_checkpointing", {"inner": cb("inner.cb"), "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.completed"}],
     "io": {"inner.cb": rs("RuntimeError", "innerfail")}})  # inner raise propagates
add("_checkpointing", {"inner": cb("inner.cb"), "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.completed"}],
     "io": {"inner.cb": None, "quiet": None, "checkpoint.cb": rs("RuntimeError", "cpfail"), "logger.debug": None}})
add("_checkpointing", {"inner": cb("inner.cb"), "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.completed"}],
     "io": {"inner.cb": None, "quiet": None, "checkpoint.cb": rs("SystemExit", "1")}})
add("_checkpointing", {"inner": {"__callable__": "inner.cb", "_flush": cb("inner.flush")},
     "checkpoint": cb("checkpoint.cb"),
     "events": [{"event_type": "tool.completed"}],
     "io": {"inner.cb": None, "quiet": None, "checkpoint.cb": None}})
add("_checkpointing", {"inner": None, "checkpoint": "notcallable",
     "events": [{"event_type": "tool.completed"}],
     "io": {"quiet": None, "logger.debug": None}})  # checkpoint not callable -> swallowed


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(cnt), OUT))
    if short:
        print("WARNING funcs with <5 cases:", short)
    missing = set(FUNCS_EXPECTED) - set(cnt)
    if missing:
        print("WARNING missing funcs:", missing)


FUNCS_EXPECTED = [
    "_reserve_child_transcript", "_format_thread_stack", "_diag_section", "_diag_sizes",
    "_budget_warning_text", "_warn_child_budget", "_child_last_event_age",
    "_register_child", "_create_isolated_worktree", "_lease_child_credential",
    "_merge_late_steer", "_validate_child_output_schema", "_build_tool_trace",
    "_build_result_entry", "_is_image_url", "_build_child_goal_message", "_checkpointing",
]

if __name__ == "__main__":
    main()