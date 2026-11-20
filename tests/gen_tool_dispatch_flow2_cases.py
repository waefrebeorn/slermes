#!/usr/bin/env python3
"""Generate tests/fixtures/tool_dispatch_flow2.jsonl — the fixture corpus for
the tool_dispatch_flow2 FLOW oracle (the unclaimed delegate_task dispatch
remainder across 7 defining modules, 28 funcs; >=5 cases per func).

Case shape: {"func": <live name>, "in": {..., "io": {seam: script}, "state": {...}}}
Script semantics (shared with the C harness dispatcher): a single value is
reused for every call; a BARE array is a per-call queue; a list RESULT must be
wrapped {"__script__": true, "ret": [...]}; {"__raise__": kind, "message": m}
raises (real builtin classes; SystemExit/KeyboardInterrupt/GeneratorExit pass
every except Exception). Absent/exhausted scripts fail closed with a
RuntimeError "no script for <seam>" raise spec recorded BEFORE propagation.
active_children_op is deterministic (no script).

Domain (see the oracle header): ASCII fixture text; no integral floats in
str()-computed positions; float() coercion fixtures avoid exotic
underscore/inf/nan strings; accounting/activity carriers are lists-or-null /
dict-or-null; gate children carry __id__ for identity.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "tool_dispatch_flow2.jsonl"


def child(cid, **attrs):
    return {"__attr__": dict(attrs), "__id__": cid}


def agent(**attrs):
    return {"__attr__": dict(attrs)}


def kids(*children):
    return {"__children__": list(children)}


def cb(seam):
    return {"__callable__": seam}


def ev(eid):
    return {"__event__": eid}


def wrap(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, msg):
    return {"__raise__": kind, "message": msg}


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


# ---- _num (6) ----
add("_num", {"value": 2.9})
add("_num", {"value": -2.9})
add("_num", {"value": True})
add("_num", {"value": "x", "default": 7})
add("_num", {"value": None})
add("_num", {"value": 5})

# ---- _str_or_none (5) ----
add("_str_or_none", {"value": "s"})
add("_str_or_none", {"value": 5})
add("_str_or_none", {"value": None})
add("_str_or_none", {"value": ""})
add("_str_or_none", {"value": True})

# ---- _tier_in_effect (7) ----
add("_tier_in_effect", {"state": {"child": child("c", service_tier="priority")}})
add("_tier_in_effect", {"state": {"child": child("c", request_overrides={"service_tier": "cold"})}})
add("_tier_in_effect", {"state": {"child": child("c", request_overrides={"speed": "fast"})}})
add("_tier_in_effect", {"state": {"child": child("c", request_overrides={"extra_body": {"service_tier": "priority"}})}})
add("_tier_in_effect", {"state": {"child": child("c", request_overrides={"extra_body": {"speed": "fast"}})}})
add("_tier_in_effect", {"state": {"child": child("c")}})
add("_tier_in_effect", {"state": {"child": child("c", service_tier="", request_overrides={"speed": "fast"})}})

# ---- _route_report (6) ----
add("_route_report", {"state": {"child": child("c", _delegate_route={"backend": "x"}, provider="p",
     model="m", reasoning_config={"enabled": True, "effort": "high"})},
     "io": {"service_tier_word": "normal"}})
add("_route_report", {"state": {"child": child("c")}})  # route absent -> null, no seam
add("_route_report", {"state": {"child": child("c", _delegate_route={"backend": "x"},
     reasoning_config={"enabled": False, "effort": "high"})}, "io": {"service_tier_word": "normal"}})
add("_route_report", {"state": {"child": child("c", _delegate_route={"backend": "x"},
     _fallback_activated=True)}, "io": {"service_tier_word": "fast"}})
add("_route_report", {"state": {"child": child("c", _delegate_route={"backend": "x"})},
     "io": {"service_tier_word": rs("RuntimeError", "boom")}})
add("_route_report", {"state": {"child": child("c", _delegate_route={"backend": "x"}, provider=5)},
     "io": {"service_tier_word": "normal"}})

# ---- _fabricated_entry (5) ----
add("_fabricated_entry", {"idx": 0, "status": "error", "error": "bad", "state": {"child": child("c")}})
add("_fabricated_entry", {"idx": 2, "status": "interrupted", "error": "stop", "duration": 1.5,
     "state": {"child": child("c", _delegate_role="worker", _delegate_route={"backend": "x"})},
     "io": {"service_tier_word": "normal"}})
add("_fabricated_entry", {"idx": 1, "status": "error", "error": "e",
     "state": {"child": child("c", _delegate_route={"backend": "x"})},
     "io": {"service_tier_word": rs("ValueError", "x")}})
add("_fabricated_entry", {"idx": 0, "status": "completed", "error": None, "duration": 0,
     "state": {"child": child("c", _delegate_role="lead")}})
add("_fabricated_entry", {"idx": 3, "status": "timeout", "error": "t", "state": {"child": child("c")}})

# ---- _append_missed_steer (5) ----
add("_append_missed_steer", {"late_steer": "go left", "state": {"entry": {"error": "failed"}}})
add("_append_missed_steer", {"late_steer": None, "state": {"entry": {"error": "failed"}}})
add("_append_missed_steer", {"late_steer": "", "state": {"entry": {"error": "failed"}}})
add("_append_missed_steer", {"late_steer": "s", "state": {"entry": {}}})  # KeyError 'error'
add("_append_missed_steer", {"late_steer": "s", "state": {"entry": {"error": None}}})  # TypeError

# ---- _close_child (6) ----
add("_close_child", {"log_message": "close failed: %s", "state": {"child": child("c", close=cb("child.close"))},
     "io": {"child.close": None}})
add("_close_child", {"log_message": "close failed: %s", "state": {"child": child("c", close=cb("child.close"))},
     "io": {"child.close": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_close_child", {"log_message": "close failed: %s", "state": {"child": child("c", close=cb("child.close"))},
     "io": {"child.close": rs("SystemExit", "1")}})
add("_close_child", {"log_message": "m", "state": {"child": child("c")}})  # close absent
add("_close_child", {"log_message": None, "state": {"child": child("c", close=cb("child.close"))},
     "io": {"child.close": rs("RuntimeError", "boom")}})  # silent swallow
add("_close_child", {"log_message": "m", "state": {"child": child("c", close="notcallable")}})

# ---- _with_children_lock (5) ----
add("_with_children_lock", {"op": "append", "state": {"parent_agent": agent(_active_children=kids()),
     "child": child("c0")}})
add("_with_children_lock", {"op": "remove", "state": {"parent_agent": agent(_active_children=kids(child("c0"))),
     "child": child("c0")}})
add("_with_children_lock", {"op": "remove", "state": {"parent_agent": agent(_active_children=kids()),
     "child": child("c0")}})  # ValueError propagate
add("_with_children_lock", {"op": "append", "state": {"parent_agent": agent(), "child": child("c0")}})  # AttributeError
add("_with_children_lock", {"op": "append", "state": {"parent_agent": agent(
     _active_children=kids(), _active_children_lock={"__lock__": True}), "child": child("c0")}})

# ---- _attach_child (6) ----
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=False),
     "child": child("c0")}})
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=True,
     _interrupt_message="stop now"), "child": child("c0", _interrupt_requested=False)},
     "io": {"request_hard_interrupt": False}})
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=True,
     _hard_interrupt_requested=ev("h1")), "child": child("c0", _interrupt_requested=False)},
     "io": {"event.is_set": True, "request_hard_interrupt": False}})
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=True,
     _hard_interrupt_requested=ev("h1"), _interrupt_message="soft"), "child": child("c0", interrupt=cb("child.interrupt"))},
     "io": {"event.is_set": False, "child.interrupt": None}})
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=True,
     _hard_interrupt_requested=ev("h1")), "child": child("c0", interrupt=cb("child.interrupt"))},
     "io": {"event.is_set": False, "child.interrupt": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_attach_child", {"state": {"parent_agent": agent(_active_children=kids(), _interrupt_requested=True,
     _hard_interrupt_requested=ev("h1")), "child": child("c0", interrupt=cb("child.interrupt"))},
     "io": {"event.is_set": False, "child.interrupt": rs("KeyboardInterrupt", "")}})

# ---- _detach_child (5) ----
add("_detach_child", {"state": {"parent_agent": agent(), "child": child("c0")}})  # no _active_children
add("_detach_child", {"state": {"parent_agent": agent(_active_children=kids(child("c0"))), "child": child("c0")}})
add("_detach_child", {"state": {"parent_agent": agent(_active_children=kids()), "child": child("c0")},
     "io": {"logger.debug": None}})  # ValueError -> log
add("_detach_child", {"state": {"parent_agent": agent(_active_children=kids(child("c0"))), "child": child("c0")},
     "io": {}})  # remove present, no log
add("_detach_child", {"state": {"parent_agent": agent(_active_children=kids(child("c0"))), "child": child("c0")}})

# ---- _signal_child_stop (6) ----
add("_signal_child_stop", {"reasons": ["parent stopped"], "state": {"child": child("c", _interrupt_requested=False)},
     "io": {"request_hard_interrupt": False}})
add("_signal_child_stop", {"reasons": ["r"], "state": {"child": child("c", _interrupt_requested=False)},
     "io": {"request_hard_interrupt": True}})
add("_signal_child_stop", {"reasons": ["r"], "state": {"child": None}})  # child None -> no seam
add("_signal_child_stop", {"reasons": ["r"], "state": {"child": child("c", _interrupt_requested=False)},
     "io": {"request_hard_interrupt": rs("RuntimeError", "boom")}})  # swallowed
add("_signal_child_stop", {"reasons": ["r"], "state": {"child": child("c", _interrupt_requested=False)},
     "io": {"request_hard_interrupt": rs("SystemExit", "1")}})  # propagate
add("_signal_child_stop", {"reasons": ["r"], "tool_reason": "custom", "state": {"child": child("c")},
     "io": {"request_hard_interrupt": False}})  # no _interrupt_requested attr

# ---- _child_activity_fingerprint (6) ----
add("_child_activity_fingerprint", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": {"api_call_count": 3, "current_tool": "terminal", "last_activity_ts": 10.5}}})
add("_child_activity_fingerprint", {"state": {"child": child("c")}, "io": {"child.get_activity_summary": None}})
add("_child_activity_fingerprint", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": rs("RuntimeError", "boom")}})
add("_child_activity_fingerprint", {"state": {"child": child("c")},
     "io": {"child.get_activity_summary": rs("GeneratorExit", "")}})
add("_child_activity_fingerprint", {"state": {"child": child("c")}, "io": {"child.get_activity_summary": {}}})
add("_child_activity_fingerprint", {"state": {"child": child("c")}, "io": {"child.get_activity_summary": 5}})

# ---- _quiet (6) ----
add("_quiet", {"log_message": "failed: %s", "io": {"quiet.body": None}})
add("_quiet", {"log_message": "failed: %s", "io": {"quiet.body": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_quiet", {"log_message": None, "io": {"quiet.body": rs("RuntimeError", "boom")}})
add("_quiet", {"log_message": "failed: %s", "io": {"quiet.body": rs("SystemExit", "1")}})
add("_quiet", {"log_message": "failed: %s", "log_args": ["ctx"], "exc_info": True,
     "io": {"quiet.body": rs("ValueError", "bad"), "logger.debug": None}})
add("_quiet", {"log_message": "no placeholder", "io": {"quiet.body": rs("RuntimeError", "boom"), "logger.debug": None}})

# ---- _safe_progress (6) ----
add("_safe_progress", {"cb": None, "event_type": "tool.started"})
add("_safe_progress", {"cb": cb("progress.cb"), "event_type": "tool.started", "args": ["a"], "kwargs": {"k": 1},
     "io": {"progress.cb": None}})
add("_safe_progress", {"cb": cb("progress.cb"), "event_type": "tool.started",
     "io": {"progress.cb": rs("RuntimeError", "boom"), "logger.debug": None}})
add("_safe_progress", {"cb": cb("progress.cb"), "event_type": "tool.started",
     "io": {"progress.cb": rs("KeyboardInterrupt", "")}})
add("_safe_progress", {"cb": "notcallable", "event_type": "tool.started", "io": {"logger.debug": None}})
add("_safe_progress", {"cb": cb("progress.cb"), "event_type": "thinking", "args": [], "kwargs": {},
     "io": {"progress.cb": None}})

# ---- describe_subagent_failure (6) ----
add("describe_subagent_failure", {"failure_reason": "model_not_found", "error": "e",
     "io": {"failure_cause_gloss": "the model was not found"}})
add("describe_subagent_failure", {"failure_reason": None, "error": "traceback",
     "io": {"failure_cause_gloss": None, "clean_error_text": "clean line"}})
add("describe_subagent_failure", {"failure_reason": "x", "error": "e",
     "io": {"failure_cause_gloss": rs("RuntimeError", "boom")}})
add("describe_subagent_failure", {"failure_reason": None, "error": "e",
     "io": {"failure_cause_gloss": None, "clean_error_text": rs("ValueError", "bad")}})
add("describe_subagent_failure", {"failure_reason": "x", "error": "e",
     "io": {"failure_cause_gloss": "", "clean_error_text": "fallback"}})
add("describe_subagent_failure", {"failure_reason": None, "error": "e", "max_chars": 50,
     "io": {"failure_cause_gloss": None, "clean_error_text": "short"}})

# ---- _format_duration (8) ----
add("_format_duration", {"seconds": 0})
add("_format_duration", {"seconds": -1})
add("_format_duration", {"seconds": 30})
add("_format_duration", {"seconds": 120})
add("_format_duration", {"seconds": 59.5})
add("_format_duration", {"seconds": "x"})
add("_format_duration", {"seconds": 179.5})
add("_format_duration", {"seconds": True})

# ---- _print_completion_line (6) ----
add("_print_completion_line", {"parent_agent": agent(), "spinner": agent(print_above=cb("spinner.print_above")),
     "line": "done", "io": {"spinner.print_above": None}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": agent(print_above=cb("spinner.print_above")),
     "line": "done", "io": {"spinner.print_above": rs("RuntimeError", "boom")}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": agent(print_above=cb("spinner.print_above")),
     "line": "done", "io": {"spinner.print_above": rs("SystemExit", "1")}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": None, "line": "done",
     "io": {"emit_parent_console": None}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": None, "line": "done", "console_line": "custom",
     "io": {"emit_parent_console": None}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": agent(print_above=cb("spinner.print_above")),
     "line": "done", "io": {"spinner.print_above": rs("RuntimeError", "boom"), "emit_parent_console": None}})
add("_print_completion_line", {"parent_agent": agent(), "spinner": None, "line": "done",
     "io": {"emit_parent_console": rs("RuntimeError", "boom")}})

# ---- _short (5) ----
add("_short", {"text": "abcdef", "n": 3})
add("_short", {"text": "abc", "n": 3})
add("_short", {"text": "ab", "n": 3})
add("_short", {"text": "abc", "n": 0})
add("_short", {"text": "", "n": 3})

# ---- _route_key (5) ----
add("_route_key", {"state": {"child": child("c", provider="p", base_url="b", model="m", api_mode="chat")}})
add("_route_key", {"state": {"child": child("c", provider="p", model="m")}})
add("_route_key", {"state": {"child": child("c")}})
add("_route_key", {"state": {"child": child("c", provider=None, base_url="b", model="m", api_mode="responses")}})
add("_route_key", {"state": {"child": child("c", provider="p", base_url="b", model="m", api_mode=None)}})

# ---- _caches_prompts (5) ----
add("_caches_prompts", {"state": {"child": child("c", _use_prompt_caching=True)}})
add("_caches_prompts", {"state": {"child": child("c", _use_prompt_caching=False)}})
add("_caches_prompts", {"state": {"child": child("c")}})
add("_caches_prompts", {"state": {"child": child("c", _use_prompt_caching=1)}})
add("_caches_prompts", {"state": {"child": child("c", _use_prompt_caching="x")}})

# ---- _first_response_started (5) ----
add("_first_response_started", {"state": {"child": child("c", session_api_calls=1)}})
add("_first_response_started", {"state": {"child": child("c", session_api_calls=0,
     _last_activity_desc="receiving stream response")}})
add("_first_response_started", {"state": {"child": child("c", session_api_calls=0)}})
add("_first_response_started", {"state": {"child": child("c", _last_activity_desc="receiving stream response")}})
add("_first_response_started", {"state": {"child": child("c", session_api_calls=2)}})

# ---- _stop_requested (4) ----
add("_stop_requested", {"state": {"child": child("c", _interrupt_requested=True)}})
add("_stop_requested", {"state": {"child": child("c", _interrupt_requested=False)}})
add("_stop_requested", {"state": {"child": child("c")}})
add("_stop_requested", {"state": {"child": child("c", _interrupt_requested=1)}})
add("_stop_requested", {"state": {"child": child("c", _interrupt_requested="x")}})

# ---- PrefixCacheGate.for_children (6) ----
LEADER = child("L", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True)
FOLLOW = child("F", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True)
add("PrefixCacheGate.for_children", {"children": [[0, {"goal": "g"}, LEADER]], "max_wait_s": 1.0})  # len<2
add("PrefixCacheGate.for_children", {"children": [[0, {}, LEADER], [1, {}, FOLLOW]], "max_wait_s": 0})
add("PrefixCacheGate.for_children", {"children": [[0, {}, child("L", _use_prompt_caching=False)], [1, {}, FOLLOW]],
     "max_wait_s": 1.0})
add("PrefixCacheGate.for_children", {"children": [[0, {}, LEADER], [1, {}, FOLLOW]], "max_wait_s": 1.0})
add("PrefixCacheGate.for_children", {"children": [[0, {}, LEADER], [1, {}, FOLLOW]], "max_wait_s": -1})
add("PrefixCacheGate.for_children", {"children": [[0, {}, LEADER], [1, {}, FOLLOW], [2, {}, child("G")]],
     "max_wait_s": 2.5})


def gate_state(leader_done=False, leader=None):
    ld = leader if leader is not None else LEADER
    return {"leader": ld, "max_wait_s": 1.0, "leader_route": ["p", "b", "m", "chat"],
            "leader_done": leader_done}


# ---- PrefixCacheGate.run (8) ----
add("PrefixCacheGate.run", {"child": LEADER, "run": cb("gate.run_fn"), "state": {"gate": gate_state()},
     "io": {"gate.run_fn": {"task_index": 0, "status": "completed"}}})
add("PrefixCacheGate.run", {"child": FOLLOW, "run": cb("gate.run_fn"), "state": {"gate": gate_state(leader_done=True)},
     "io": {"gate.run_fn": None, "time.monotonic": [0]}})
add("PrefixCacheGate.run", {"child": FOLLOW, "run": cb("gate.run_fn"),
     "state": {"gate": gate_state(leader=child("L", provider="p", base_url="b", model="m", api_mode="chat",
              _use_prompt_caching=True, session_api_calls=1))},
     "io": {"gate.run_fn": None, "time.monotonic": [0]}})
add("PrefixCacheGate.run", {"child": child("F", provider="p", base_url="b", model="m", api_mode="chat",
     _use_prompt_caching=True, _interrupt_requested=True), "run": cb("gate.run_fn"),
     "state": {"gate": gate_state()}, "io": {"time.monotonic": [0]}})
add("PrefixCacheGate.run", {"child": FOLLOW, "run": cb("gate.run_fn"), "state": {"gate": gate_state()},
     "io": {"gate.run_fn": None, "time.monotonic": [0, 2]}})
add("PrefixCacheGate.run", {"child": FOLLOW, "run": cb("gate.run_fn"), "parent": agent(_interrupt_requested=True),
     "state": {"gate": gate_state()}, "io": {"time.monotonic": [0]}})
add("PrefixCacheGate.run", {"child": FOLLOW, "run": cb("gate.run_fn"), "state": {"gate": gate_state()},
     "io": {"gate.run_fn": None, "time.monotonic": [0, 0.02, 1.5], "event.wait": None}})
add("PrefixCacheGate.run", {"child": LEADER, "run": cb("gate.run_fn"), "state": {"gate": gate_state()},
     "io": {"gate.run_fn": rs("RuntimeError", "boom")}})

# ---- _get_prefix_stagger_seconds (9) ----
add("_get_prefix_stagger_seconds", {"io": {"load_config": {}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": None}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": 5}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": "2.5"}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": -3}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": 100}}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": "abc"}, "logger.warning": None}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": {"prefix_stagger_seconds": [1]}, "logger.warning": None}})
add("_get_prefix_stagger_seconds", {"io": {"load_config": rs("RuntimeError", "boom")}})

# ---- waiting_on_resume (6) ----
add("waiting_on_resume", {"entry": {"auto_resume": {"sid": "s"}}})
add("waiting_on_resume", {"entry": {"waiting_to_resume": True}})
add("waiting_on_resume", {"entry": {"unstarted_auto": 1}})
add("waiting_on_resume", {"entry": {}})
add("waiting_on_resume", {"entry": {"auto_resume": "notdict"}})
add("waiting_on_resume", {"entry": {"waiting_to_resume": "", "unstarted_auto": None}})

# ---- finite_turn_required (5) ----
add("finite_turn_required", {"finite_turn": True})
add("finite_turn_required", {"finite_turn": False})
add("finite_turn_required", {"finite_turn": None})
add("finite_turn_required", {"finite_turn": "x"})
add("finite_turn_required", {"finite_turn": 5})

# ---- _process_accounting_lines (7) ----
add("_process_accounting_lines", {"r": {"handed_off_processes": [{"session_id": "s1", "command": "ls", "note": "n"}]}})
add("_process_accounting_lines", {"r": {"orphaned_processes": [{"session_id": "s2", "command": "sleep 9", "runtime_seconds": 12}]}})
add("_process_accounting_lines", {"r": {"unread_completions": [{"session_id": "s3", "command": "make", "exit_code": 0, "output_tail": "ok"}]}})
add("_process_accounting_lines", {"r": {"handed_off_processes": [{"session_id": "s1", "command": "ls", "note": "n"}],
     "orphaned_processes": [{"session_id": "s2", "command": "x", "runtime_seconds": 1}],
     "unread_completions": [{"session_id": "s3", "command": "y", "exit_code": 2, "output_tail": "t"}]}})
add("_process_accounting_lines", {"r": {}})
add("_process_accounting_lines", {"r": {"handed_off_processes": None, "orphaned_processes": [], "unread_completions": None}})
add("_process_accounting_lines", {"r": {"handed_off_processes": [{"command": "c"}]}})  # session_id/note absent


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count audit
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
    "_num", "_str_or_none", "_tier_in_effect", "_route_report", "_fabricated_entry",
    "_append_missed_steer", "_close_child", "_with_children_lock", "_attach_child",
    "_detach_child", "_signal_child_stop", "_child_activity_fingerprint",
    "_quiet", "_safe_progress", "describe_subagent_failure", "_format_duration",
    "_print_completion_line", "_short", "_route_key", "_caches_prompts",
    "_first_response_started", "_stop_requested", "PrefixCacheGate.for_children",
    "PrefixCacheGate.run", "_get_prefix_stagger_seconds", "waiting_on_resume",
    "finite_turn_required", "_process_accounting_lines",
]

if __name__ == "__main__":
    main()