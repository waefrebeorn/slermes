#!/usr/bin/env python3
"""Generate tests/fixtures/relay_shared_metrics_flow2.jsonl — the scripted-seam
case corpus for the relay_shared_metrics_flow2 FLOW port. >=5 cases per ported
func, covering happy paths, branch gates, scripted Exception (swallowed by
guards), scripted BaseException (passthrough), and fail-closed missing scripts.

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script},
"state": {...mutable carrier...}}}. A bare JSON array script is a per-call
queue; a list RESULT is wrapped {"__script__": true, "ret": [...]}; a raise is
{"__raise__": kind, "message": msg}. Deterministic (no randomness).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_shared_metrics_flow2.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def seq(*vals):
    return list(vals)


def listret(arr):
    return {"__script__": True, "ret": arr}


SESS = {"session_id": "s1", "ref": "S1", "relay_session": {"handle": "H"},
        "closing": False, "tasks": {}}

# ── _text ──
add("_text", event={"k": "hello"}, key="k")
add("_text", event={"k": ""}, key="k")
add("_text", event={"k": None}, key="k")
add("_text", event={"k": 0}, key="k")
add("_text", event={"k": 5}, key="k")
add("_text", event={"k": False}, key="k")
add("_text", event={"k": "x"}, key="missing")
add("_text", event={"k": 7.5}, key="k")

# ── _elapsed_ms ──
add("_elapsed_ms", started_ns=1000000, io={"monotonic_ns": 5000000})
add("_elapsed_ms", started_ns=5000000, io={"monotonic_ns": 1000000})   # negative -> 0
add("_elapsed_ms", started_ns=0, io={"monotonic_ns": 0})
add("_elapsed_ms", started_ns=1500000, io={"monotonic_ns": 1000000})   # floor negative
add("_elapsed_ms", started_ns=0, io={"monotonic_ns": raise_("RuntimeError")})
add("_elapsed_ms", started_ns=0, io={})   # fail-closed monotonic_ns

# ── _scope_handle ──
add("_scope_handle", session=SESS, task={"handle": "TH"})
add("_scope_handle", session=SESS, task=None)
add("_scope_handle", session=SESS, task={"handle": None})
add("_scope_handle", session={"session_id": "s", "relay_session": {"handle": None}}, task=None)
add("_scope_handle", session=SESS, task={"handle": {"h": 1}})

# ── _sole ──
add("_sole", items=[{"ref": "A", "v": 1}])
add("_sole", items=[{"ref": "A", "v": 1}, {"ref": "A", "v": 2}])   # same identity
add("_sole", items=[{"ref": "A"}, {"ref": "B"}])                    # two identities
add("_sole", items=[])
add("_sole", items=[{"ref": "A"}, {"ref": "A"}, {"ref": "B"}])
add("_sole", items=[{"ref": "Z", "v": 9}])

# ── _forget ──
add("_forget", key="s1", owner={"ref": "S1"}, state={"index": {"s1": {"ref": "S1"}}})
add("_forget", key="s1", owner={"ref": "S1"}, state={"index": {"s1": {"ref": "OTHER"}}})
add("_forget", key="s1", owner={"ref": "S1"}, state={"index": {}})
add("_forget", key="s1", owner={"ref": "S1"}, state={"index": {"s1": {"ref": "S1"}, "s2": {"ref": "S2"}}})
add("_forget", key="missing", owner={"ref": "S1"}, state={"index": {"s1": {"ref": "S1"}}})

# ── _task_parent_handle ──
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": None})
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": {"lease": {"session_id": "s1"}, "task_id": "t1", "handle": "AH"}})
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": {"lease": {"session_id": "s1"}, "task_id": "OTHER", "handle": "AH"}})
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": {"lease": {"session_id": "OTHER"}, "task_id": "t1", "handle": "AH"}})
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": {"lease": {"session_id": "s1"}, "task_id": "t1", "handle": None}})
add("_task_parent_handle", session=SESS, task_id="t1",
    io={"relay_runtime.active_turn": raise_("RuntimeError")})

# ── _with_launch_entrypoint ──
add("_with_launch_entrypoint", event={"entrypoint": "repl"}, io={})
add("_with_launch_entrypoint", event={"parent_task_id": "p"}, io={})
add("_with_launch_entrypoint", event={}, io={"contract.execution_surface": "gateway"})
add("_with_launch_entrypoint", event={},
    io={"contract.execution_surface": "cli", "is_single_query_session": False})
add("_with_launch_entrypoint", event={"session_id": "s"},
    io={"contract.execution_surface": "cli", "is_single_query_session": True,
        "get_session_env": "a2a"})
add("_with_launch_entrypoint", event={"session_id": "s"},
    io={"contract.execution_surface": "cli", "is_single_query_session": True,
        "get_session_env": " Kanban "})
add("_with_launch_entrypoint", event={"session_id": "s"},
    io={"contract.execution_surface": "cli", "is_single_query_session": True,
        "get_session_env": "other"})
add("_with_launch_entrypoint", event={},
    io={"contract.execution_surface": "cli", "is_single_query_session": True,
        "get_session_env": None})
add("_with_launch_entrypoint", event={}, io={"contract.execution_surface": raise_("ValueError")})

# ── _cold_key ──
add("_cold_key", session_id="s1", io={"get_conversation_context": "conv"})
add("_cold_key", session_id="s1", io={"get_conversation_context": ""})
add("_cold_key", session_id="s1", io={"get_conversation_context": None})
add("_cold_key", session_id="", io={"get_conversation_context": None})
add("_cold_key", session_id="s1", io={"get_conversation_context": raise_("RuntimeError")})
add("_cold_key", session_id="s1", io={})

# ── _process_marks_admitted ──
add("_process_marks_admitted", io={"relay_runtime.current_turn": None})
add("_process_marks_admitted", io={"relay_runtime.current_turn": {"relay_enabled": True}})
add("_process_marks_admitted", io={"relay_runtime.current_turn": {"relay_enabled": False}})
add("_process_marks_admitted", io={"relay_runtime.current_turn": raise_("RuntimeError")})
add("_process_marks_admitted", io={})

# ── _raw_config ──
add("_raw_config", io={"read_raw_config_readonly": {"telemetry": {"enabled": True}}})
add("_raw_config", io={"read_raw_config_readonly": None})
add("_raw_config", io={"read_raw_config_readonly": {}})
add("_raw_config", io={"read_raw_config_readonly": raise_("ValueError")})
add("_raw_config", io={})

# ── _resolved_send_config ──
add("_resolved_send_config",
    io={"read_raw_config_readonly": {"a": 1}, "resolve_send_config": {"send": True, "endpoint": "e"}})
add("_resolved_send_config",
    io={"read_raw_config_readonly": None, "resolve_send_config": {"send": False}})
add("_resolved_send_config", io={"read_raw_config_readonly": raise_("ValueError")})
add("_resolved_send_config",
    io={"read_raw_config_readonly": {}, "resolve_send_config": raise_("RuntimeError")})
add("_resolved_send_config", io={"read_raw_config_readonly": {}})   # fail-closed resolve

# ── _guarded ──
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[1], kwargs={"x": 2},
    io={"cb": 42})
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"cb": None})
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"cb": raise_("ValueError"), "logger.warning": None})
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"cb": raise_("SystemExit", "bye")})           # BaseException passthrough
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"cb": raise_("KeyboardInterrupt")})
add("_guarded", message="m", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"cb": raise_("ValueError")})                  # fail-closed logger.warning
add("_guarded", message="m", callback={"__callable__": "cb"}, io={})  # fail-closed cb

# ── _with_scope_stack ──
add("_with_scope_stack", callback={"__callable__": "cb"}, args=[1], kwargs={"k": "v"},
    io={"relay.get_scope_stack": None, "cb": "R"})
add("_with_scope_stack", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"relay.get_scope_stack": None, "cb": None})
add("_with_scope_stack", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"relay.get_scope_stack": raise_("RuntimeError")})
add("_with_scope_stack", callback={"__callable__": "cb"}, args=[], kwargs={},
    io={"relay.get_scope_stack": None, "cb": raise_("SystemExit")})
add("_with_scope_stack", callback={"__callable__": "cb"}, io={"relay.get_scope_stack": None})

# ── _emit_rows ──
add("_emit_rows", session=None, rows=[["hermes.a", {"x": "1"}]],
    io={"rt.record_process_mark": None})
add("_emit_rows", session=SESS, rows=[["hermes.a", {"x": "1"}], ["hermes.b", {"y": "2"}]],
    io={"rt._mark": None})
add("_emit_rows", session=None, rows=[], io={})
add("_emit_rows", session=SESS, rows=[["hermes.a", {}]],
    io={"rt._mark": raise_("ValueError"), "logger.warning": None})
add("_emit_rows", session=None, rows=[["hermes.a", {}]],
    io={"rt.record_process_mark": raise_("GeneratorExit")})   # passthrough
add("_emit_rows", session=SESS, rows=[["hermes.a", {}]], io={})   # fail-closed rt._mark

# ── _any_session ──
add("_any_session", event={"session_id": "s1"}, io={"rt._task_session": SESS})
add("_any_session", event={"session_id": "s1"}, io={"rt._task_session": None, "rt._session": SESS})
add("_any_session", event={"session_id": "s1"}, io={"rt._task_session": None, "rt._session": None})
add("_any_session", event={"session_id": "s1"}, io={"rt._task_session": raise_("RuntimeError")})
add("_any_session", event={"session_id": "s1"}, io={"rt._task_session": None})  # fail-closed _session

# ── _task_pair ──
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=False,
    io={"rt._task_session": {"session_id": "s1", "tasks": {"t1": {"task_id": "t1", "ref": "T1"}}}})
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=False,
    io={"rt._task_session": {"session_id": "s1", "tasks": {}}})
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=False,
    io={"rt._task_session": None})
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=True, lookup={"allow_task_id_fallback": True},
    io={"rt._task_session": seq({"session_id": "s1", "tasks": {}}, {"session_id": "s1", "tasks": {"t1": {"task_id": "t1"}}}),
        "rt.start_task": {"task_id": "t1", "cost": {"user_turn": False}}})
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=True,
    io={"rt._task_session": {"session_id": "s1", "tasks": {}}, "rt.start_task": None})
add("_task_pair", event={"session_id": "s1", "task_id": "t1"}, start=True,
    io={"rt._task_session": {"session_id": "s1", "tasks": {}}, "rt.start_task": raise_("RuntimeError")})

# ── _abort_session ──
add("_abort_session", base_event={"session_id": "s1"},
    state={"session": {"session_id": "s1", "closing": False, "tasks": {}}},
    io={"rt._end_pending_model_calls": None})
add("_abort_session", base_event={"session_id": "s1"},
    state={"session": {"session_id": "s1", "closing": True, "tasks": {}}}, io={})
add("_abort_session", base_event={"session_id": "s1", "status": "x"},
    state={"session": {"session_id": "s1", "closing": False, "tasks": {"t1": {}, "t2": {}}}},
    io={"rt._finish_task": None, "rt._end_pending_model_calls": None})
add("_abort_session", base_event={"session_id": "s1"},
    state={"session": {"session_id": "s1", "closing": False, "tasks": {"t1": {}}}},
    io={"rt._finish_task": raise_("ValueError")})
add("_abort_session", base_event={"session_id": "s1"},
    state={"session": {"session_id": "s1", "closing": False, "tasks": {}}},
    io={"rt._end_pending_model_calls": raise_("SystemExit")})
add("_abort_session", base_event={"session_id": "s1"},
    state={"session": {"session_id": "s1", "closing": False, "tasks": {"t1": {}}}},
    io={"rt._finish_task": None})   # fail-closed end_pending

# ── start_user_turn ──
add("start_user_turn", event={"session_id": "s1"},
    io={"rt.start_task": {"task_id": "t1", "cost": {"user_turn": False}}}, state={})
add("start_user_turn", event={"session_id": "s1"}, io={"rt.start_task": None}, state={})
add("start_user_turn", event={"session_id": "s1"},
    io={"rt.start_task": {"task_id": "t1", "cost": {"user_turn": True}}}, state={})
add("start_user_turn", event={"session_id": "s1"}, io={"rt.start_task": raise_("RuntimeError")}, state={})
add("start_user_turn", event={"session_id": "s1"}, io={}, state={})

# ── _deregister ──
add("_deregister", state={"_subscriber_name": "sub", "_registered": True, "id": "r1"},
    io={"rt._safe": None, "host.release_managed_execution": None})
add("_deregister", state={"_subscriber_name": "sub", "_registered": True, "id": "r1"},
    io={"rt._safe": raise_("ValueError")})
add("_deregister", state={"_subscriber_name": "sub", "_registered": True, "id": "r1"},
    io={"rt._safe": None, "host.release_managed_execution": raise_("SystemExit")})
add("_deregister", state={"_subscriber_name": "sub", "_registered": False, "id": "r1"},
    io={"rt._safe": None, "host.release_managed_execution": None})
add("_deregister", state={"_subscriber_name": "sub", "_registered": True, "id": "r1"},
    io={"rt._safe": None})   # fail-closed release

# ── _join_send_thread ──
add("_join_send_thread", timeout=2.0, state={"_send_thread": None}, io={})
add("_join_send_thread", timeout=1.5, state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True, "thread.join": None})
add("_join_send_thread", timeout=2.0, state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": False})
add("_join_send_thread", timeout=2.0, state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True, "thread.join": raise_("ValueError"), "logger.debug": None})
add("_join_send_thread", timeout=2.0, state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True, "thread.join": raise_("KeyboardInterrupt")})
add("_join_send_thread", timeout=2.0, state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True})   # fail-closed join

# ── _release ──
add("_release", state={"_send_thread": None}, io={"atexit.unregister": None})
add("_release", state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True, "thread.join": None, "atexit.unregister": None})
add("_release", state={"_send_thread": None}, io={"atexit.unregister": raise_("ValueError")})
add("_release", state={"_send_thread": None}, io={"atexit.unregister": raise_("SystemExit")})
add("_release", state={"_send_thread": {"__thread__": True}},
    io={"thread.is_alive": True, "thread.join": raise_("GeneratorExit")})
add("_release", state={"_send_thread": None}, io={})   # fail-closed atexit

# ── _lineage_spent ──
add("_lineage_spent", session_id="s1",
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": {"spent": ["t1", "t2"]}}})
add("_lineage_spent", session_id="s1", state={"_lineage_of": {}, "_lineages": {}})
add("_lineage_spent", session_id="s1",
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": {"spent": None}}})
add("_lineage_spent", session_id="s1",
    state={"_lineage_of": {"s1": "root"}, "_lineages": {}})
add("_lineage_spent", session_id="s2",
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": {"spent": ["t1"]}}})

# ── _close_child_session ──
add("_close_child_session", kwargs={"child_session_id": "c1"}, io={"rt.close_session": None})
add("_close_child_session", kwargs={"child_session_id": ""}, io={})
add("_close_child_session", kwargs={}, io={})
add("_close_child_session", kwargs={"child_session_id": "c1"}, io={"rt.close_session": raise_("RuntimeError")})
add("_close_child_session", kwargs={"child_session_id": "c1"}, io={})   # fail-closed
add("_close_child_session", kwargs={"child_session_id": 0}, io={})

# ── _run_task_hook ──
add("_run_task_hook", method="start_task", retry_failed=True, event={"session_id": "s1"},
    io={"enabled": True, "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": None})
add("_run_task_hook", method="finish_task", event={"session_id": "s1"},
    io={"enabled": False})
add("_run_task_hook", method="start_task", event={},
    io={"enabled": True, "_get_runtime": None})
add("_run_task_hook", method="start_task", event={},
    io={"enabled": True, "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": raise_("ValueError")})
add("_run_task_hook", method="start_task", event={}, io={"enabled": raise_("SystemExit")})
add("_run_task_hook", method="start_task", event={}, io={"enabled": True})   # fail-closed _get_runtime

# ── record_process_mark (module) ──
add("record_process_mark", mark="hermes.x", data={"a": "1"},
    io={"enabled": True, "relay_runtime.current_turn": None,
        "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": None})
add("record_process_mark", mark="hermes.x", data={}, io={"enabled": False})
add("record_process_mark", mark="hermes.x", data={},
    io={"enabled": True, "relay_runtime.current_turn": {"relay_enabled": False}})
add("record_process_mark", mark="hermes.x", data={},
    io={"enabled": True, "relay_runtime.current_turn": None, "_get_runtime": None})
add("record_process_mark", mark="hermes.x", data={},
    io={"enabled": True, "relay_runtime.current_turn": None,
        "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": raise_("RuntimeError")})
add("record_process_mark", mark="hermes.x", data={}, io={"enabled": True})  # fail-closed current_turn

# ── record_process_marks_saved (module) ──
add("record_process_marks_saved", marks=[["hermes.x", {"a": "1"}]],
    io={"enabled": True, "relay_runtime.current_turn": None,
        "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": 1})
add("record_process_marks_saved", marks=[["a", {}], ["b", {}]], io={"enabled": False})
add("record_process_marks_saved", marks=[],
    io={"enabled": True, "relay_runtime.current_turn": None, "_get_runtime": None})
add("record_process_marks_saved", marks=[["a", {}]],
    io={"enabled": True, "relay_runtime.current_turn": None,
        "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": None})
add("record_process_marks_saved", marks=[["a", {}]],
    io={"enabled": True, "relay_runtime.current_turn": {"relay_enabled": True},
        "_get_runtime": {"__runtime__": True, "id": "r1"}, "rt._safe": raise_("ValueError")})
add("record_process_marks_saved", marks=[["a", {}]], io={"enabled": True})  # fail-closed

# ── close_session_run ──
add("close_session_run", session_id="s1",
    state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1", "rt._safe": None})
add("close_session_run", session_id="", io={})
add("close_session_run", session_id="s1", io={"enabled": False})
add("close_session_run", session_id="s1", state={"_RUNTIMES": {}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1"})
add("close_session_run", session_id="s1", state={"_RUNTIMES": {"p1": {"__failed__": True}}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1"})
add("close_session_run", session_id="s1",
    state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1", "rt._safe": raise_("SystemExit")})

# ── rotate_segment (module) ──
add("rotate_segment", old_session_id="s1", new_session_id="s2",
    state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1", "rt._safe": None})
add("rotate_segment", old_session_id="", new_session_id="s2", io={})
add("rotate_segment", old_session_id="s1", new_session_id="s1", io={})
add("rotate_segment", old_session_id="s1", new_session_id="s2", io={"enabled": False})
add("rotate_segment", old_session_id="s1", new_session_id="s2", state={"_RUNTIMES": {}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1"})
add("rotate_segment", old_session_id="s1", new_session_id="s2",
    state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"enabled": True, "relay_runtime.current_profile_key": "p1", "rt._safe": raise_("ValueError")})

# ── shutdown_runtimes ──
add("shutdown_runtimes", state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"rt._safe": None})
add("shutdown_runtimes", state={"_RUNTIMES": {}}, io={})
add("shutdown_runtimes",
    state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}, "p2": {"__runtime__": True, "id": "r2"}}},
    io={"rt._safe": None})
add("shutdown_runtimes", state={"_RUNTIMES": {"p1": {"__failed__": True}}}, io={})
add("shutdown_runtimes", state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}},
    io={"rt._safe": raise_("KeyboardInterrupt")})
add("shutdown_runtimes", state={"_RUNTIMES": {"p1": {"__runtime__": True, "id": "r1"}}}, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count check (>=5)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("WARNING funcs with <5 cases: %s" % short)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())