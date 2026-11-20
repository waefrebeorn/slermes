#!/usr/bin/env python3
"""Generate tests/fixtures/process_registry_notifications_flow2.jsonl for the
process_registry_notifications_flow2 FLOW oracle (tools/process_registry_notifications.py
UNCLAIMED remainder). >=5 cases per ported function (13 funcs), exercising decision
branches, seam calls, BaseException propagation (full builtin hierarchy), real
composition, truncation boundaries and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception.
ASCII / integral-number domain (write ints without a decimal point).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "process_registry_notifications_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── is_child_process_notification ──
add("is_child_process_notification", {"evt": {"type": "async_delegation", "owner_task_id": "sa-5"}})
add("is_child_process_notification", {"evt": {"type": "async_delegation_work_closeout", "task_id": "sa-9"}})
add("is_child_process_notification", {"evt": {"owner_task_id": "sa-123"}})
add("is_child_process_notification", {"evt": {"task_id": "sa-7"}})
add("is_child_process_notification", {"evt": {"owner_task_id": "ordinary"}})
add("is_child_process_notification", {"evt": {"owner_task_id": None, "task_id": "sa-7"}})
add("is_child_process_notification", {"evt": {}})
add("is_child_process_notification", {"evt": {"owner_task_id": 123}})
add("is_child_process_notification", {"evt": {"owner_task_id": "sa-x", "type": "completion"}})

# ── should_surface_notification (seam: surface_child) ──
add("should_surface_notification", {"evt": {"type": "completion"}})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}, "surface_child": True})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}, "surface_child": False})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": True})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": False})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": None})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": R("SystemExit", "bye")})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": R("FileNotFoundError", "gone")})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}}, {"surface_child": R("KeyboardInterrupt", "int")})
add("should_surface_notification", {"evt": {"owner_task_id": "sa-1"}})  # fail-closed -> RuntimeError

# ── is_truncated ──
add("is_truncated", {"entry": {"truncated": True}})
add("is_truncated", {"entry": {"exit_reason": "max_iterations"}})
add("is_truncated", {"entry": {"truncated": False, "exit_reason": "done"}})
add("is_truncated", {"entry": {}})
add("is_truncated", {"entry": {"truncated": False, "exit_reason": "max_iterations"}})
add("is_truncated", {"entry": {"truncated": 0, "exit_reason": "max_iterations"}})
add("is_truncated", {"entry": {"exit_reason": None}})
add("is_truncated", {"entry": {"truncated": "yes"}})

# ── delegation_wakes_parent ──
add("delegation_wakes_parent", {"evt": {"type": "completion"}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "task_failure_notice": True}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "_sibling_failed": True}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "role": "cron_run"}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "completed"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "failed", "error": "x"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "interrupted"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "interrupted"}], "stalled_after_quiet_seconds": 0}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": []}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"error": "boom"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "success"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [{"status": "cancelled"}]}})
add("delegation_wakes_parent", {"evt": {"type": "async_delegation", "results": [5, {"status": "failed"}]}})

# ── completion_resumes_parent ──
add("completion_resumes_parent", {"evt": {"type": "completion"}})
add("completion_resumes_parent", {"evt": {"type": "watch_match"}})
add("completion_resumes_parent", {"evt": {"type": "async_delegation", "task_failure_notice": True}})
add("completion_resumes_parent", {"evt": {"type": "async_delegation"}})
add("completion_resumes_parent", {"evt": {"type": "async_delegation", "role": "cron_run"}})
add("completion_resumes_parent", {"evt": {"type": "async_delegation", "_sibling_resumes": True}})
add("completion_resumes_parent", {"evt": {"type": "async_delegation", "_sibling_resumes": False, "role": "cron_run"}})
add("completion_resumes_parent", {"evt": {}})

# ── group_process_notifications ──
add("group_process_notifications", {"notifications": []})
add("group_process_notifications", {"notifications": [[{"type": "completion"}, "A"], [{"type": "completion"}, "B"]]})
add("group_process_notifications", {"notifications": [[{"type": "watch_match"}, "W"]]})
add("group_process_notifications", {"notifications": [
    [{"type": "completion"}, "A"], [{"type": "completion"}, "B"],
    [{"type": "watch_match"}, "W"], [{"type": "completion"}, "C"]]})
add("group_process_notifications", {"notifications": [
    [{"type": "completion"}, "A"], [{"type": "async_delegation"}, "D"]]})
add("group_process_notifications", {"notifications": [[{}, "A"]]})
add("group_process_notifications", {"notifications": [[{"type": None}, "A"]]})
add("group_process_notifications", {"notifications": [
    [{"type": "watch_disabled"}, "W"], [{"type": "completion"}, "C"]]})

# ── short_title ──
add("short_title", {"title": "hello"})
add("short_title", {"title": "a" * 197})
add("short_title", {"title": "b" * 200})
add("short_title", {"title": "c" * 201})
add("short_title", {"title": ""})
add("short_title", {"title": "d" * 250})
add("short_title", {"title": "a b c"})

# ── short_command ──
add("short_command", {"command": "  ls   -la  "})
add("short_command", {"command": "a\tb\nc"})
add("short_command", {"command": ""})
add("short_command", {"command": None})
add("short_command", {"command": "x" * 85})
add("short_command", {"command": "y" * 80})
add("short_command", {"command": "git   status"})
add("short_command", {"command": 5})

# ── process_completion_display_text ──
add("process_completion_display_text", {"events": []})
add("process_completion_display_text", {"events": [{"exit_code": 0}, {"exit_code": 1}]})
add("process_completion_display_text", {"events": [{"exit_code": 0, "command": "ls"}]})
add("process_completion_display_text", {"events": [{"exit_code": 1, "command": "ls"}]})
add("process_completion_display_text", {"events": [{"completion_reason": "killed", "exit_code": -15, "command": "ls"}]})
add("process_completion_display_text", {"events": [{"completion_reason": "lost", "command": "ls"}]})
add("process_completion_display_text", {"events": [{"exit_code": 0}]})
add("process_completion_display_text", {"events": [{"exit_code": 1, "command": ""}]})
add("process_completion_display_text", {"events": [{"completion_reason": "killed", "command": "  ls   -l "}]})
add("process_completion_display_text", {"events": [{"exit_code": "?"}]})

# ── completion_status ──
add("completion_status", {"evt": {}})
add("completion_status", {"evt": {"exit_code": 0}})
add("completion_status", {"evt": {"exit_code": 1}})
add("completion_status", {"evt": {"completion_reason": "killed"}})
add("completion_status", {"evt": {"completion_reason": "killed", "termination_source": "user"}})
add("completion_status", {"evt": {"completion_reason": "lost"}})
add("completion_status", {"evt": {"completion_reason": "timeout", "exit_code": 0}})
add("completion_status", {"evt": {"completion_reason": "killed", "termination_source": ""}})

# ── heartbeat_output_tail ──
add("heartbeat_output_tail", {"output": "abc", "omitted": 0})
add("heartbeat_output_tail", {"output": "abc"})
add("heartbeat_output_tail", {"output": "z" * 2000, "omitted": 3})
add("heartbeat_output_tail", {"output": "z" * 2001, "omitted": 0})
add("heartbeat_output_tail", {"output": "q" * 2100, "omitted": 5})
add("heartbeat_output_tail", {"output": "", "omitted": 0})
add("heartbeat_output_tail", {"output": "xyz", "omitted": 7})

# ── heartbeat_output_text ──
add("heartbeat_output_text", {"evt": {"output": "hello"}})
add("heartbeat_output_text", {"evt": {"omitted": 5, "output": "tail"}})
add("heartbeat_output_text", {"evt": {"omitted": 0, "output": "tail"}})
add("heartbeat_output_text", {"evt": {"output": ""}})
add("heartbeat_output_text", {"evt": {"omitted": "3", "output": "x"}})
add("heartbeat_output_text", {"evt": {"omitted": None, "output": "x"}})
add("heartbeat_output_text", {"evt": {"omitted": -2, "output": "y"}})

# ── join_heartbeat_outputs ──
add("join_heartbeat_outputs", {"earlier": {"output": "abc", "omitted": 0}, "later": {"output": "def", "omitted": 0}})
add("join_heartbeat_outputs", {"earlier": {"output": "abc", "omitted": 5}, "later": {"output": "def", "omitted": 0}})
add("join_heartbeat_outputs", {"earlier": {"output": "abc", "omitted": 2}, "later": {"output": "de", "omitted": 3}})
add("join_heartbeat_outputs", {"earlier": {}, "later": {}})
add("join_heartbeat_outputs", {"earlier": {"output": "abc", "omitted": None}, "later": {"output": "de"}})
add("join_heartbeat_outputs", {"earlier": {"output": "a" * 1999, "omitted": 1}, "later": {"output": "bb", "omitted": 0}})
add("join_heartbeat_outputs", {"earlier": {"output": "abc", "omitted": 1}, "later": {"output": "de", "omitted": "4"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
