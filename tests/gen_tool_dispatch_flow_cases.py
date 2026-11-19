#!/usr/bin/env python3
"""Generate tests/fixtures/tool_dispatch_flow.jsonl — the fixture corpus for
the tool_dispatch_flow FLOW oracle (tools/delegate_tool_dispatch.py, 21
UNCLAIMED funcs; >=5 cases per func).

Case shape: {"func": <live name>, "in": {..., "io": {seam: script}, "state": {...}}}
Script semantics (shared with the C harness dispatcher): a single value is
reused for every call; a BARE array is a per-call queue; a list RESULT must be
wrapped {"__script__": true, "ret": [...]}; {"__raise__": kind, "message": m}
raises (real builtin classes; SystemExit/KeyboardInterrupt/GeneratorExit pass
every except Exception). Absent/exhausted scripts fail closed with a
RuntimeError "no script for <seam>" raise spec recorded BEFORE propagation.

Domain (see the oracle header): ASCII fixture text; no integral floats in
json.dumps-computed positions (total_duration scripted non-integral); entries
are plain dicts; interrupt-before-first-wait cases carry ONE pending future;
round(x,2) fixtures avoid exact half-way decimals.
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "tool_dispatch_flow.jsonl"


def child(cid, **extra):
    a = {"_subagent_id": cid, "_delegate_role": "worker", "model": "m-" + cid}
    a.update(extra)
    return {"__attr__": a}


def spinner():
    return {"__attr__": {"text": "idle"}}


def agent(**extra):
    a = {"session_id": "s1", "_delegate_spinner": spinner(),
         "_interrupt_requested": False, "_oneshot_session": False, "_delegate_depth": 0}
    a.update(extra)
    return {"__attr__": a}


def writer(path):
    return {"__attr__": {"path": path}}


def entry(i, status="completed", **extra):
    e = {"task_index": i, "status": status, "summary": "ok", "error": None,
         "api_calls": 1, "duration_seconds": 3}
    e.update(extra)
    return e


def batch(**over):
    b = {
        "task_list": [{"goal": "goal one"}, {"goal": "goal two"}],
        "children": [[0, {"goal": "goal one"}, child("c0")],
                     [1, {"goal": "goal two"}, child("c1")]],
        "parent_agent": agent(),
        "creds": {"model": "test-model"},
        "context": "ctx",
        "top_role": "delegate",
        "max_children": 2,
        "live_deleg_id": "d1",
        "live_writers": [writer("/tmp/t0.log"), writer("/tmp/t1.log")],
        "live_paths": ["/tmp/t0.log", "/tmp/t1.log"],
        "origin_wake_sid": "wake1",
        "origin_ui_session_id": "ui1",
        "origin_owner_transport": {"__attr__": {"kind": "tui"}},
        "origin_owner_session_record": {"__attr__": {"sid": "ui1"}},
        "origin_session_history_delivery": True,
        "overall_start": 10.0,
        "group": None,
        "unit_id": None,
        "live_home": None,
        "refused": [],
        "detached_only": False,
    }
    b.update(over)
    return b


def one_batch(**over):
    b = batch(**over)
    b["task_list"] = [{"goal": "solo goal"}]
    b["children"] = [[0, {"goal": "solo goal"}, child("c0")]]
    b["live_writers"] = [writer("/tmp/t0.log")]
    b["live_paths"] = ["/tmp/t0.log"]
    return b


def grouped_batch():
    b = batch()
    b["task_list"] = [{"goal": "ga", "group": "a"}, {"goal": "gb", "group": "a"},
                      {"goal": "gc", "group": "b"}, {"goal": "gd"}]
    b["children"] = [[0, b["task_list"][0], child("c0")],
                     [1, b["task_list"][1], child("c1")],
                     [2, b["task_list"][2], child("c2")],
                     [3, b["task_list"][3], child("c3")]]
    b["live_writers"] = [writer("/tmp/t%d.log" % i) for i in range(4)]
    b["live_paths"] = ["/tmp/t%d.log" % i for i in range(4)]
    return b


def fut(fid, done=True, result=None, cancelled=False):
    return {"id": fid, "done": done,
            "result": result if result is not None else entry(int(fid[1:])),
            "cancelled": cancelled}


def wrap(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(kind, msg):
    return {"__raise__": kind, "message": msg}


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


# ---- 1. _Batch.owner_kwargs (5) ----
add("_Batch.owner_kwargs", {"state": {"batch": batch()}})
add("_Batch.owner_kwargs", {"state": {"batch": batch(origin_ui_session_id="")}})
add("_Batch.owner_kwargs", {"state": {"batch": batch(origin_owner_transport=None,
                                                     origin_owner_session_record=None)}})
add("_Batch.owner_kwargs", {"state": {"batch": batch(origin_ui_session_id="x9",
                                                     origin_owner_transport={"__attr__": {"k": 1}})}})
add("_Batch.owner_kwargs", {"state": {"batch": batch(origin_ui_session_id="0")}})

# ---- 2. _Batch.run_child (6) ----
add("_Batch.run_child", {"i": 0, "task": {"goal": "g"}, "child": child("c0"),
                         "state": {"batch": batch()},
                         "io": {"run_single_child": entry(0)}})
add("_Batch.run_child", {"i": 1, "task": {}, "child": child("c1"),
                         "state": {"batch": batch()}, "io": {"run_single_child": entry(1)}})
add("_Batch.run_child", {"i": 0, "task": {"goal": "g"}, "child": child("c0"),
                         "state": {"batch": batch()},
                         "io": {"run_single_child": raise_spec("ValueError", "bad child")}})
add("_Batch.run_child", {"i": 0, "task": {"goal": "g"}, "child": child("c0"),
                         "state": {"batch": batch()},
                         "io": {"run_single_child": raise_spec("SystemExit", "bye")}})
add("_Batch.run_child", {"i": 0, "task": {"goal": "g"}, "child": child("c0"),
                         "state": {"batch": batch()}, "io": {}})
add("_Batch.run_child", {"i": 2, "task": {"goal": "g2"}, "child": child("c2"),
                         "state": {"batch": batch(origin_ui_session_id="")},
                         "io": {"run_single_child": entry(2)}})

# ---- 3. _announce_batch (7) ----
add("_announce_batch", {"n_tasks": 2, "live_deleg_id": "d1",
                        "state": {"parent_agent": agent()},
                        "io": {"format_batch_tag": "set 1", "print_completion_line": None}})
add("_announce_batch", {"n_tasks": 1, "live_deleg_id": "d1",
                        "state": {"parent_agent": agent()}, "io": {}})
add("_announce_batch", {"n_tasks": 3, "live_deleg_id": "",
                        "state": {"parent_agent": agent()}, "io": {}})
add("_announce_batch", {"n_tasks": 3, "live_deleg_id": None,
                        "state": {"parent_agent": agent()}, "io": {}})
add("_announce_batch", {"n_tasks": 2, "live_deleg_id": "d1",
                        "state": {"parent_agent": agent(_delegate_spinner=None)},
                        "io": {"format_batch_tag": "set 2", "print_completion_line": None}})
add("_announce_batch", {"n_tasks": 2, "live_deleg_id": "d1",
                        "state": {"parent_agent": agent()},
                        "io": {"format_batch_tag": raise_spec("RuntimeError", "tag boom")}})
add("_announce_batch", {"n_tasks": 2, "live_deleg_id": "d1",
                        "state": {"parent_agent": agent()},
                        "io": {"format_batch_tag": "set 1",
                               "print_completion_line": raise_spec("KeyboardInterrupt", "ki")}})

# ---- 4. _capture_origin (7) ----
_cap_io = {"current_origin_session_id": "wake9", "get_session_env": "ui9",
           "session_history_delivery_supported": True,
           "capture_gateway_steer_authority": wrap([{"__attr__": {"kind": "tui"}},
                                                    {"__attr__": {"sid": "ui9"}}])}
add("_capture_origin", {"io": dict(_cap_io)})
add("_capture_origin", {"io": {**_cap_io, "get_session_env": raise_spec("RuntimeError", "env down")}})
add("_capture_origin", {"io": {**_cap_io, "session_history_delivery_supported": raise_spec("OSError", "nope")}})
add("_capture_origin", {"io": {**_cap_io, "current_origin_session_id": raise_spec("SystemExit", "sx")}})
add("_capture_origin", {"io": {**_cap_io, "capture_gateway_steer_authority": raise_spec("ValueError", "steer")}})
add("_capture_origin", {"io": {}})
add("_capture_origin", {"io": {**_cap_io, "session_history_delivery_supported": False}})

# ---- 5. _report_child_done (11) ----
_rep = {"state": {"parent_agent": agent(), "spinner": spinner()},
        "entry": entry(0), "tag": "set 1", "task_labels": ["g0", "g1"],
        "n_tasks": 2, "remaining": 1}
add("_report_child_done", {**_rep, "io": {"print_completion_line": None, "spinner.update_text": None}})
add("_report_child_done", {**_rep, "entry": entry(1, "failed", failure_reason="crash", error="boom"),
                           "io": {"print_completion_line": None, "spinner.update_text": None,
                                  "describe_subagent_failure": "it crashed"}})
add("_report_child_done", {**_rep, "entry": entry(0, "completed", schema_valid=False),
                           "io": {"print_completion_line": None, "spinner.update_text": None}})
add("_report_child_done", {**_rep, "entry": entry(0, "?"),
                           "io": {"print_completion_line": None, "spinner.update_text": None}})
add("_report_child_done", {**_rep, "entry": entry(5), "task_labels": ["g0"],
                           "io": {"print_completion_line": None, "spinner.update_text": None}})
add("_report_child_done", {**_rep, "remaining": 0,
                           "io": {"print_completion_line": None}})
add("_report_child_done", {**_rep, "spinner": None,
                           "io": {"print_completion_line": None}})
add("_report_child_done", {**_rep, "io": {"print_completion_line": None,
                                          "spinner.update_text": raise_spec("RuntimeError", "spin fail"),
                                          "logger.debug": None}})
add("_report_child_done", {**_rep, "io": {"print_completion_line": raise_spec("GeneratorExit", "ge")}})
add("_report_child_done", {**_rep, "entry": {"status": "completed"},
                           "io": {"print_completion_line": None}})
add("_report_child_done", {**_rep, "entry": entry(0, duration_seconds=3.5), "tag": "",
                           "io": {"print_completion_line": None, "spinner.update_text": None}})

# ---- 6. _record_finished_child (6) ----
add("_record_finished_child", {"entry": entry(0), "honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1")},
                               "io": {"record_unit_child": None}})
add("_record_finished_child", {"entry": entry(0), "honor_parent_interrupt": True,
                               "state": {"batch": batch(unit_id="u1")}, "io": {}})
add("_record_finished_child", {"entry": entry(0), "honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id=None)}, "io": {}})
add("_record_finished_child", {"entry": "not-a-dict", "honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1")}, "io": {}})
add("_record_finished_child", {"entry": [1, 2], "honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1")}, "io": {}})
add("_record_finished_child", {"entry": entry(0), "honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1")},
                               "io": {"record_unit_child": raise_spec("OSError", "disk")}})

# ---- 7. _waiting_on_resume (5) ----
add("_waiting_on_resume", {"entry": entry(0), "io": {"waiting_on_resume": True}})
add("_waiting_on_resume", {"entry": entry(0), "io": {"waiting_on_resume": False}})
add("_waiting_on_resume", {"entry": entry(0), "io": {"waiting_on_resume": raise_spec("ValueError", "v")}})
add("_waiting_on_resume", {"entry": entry(0), "io": {"waiting_on_resume": raise_spec("KeyboardInterrupt", "k")}})
add("_waiting_on_resume", {"entry": entry(0), "io": {}})

# ---- 8. _release_unstarted_child (5) ----
_rel_io = {"release_unstarted": None, "detach_child": None, "close_child": None}
add("_release_unstarted_child", {"child": child("c0"), "state": {"batch": batch()}, "io": dict(_rel_io)})
add("_release_unstarted_child", {"child": child("c0"), "state": {"batch": batch()},
                                 "io": {**_rel_io, "release_unstarted": raise_spec("RuntimeError", "r")}})
add("_release_unstarted_child", {"child": child("c0"), "state": {"batch": batch()},
                                 "io": {**_rel_io, "detach_child": raise_spec("OSError", "d")}})
add("_release_unstarted_child", {"child": child("c0"), "state": {"batch": batch()},
                                 "io": {**_rel_io, "close_child": raise_spec("SystemExit", "s")}})
add("_release_unstarted_child", {"child": child("c0"), "state": {"batch": batch()}, "io": {}})

# ---- 9. _run_gated (7) ----
_gated = {"gate": {"__attr__": {"kind": "prefix"}}, "honor_parent_interrupt": True,
          "i": 0, "task": {"goal": "g"}, "child": child("c0"), "state": {"batch": batch()}}
add("_run_gated", {**_gated, "io": {"gate.run": entry(0)}})
add("_run_gated", {**_gated, "io": {"gate.run": None, **_rel_io,
                                    "fabricated_entry": entry(0, "interrupted")}})
add("_run_gated", {**_gated, "io": {"gate.run": False}})
add("_run_gated", {**_gated, "io": {"gate.run": {}}})
add("_run_gated", {**_gated, "honor_parent_interrupt": False, "io": {"gate.run": entry(0)}})
add("_run_gated", {**_gated, "io": {"gate.run": raise_spec("TimeoutError", "gate wait")}})
add("_run_gated", {**_gated, "io": {"gate.run": None, **_rel_io}})

# ---- 10. _run_children_parallel (12) ----
def par_io(ticks, f0=None, f1=None, **extra):
    io = {"executor.new": {}, "get_prefix_stagger_seconds": 0.5,
          "gate.for_children": None, "format_batch_tag": "set 1",
          "executor.submit": [f0 or fut("f0"), f1 or fut("f1")],
          "cf_wait": [wrap(t) for t in ticks],
          "executor.shutdown": None,
          "print_completion_line": None}
    io.update(extra)
    return io


_ticks2 = [[["f0"], ["f1"]], [["f1"], []]]
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2)})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2,
                                            **{"gate.for_children": {"__attr__": {"kind": "prefix"}}})})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None,
                                                                           _interrupt_requested=True)),
                                         "results": []},
                               "io": par_io([["f0"], ["f1"]], f1=fut("f1", done=False),
                                            fabricated_entry=entry(1, "interrupted"),
                                            **{"future.cancelled": False})})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch(parent_agent=agent(_delegate_spinner=None,
                                                                               _interrupt_requested=True)),
                                         "results": []},
                               "io": {"executor.new": {}, "get_prefix_stagger_seconds": 0.5,
                                      "gate.for_children": None, "format_batch_tag": "set 1",
                                      "executor.submit": [fut("f0", done=False, cancelled=True)],
                                      "fabricated_entry": entry(0, "interrupted"),
                                      "future.cancelled": True,
                                      "executor.shutdown": None, **_rel_io}})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2, f0=fut("f0", result=raise_spec("ValueError", "child boom")),
                                            fabricated_entry=entry(0, "error"))})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2, f0=fut("f0", result=raise_spec("SystemExit", "sx")))})
add("_run_children_parallel", {"honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1",
                                                        parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2, f0=fut("f0", result=entry(0, "failed")),
                                            record_unit_child=None,
                                            describe_subagent_failure="it failed",
                                            waiting_on_resume=False,
                                            push_task_failure_notice=None)})
add("_run_children_parallel", {"honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1",
                                                        parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2, f0=fut("f0", result=entry(0, "failed")),
                                            record_unit_child=None,
                                            describe_subagent_failure="it failed",
                                            waiting_on_resume=True)})
add("_run_children_parallel", {"honor_parent_interrupt": False,
                               "state": {"batch": batch(unit_id="u1",
                                                        parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": par_io(_ticks2, f0=fut("f0", result=entry(0, "timeout")),
                                            record_unit_child=None,
                                            describe_subagent_failure="it timed out",
                                            waiting_on_resume=False,
                                            push_task_failure_notice=raise_spec("OSError", "push fail"),
                                            **{"logger.debug": None})})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": {**par_io(_ticks2),
                                      "executor.submit": raise_spec("RuntimeError", "submit fail")}})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": []},
                               "io": {}})
add("_run_children_parallel", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None)),
                                         "results": [entry(1)]},
                               "io": par_io([[["f0"], []]])})

# ---- 11. _execute_and_aggregate (9) ----
def agg_io(**extra):
    io = {"run_single_child": entry(0), "finalize_child_results": None,
          "time.monotonic": 11.234, "writer.finalize": None,
          "update_manifest_statuses": None, "finish_delegation_unit": None,
          "process_accounting_lines": wrap([])}
    io.update(extra)
    return io


add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch()}, "io": agg_io()})
add("_execute_and_aggregate", {"honor_parent_interrupt": False,
                               "state": {"batch": one_batch(unit_id="u1")},
                               "io": agg_io(record_unit_child=None)})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch(group="gA", live_deleg_id=None)},
                               "io": agg_io()})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch(group="")}, "io": agg_io()})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch(refused=[entry(1, "failed")])},
                               "io": agg_io()})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch()},
                               "io": agg_io(**{"writer.finalize": raise_spec("OSError", "wf"),
                                               "logger.debug": None})})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch()},
                               "io": agg_io(**{"process_accounting_lines": wrap(["bg proc 1 running"])})})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": one_batch()},
                               "io": {**agg_io(), "time.monotonic": raise_spec("RuntimeError", "clock")}})
add("_execute_and_aggregate", {"honor_parent_interrupt": True,
                               "state": {"batch": batch(parent_agent=agent(_delegate_spinner=None))},
                               "io": {**par_io(_ticks2), **agg_io()}})

# ---- 12. _run_sync_with_note (5) ----
add("_run_sync_with_note", {"reason": "no_async", "state": {"batch": one_batch()}, "io": agg_io()})
add("_run_sync_with_note", {"reason": "at_capacity", "state": {"batch": one_batch()}, "io": agg_io()})
add("_run_sync_with_note", {"reason": "bogus", "state": {"batch": one_batch()}, "io": agg_io()})
add("_run_sync_with_note", {"reason": "no_async", "state": {"batch": one_batch()},
                            "io": {**agg_io(), "time.monotonic": raise_spec("RuntimeError", "clock")}})
add("_run_sync_with_note", {"reason": "no_async",
                            "state": {"batch": one_batch(refused=[entry(1, "failed")])},
                            "io": agg_io()})

# ---- 13. _resolve_async_wake_sid (9) ----
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1", "oneshot_session": True,
                                "io": {"finite_turn_required": False}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1",
                                "io": {"finite_turn_required": True}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1",
                                "io": {"finite_turn_required": None, "get_session_env": "1"}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1",
                                "io": {"finite_turn_required": None, "get_session_env": "0",
                                       "async_delivery_supported": True}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1",
                                "io": {"finite_turn_required": False,
                                       "async_delivery_supported": raise_spec("RuntimeError", "ad")}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1",
                                "io": {"finite_turn_required": False,
                                       "async_delivery_supported": raise_spec("SystemExit", "sx")}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1", "origin_session_history_delivery": True,
                                "io": {"finite_turn_required": False, "async_delivery_supported": False,
                                       "logger.info": None}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "w1", "origin_session_history_delivery": False,
                                "io": {"finite_turn_required": False, "async_delivery_supported": False,
                                       "logger.info": None}})
add("_resolve_async_wake_sid", {"origin_wake_sid": "",
                                "io": {"finite_turn_required": False, "async_delivery_supported": False}})

# ---- 14. _resolve_async_session_key (7) ----
_sk = {"state": {"parent_agent": agent()}, "origin_ui_session_id": "ui0"}
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": "k1",
                                                 "get_session_env": ["tui", "ui2"]}})
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": "k1",
                                                 "get_session_env": ["cli", "ui2"]}})
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": "",
                                                 "get_session_env": ["cli", ""]}})
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": "k1",
                                                 "get_session_env": raise_spec("RuntimeError", "env")}})
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": raise_spec("OSError", "ock")}})
add("_resolve_async_session_key", {**_sk, "state": {"parent_agent": agent(session_id=None)},
                                   "io": {"get_current_session_key": "",
                                          "get_session_env": ["tui", ""]}})
add("_resolve_async_session_key", {**_sk, "io": {"get_current_session_key": "k1",
                                                 "get_session_env": ["tui", ""]}})

# ---- 15. _batch_progress_token (7) ----
def pchild(cid, summary):
    return {"__attr__": {"_subagent_id": cid, "__summary__": summary}}


add("_batch_progress_token", {"child_agents": [child("c0"), child("c1")],
                              "io": {"child.get_activity_summary": [
                                  {"api_call_count": 2, "current_tool": "read_file", "last_activity_ts": 100.5},
                                  {"api_call_count": 1, "current_tool": None, "last_activity_ts": 99.25}]}})
add("_batch_progress_token", {"child_agents": [child("c0")],
                              "io": {"child.get_activity_summary": raise_spec("RuntimeError", "sum")}})
add("_batch_progress_token", {"child_agents": [child("c0")],
                              "io": {"child.get_activity_summary": "not-a-dict"}})
add("_batch_progress_token", {"child_agents": [child("c0")],
                              "io": {"child.get_activity_summary": raise_spec("SystemExit", "sx")}})
add("_batch_progress_token", {"child_agents": [], "io": {}})
add("_batch_progress_token", {"child_agents": [child("c0")],
                              "io": {"child.get_activity_summary": {}}})
add("_batch_progress_token", {"child_agents": [child("c0")], "io": {}})

# ---- 16. _dispatched_payload (7) ----
def unit_b(**over):
    return batch(**over)


add("_dispatched_payload", {"dispatched": [[unit_b(), "d1"]], "state": {"batch": one_batch()}})
add("_dispatched_payload", {"dispatched": [[unit_b(group="a"), "d1-1"], [unit_b(group="b"), "d1-2"]],
                            "state": {"batch": batch()}})
add("_dispatched_payload", {"dispatched": [[unit_b(), "d1"]],
                            "state": {"batch": one_batch(refused=[entry(1, "failed")])}})
add("_dispatched_payload", {"dispatched": [[unit_b(), "d1"]],
                            "state": {"batch": one_batch(live_paths=["/tmp/x.log"])}})
add("_dispatched_payload", {"dispatched": [[unit_b(), "u9"]],
                            "state": {"batch": one_batch(live_deleg_id=None)}})
add("_dispatched_payload", {"dispatched": [], "state": {"batch": one_batch(live_deleg_id=None)}})
add("_dispatched_payload", {"dispatched": [[unit_b(), "d1"]],
                            "state": {"batch": one_batch(children=[[0, {"goal": "solo goal"},
                                                                    child("c0", _subagent_id=None)]])}})

# ---- 17. _units_of (7) ----
add("_units_of", {"state": {"batch": batch()}, "io": {"get_independent_completions": False}})
add("_units_of", {"state": {"batch": grouped_batch()}, "io": {"get_independent_completions": True}})
add("_units_of", {"state": {"batch": batch(task_list=[{"goal": "g", "group": ""}],
                                           children=[[0, {"goal": "g", "group": ""}, child("c0")]])},
                  "io": {"get_independent_completions": True}})
add("_units_of", {"state": {"batch": batch(task_list=[{"goal": "g", "group": None}],
                                           children=[[0, {"goal": "g", "group": None}, child("c0")]])},
                  "io": {"get_independent_completions": True}})
add("_units_of", {"state": {"batch": batch(task_list=[{"goal": "g", "group": 0}],
                                           children=[[0, {"goal": "g", "group": 0}, child("c0")]])},
                  "io": {"get_independent_completions": True}})
add("_units_of", {"state": {"batch": batch()}, "io": {}})
add("_units_of", {"state": {"batch": batch(children=[])}, "io": {"get_independent_completions": True}})

# ---- 18. _dispatch_unit (7) ----
_du = {"unit_id": "u1", "slot_key": None,
       "routing": {"session_key": "k1", "origin_ui_session_id": "ui1",
                   "origin_session_id": "", "parent_session_id": "s1", "max_async_children": 4},
       "state": {"unit": one_batch()}}
add("_dispatch_unit", {**_du, "io": {"dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}})
add("_dispatch_unit", {**_du, "state": {"unit": one_batch(creds={"model": None})},
                       "io": {"dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}})
add("_dispatch_unit", {**_du, "state": {"unit": one_batch(creds={"model": None},
                                                            children=[[0, {"goal": "solo goal"},
                                                                       child("c0", model=None)]])},
                       "io": {"dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}})
add("_dispatch_unit", {**_du, "state": {"unit": batch()},
                       "io": {"dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}})
add("_dispatch_unit", {**_du, "state": {"unit": one_batch(live_writers=[None])},
                       "io": {"dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}})
add("_dispatch_unit", {**_du, "io": {"dispatch_async": raise_spec("RuntimeError", "reg fail")}})
add("_dispatch_unit", {**_du, "state": {"unit": one_batch(creds={})}, "io": {}})

# ---- 19. _restore_parent_cancellation (5) ----
add("_restore_parent_cancellation", {"state": {"unit": batch()},
                                     "io": {"attach_child": [None, None]}})
add("_restore_parent_cancellation", {"state": {"unit": batch(children=[])}, "io": {}})
add("_restore_parent_cancellation", {"state": {"unit": batch()},
                                     "io": {"attach_child": raise_spec("OSError", "att")}})
add("_restore_parent_cancellation", {"state": {"unit": batch()},
                                     "io": {"attach_child": [None, raise_spec("SystemExit", "sx")]}})
add("_restore_parent_cancellation", {"state": {"unit": batch()}, "io": {}})

# ---- 20. _dispatch_background (10) ----
def bg_io(**extra):
    io = {"finite_turn_required": False, "async_delivery_supported": True,
          "get_current_session_key": "k1", "get_session_env": ["cli", "ui2"],
          "get_max_async_children": 4, "get_independent_completions": False,
          "detach_child": None,
          "dispatch_async": {"status": "dispatched", "delegation_id": "d1"}}
    io.update(extra)
    return io


add("_dispatch_background", {"state": {"batch": one_batch(origin_wake_sid="")},
                             "io": {**bg_io(), "async_delivery_supported": False,
                                    "logger.info": None, **agg_io()}})
add("_dispatch_background", {"state": {"batch": one_batch()}, "io": bg_io()})
add("_dispatch_background", {"state": {"batch": grouped_batch()},
                             "io": bg_io(get_independent_completions=True,
                                         detach_child=[None, None, None, None],
                                         dispatch_async=[{"status": "dispatched", "delegation_id": "d1-1"},
                                                         {"status": "dispatched", "delegation_id": "d1-2"},
                                                         {"status": "dispatched", "delegation_id": "d1-3"}])})
add("_dispatch_background", {"state": {"batch": one_batch(detached_only=True)},
                             "io": bg_io(dispatch_async={"status": "rejected", "error": "pool full"},
                                         **_rel_io, tool_error="{\"error\": \"Not continued\"}")})
add("_dispatch_background", {"state": {"batch": one_batch()},
                             "io": bg_io(dispatch_async={"status": "rejected", "error": "pool full"},
                                         **{"logger.info": None}, **agg_io())})
add("_dispatch_background", {"state": {"batch": grouped_batch()},
                             "io": bg_io(get_independent_completions=True,
                                         detach_child=[None, None, None, None],
                                         dispatch_async=[{"status": "dispatched", "delegation_id": "d1-1"},
                                                         {"status": "rejected", "error": "sched"},
                                                         {"status": "dispatched", "delegation_id": "d1-3"}],
                                         **{"logger.warning": None},
                                         attach_child=None,
                                         run_single_child=entry(2),
                                         finalize_child_results=None,
                                         **{"time.monotonic": 12.567, "writer.finalize": None,
                                            "update_manifest_statuses": None,
                                            "finish_delegation_unit": None,
                                            "process_accounting_lines": wrap([])})})
add("_dispatch_background", {"state": {"batch": one_batch(live_deleg_id=None)},
                             "io": bg_io(new_delegation_id="gen1")})
add("_dispatch_background", {"state": {"batch": one_batch(parent_agent=agent(_oneshot_session=True))},
                             "io": {**bg_io(), "logger.info": None, **agg_io()}})
add("_dispatch_background", {"state": {"batch": one_batch()},
                             "io": bg_io(dispatch_async=raise_spec("KeyboardInterrupt", "ki"))})
add("_dispatch_background", {"state": {"batch": one_batch()}, "io": {}})

# ---- 21. _refuse_detached_only (5) ----
add("_refuse_detached_only", {"why": "pool is full", "state": {"batch": batch()},
                              "io": {**_rel_io, "release_unstarted": [None, None],
                                     "detach_child": [None, None], "close_child": [None, None],
                                     "tool_error": "{\"error\": \"Not continued: pool is full.\"}"}})
add("_refuse_detached_only", {"why": "w", "state": {"batch": batch(children=[])},
                              "io": {"tool_error": "{\"error\": \"Not continued: w.\"}"}})
add("_refuse_detached_only", {"why": "w", "state": {"batch": batch()},
                              "io": {**_rel_io, "release_unstarted": raise_spec("RuntimeError", "rel")}})
add("_refuse_detached_only", {"why": "w", "state": {"batch": batch()},
                              "io": {**_rel_io, "tool_error": raise_spec("SystemExit", "sx")}})
add("_refuse_detached_only", {"why": "w", "state": {"batch": batch()}, "io": {**_rel_io}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [f for f in [
        "_Batch.owner_kwargs", "_Batch.run_child", "_announce_batch", "_capture_origin",
        "_report_child_done", "_record_finished_child", "_waiting_on_resume",
        "_release_unstarted_child", "_run_gated", "_run_children_parallel",
        "_execute_and_aggregate", "_run_sync_with_note", "_resolve_async_wake_sid",
        "_resolve_async_session_key", "_batch_progress_token", "_dispatched_payload",
        "_units_of", "_dispatch_unit", "_restore_parent_cancellation",
        "_dispatch_background", "_refuse_detached_only"] if counts.get(f, 0) < 5]
    if missing:
        raise SystemExit("generator: <5 cases for %s" % missing)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %s: %d cases across %d funcs" % (OUT, len(CASES), len(counts)))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))


if __name__ == "__main__":
    main()