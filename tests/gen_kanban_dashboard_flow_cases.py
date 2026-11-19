#!/usr/bin/env python3
"""Generate tests/fixtures/kanban_dashboard_flow.jsonl for the
kanban_dashboard_flow FLOW oracle (>=5 cases per ported function).

Each case is {"func": <live_name>, "in": {...}} where in.io carries the scripted
seam results ({"ret"/"raise"/"import_raise"} or a per-call queue LIST) the C
harness dispatcher and the oracle's recording stubs both consume. Cases cover
normal paths, falsy/empty edges, error mapping, BaseException propagation,
in-function import failure, and fail-closed (missing script) paths.

ASCII fixture scope. _task_dict cases carry a FULL kanban_db.Task field object
(asdict round-trips it). Run: python tests/gen_kanban_dashboard_flow_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "kanban_dashboard_flow.jsonl"

# Full kanban_db.Task field object (defaults pinned to the live dataclass).
def base_task(**over):
    t = {
        "id": "t1", "title": "T", "body": None, "assignee": None, "status": "todo",
        "priority": 0, "created_by": "dashboard", "created_at": 100, "started_at": None,
        "completed_at": None, "workspace_kind": "scratch", "workspace_path": None,
        "claim_lock": None, "claim_expires": None, "tenant": None, "branch_name": None,
        "project_id": None, "result": None, "idempotency_key": None, "consecutive_failures": 0,
        "worker_pid": None, "last_failure_error": None, "max_runtime_seconds": None,
        "last_heartbeat_at": None, "current_run_id": None, "workflow_template_id": None,
        "current_step_key": None, "skills": None, "model_override": None, "provider_override": None,
        "reasoning_effort": None, "max_retries": None, "goal_mode": False, "goal_max_turns": None,
        "session_id": None, "block_kind": None, "block_recurrences": 0, "completion_contract": None,
    }
    t.update(over)
    return t


def ret(v):
    return {"ret": v}


def raise_(t, m=""):
    return {"raise": {"type": t, "message": m}}


def imp(t, m=""):
    return {"import_raise": {"type": t, "message": m}}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# 1. _placeholders
for ids in (["a", "b", "c"], [], ["x"], ["a", "b"], [1, 2, 3], ["only"]):
    add("_placeholders", ids=ids)

# 2. _cap
for s, n in (("  hello world  ", 5), ("hi", 5), (None, 3), ("abcdef", 6),
             ("  pad  ", 10), ("abcdefgh", 3), ("", 4)):
    add("_cap", s=s, n=n)

# 3. _attachment_dict
add("_attachment_dict", a={"id": 1, "task_id": "t1", "filename": "f.pdf", "content_type": "application/pdf",
                           "size": 10, "uploaded_by": "u", "stored_path": "/x/f.pdf", "created_at": 100})
add("_attachment_dict", a={"id": 2, "task_id": "t2", "filename": "g.txt", "content_type": None,
                           "size": 0, "uploaded_by": None, "stored_path": "/y/g.txt", "created_at": 0})
add("_attachment_dict", a={"id": 3, "task_id": "t1", "filename": "a b.png", "content_type": "image/png",
                           "size": 999, "uploaded_by": "dash", "stored_path": "/z/a b.png", "created_at": 5})
add("_attachment_dict", a={"id": 4, "task_id": "t9", "filename": "x", "content_type": "text/plain",
                           "size": 1, "uploaded_by": "u2", "stored_path": "/x", "created_at": 7})
add("_attachment_dict", a={"id": 5, "task_id": "t5", "filename": "y", "content_type": None,
                           "size": 42, "uploaded_by": None, "stored_path": "/p/y", "created_at": 9})
add("_attachment_dict", a={"id": 6, "task_id": "t6", "filename": "z", "content_type": "application/json",
                           "size": 3, "uploaded_by": "u3", "stored_path": "/q/z", "created_at": 11})

# 4. _board_display_kwargs
add("_board_display_kwargs", p={"name": "N", "description": "D", "icon": "I", "color": "C"})
add("_board_display_kwargs", p={"name": None, "description": None, "icon": None, "color": None})
add("_board_display_kwargs", p={"name": "Board", "description": "", "icon": "star", "color": "#fff"})
add("_board_display_kwargs", p={"name": "x", "description": "d", "icon": None, "color": "red"})
add("_board_display_kwargs", p={"name": "", "description": "desc", "icon": "i", "color": None})
add("_board_display_kwargs", p={"name": "Z", "description": None, "icon": "", "color": "blue"})

# 5. _warnings_summary_from_diagnostics
add("_warnings_summary_from_diagnostics", diagnostics=[])
add("_warnings_summary_from_diagnostics", diagnostics=[{"kind": "stale", "count": 1, "last_seen_at": 10, "severity": "warning"}])
add("_warnings_summary_from_diagnostics", diagnostics=[{"kind": "stale", "count": 2, "last_seen_at": 50, "severity": "warning"},
                                                        {"kind": "loop", "count": 1, "last_seen_at": 90, "severity": "critical"}])
add("_warnings_summary_from_diagnostics", diagnostics=[{"kind": "a", "last_seen_at": 5, "severity": "error"}])  # count default 1
add("_warnings_summary_from_diagnostics", diagnostics=[{"kind": "a", "count": 3, "last_seen_at": None, "severity": "bogus"}])
add("_warnings_summary_from_diagnostics", diagnostics=[{"kind": "a", "count": 1, "last_seen_at": 1, "severity": "critical"},
                                                        {"kind": "b", "count": 2, "last_seen_at": 2, "severity": "warning"}])

# 6. _require_ok
for ok in (False, True, 0, 1, "", "x"):
    add("_require_ok", ok=ok)

# 7. _conflict
for d in ("nope", "task not found", "", "cannot move", "a b c", "x: y"):
    add("_conflict", detail=d)

# 8. _require
add("_require", ident="known", label="task", io={"kanban_db.get_task": ret({"id": "known"})})
add("_require", ident="missing", label="task", io={"kanban_db.get_task": ret(None)})
add("_require", ident="e", label="task", io={"kanban_db.get_task": ret({})})
add("_require", ident="x", label="run", ident_key="run_id", getter_name="kanban_db.get_run",
    io={"kanban_db.get_run": raise_("RuntimeError", "boom")})
add("_require", ident="x", label="task", io={"kanban_db.get_task": raise_("ValueError", "bad")})
add("_require", ident="x", label="task", io={})

# 9. _require_task
add("_require_task", task_id="known", io={"kanban_db.get_task": ret({"id": "known", "title": "T"})})
add("_require_task", task_id="missing", io={"kanban_db.get_task": ret(None)})
add("_require_task", task_id="t1", io={})
add("_require_task", task_id="t1", io={"kanban_db.get_task": raise_("SystemExit", "1")})
add("_require_task", task_id="t2", io={"kanban_db.get_task": ret({"id": "t2"})})
add("_require_task", task_id="t3", io={"kanban_db.get_task": raise_("RuntimeError", "db")})

# 10. _require_run
add("_require_run", run_id=5, io={"kanban_db.get_run": ret({"id": 5})})
add("_require_run", run_id=5, io={"kanban_db.get_run": ret(None)})
add("_require_run", run_id=7, io={})
add("_require_run", run_id=8, io={"kanban_db.get_run": raise_("SystemExit", "1")})
add("_require_run", run_id=9, io={"kanban_db.get_run": ret({"id": 9, "task_id": "t1"})})
add("_require_run", run_id=10, io={"kanban_db.get_run": raise_("ValueError", "x")})

# 11. _normalize_slug_or_400
add("_normalize_slug_or_400", slug="ABC", io={"kanban_db._normalize_board_slug": ret("abc")})
add("_normalize_slug_or_400", slug="X", io={"kanban_db._normalize_board_slug": raise_("ValueError", "bad slug")})
add("_normalize_slug_or_400", slug="X", io={"kanban_db._normalize_board_slug": raise_("RuntimeError", "boom")})
add("_normalize_slug_or_400", slug="X", io={"kanban_db._normalize_board_slug": raise_("SystemExit", "1")})
add("_normalize_slug_or_400", slug="ok", io={"kanban_db._normalize_board_slug": ret(None)})
add("_normalize_slug_or_400", slug="ok", io={})

# 12. _existing_board_slug
add("_existing_board_slug", slug="exists", io={"kanban_db._normalize_board_slug": ret("exists"), "kanban_db.board_exists": ret(True)})
add("_existing_board_slug", slug="missing", io={"kanban_db._normalize_board_slug": ret("missing"), "kanban_db.board_exists": ret(False)})
add("_existing_board_slug", slug="x", io={"kanban_db._normalize_board_slug": ret("")})
add("_existing_board_slug", slug="X", io={"kanban_db._normalize_board_slug": raise_("ValueError", "bad")})
add("_existing_board_slug", slug="x", io={})
add("_existing_board_slug", slug="b", io={"kanban_db._normalize_board_slug": ret("b"), "kanban_db.board_exists": raise_("RuntimeError", "x")})

# 13. _ws_upgrade_authorized
add("_ws_upgrade_authorized", io={"_ws_auth_ok": ret(True), "_ws_request_is_allowed": ret(True)})
add("_ws_upgrade_authorized", io={"_ws_auth_ok": ret(False)})
add("_ws_upgrade_authorized", io={"_ws_auth_ok": ret(True), "_ws_request_is_allowed": ret(False)})
add("_ws_upgrade_authorized", io={"import:hermes_cli.web_server_chat": imp("ImportError", "no module")})
add("_ws_upgrade_authorized", io={"_ws_auth_ok": raise_("RuntimeError", "x")})
add("_ws_upgrade_authorized", io={})

# 14. _since_param
for qp in ({"since": "5"}, {"since": "-3"}, {"since": "abc"}, {}, {"since": "  "},
           {"since": "0"}, {"since": "  7  "}, {"since": "3.5"}):
    add("_since_param", query_params=qp)

# 15. _ws_board
add("_ws_board", raw="ABC", io={"kanban_db._normalize_board_slug": ret("abc")})
add("_ws_board", raw=None)
add("_ws_board", raw="")
add("_ws_board", raw="X", io={"kanban_db._normalize_board_slug": raise_("ValueError", "bad")})
add("_ws_board", raw="X", io={"kanban_db._normalize_board_slug": raise_("SystemExit", "1")})
add("_ws_board", raw="ok", io={})

# 16. _ws_board_live
add("_ws_board_live", normed="default")
add("_ws_board_live", normed="exists", io={"kanban_db.board_exists": ret(True)})
add("_ws_board_live", normed="missing", io={"kanban_db.board_exists": ret(False)})
add("_ws_board_live", normed=None)
add("_ws_board_live", normed="")
add("_ws_board_live", normed="b", io={"kanban_db.board_exists": raise_("RuntimeError", "x")})

# 17. _load_config_or_empty
add("_load_config_or_empty", io={"load_config": ret({"a": 1})})
add("_load_config_or_empty", io={"load_config": ret(None)})
add("_load_config_or_empty", io={"import:hermes_cli.config": imp("ImportError", "x")})
add("_load_config_or_empty", io={"load_config": raise_("ValueError", "x")})
add("_load_config_or_empty", io={"load_config": raise_("SystemExit", "1")})
add("_load_config_or_empty", io={"load_config": ret({})})
add("_load_config_or_empty", io={})

# 18. _active_profile_name
add("_active_profile_name", io={"get_active_profile_name": ret("prof1")})
add("_active_profile_name", io={"get_active_profile_name": ret(None)})
add("_active_profile_name", io={"import:hermes_cli.profiles": imp("ImportError", "x")})
add("_active_profile_name", io={"get_active_profile_name": raise_("ValueError", "x")})
add("_active_profile_name", io={"get_active_profile_name": raise_("KeyboardInterrupt", "")})
add("_active_profile_name", io={"get_active_profile_name": ret("")})

# 19. _resolve_board
add("_resolve_board", board=None)
add("_resolve_board", board="")
add("_resolve_board", board="default", io={"kanban_db._normalize_board_slug": ret("default")})
add("_resolve_board", board="exists", io={"kanban_db._normalize_board_slug": ret("exists"),
                                          "kanban_db.board_exists": ret(True), "kanban_db.read_board_metadata": ret({"archived": False})})
add("_resolve_board", board="arch", io={"kanban_db._normalize_board_slug": ret("arch"),
                                        "kanban_db.board_exists": ret(True), "kanban_db.read_board_metadata": ret({"archived": True})})
add("_resolve_board", board="missing", io={"kanban_db._normalize_board_slug": ret("missing"), "kanban_db.board_exists": ret(False)})
add("_resolve_board", board="X", io={"kanban_db._normalize_board_slug": raise_("ValueError", "bad")})

# 20. _conn
add("_conn", board="b1", io={"kanban_db.init_db": ret(None), "kbc.connect": ret("conn-1")})
add("_conn", board="b1", io={"kanban_db.init_db": raise_("RuntimeError", "db locked"), "log.warning": ret(None), "kbc.connect": ret("conn-1")})
add("_conn", board="b1", io={"kanban_db.init_db": raise_("SystemExit", "1")})
add("_conn", board="b1", io={"kanban_db.init_db": ret(None), "kbc.connect": raise_("RuntimeError", "no conn")})
add("_conn", board=None, io={"kanban_db.init_db": ret(None), "kbc.connect": ret("conn-2")})
add("_conn", board="b1", io={})

# 21. _attach_diagnostics
add("_attach_diagnostics", task_d={"id": "t1"}, diags=[{"kind": "stale", "count": 1, "last_seen_at": 10, "severity": "warning"}])
add("_attach_diagnostics", task_d={"id": "t2"}, diags=[])
add("_attach_diagnostics", task_d={"id": "t3"}, diags=None)
add("_attach_diagnostics", task_d={"id": "t4", "x": 1}, diags=[{"kind": "a", "count": 2, "last_seen_at": 5, "severity": "error"},
                                                               {"kind": "b", "count": 1, "last_seen_at": 9, "severity": "critical"}])
add("_attach_diagnostics", task_d={}, diags=[{"kind": "z", "count": 1, "last_seen_at": 1, "severity": "warning"}])
add("_attach_diagnostics", task_d={"id": "t6"}, diags=[{"kind": "q", "count": 4, "last_seen_at": 20, "severity": "critical"}])

# 22. _task_dict
add("_task_dict", task=base_task(), latest_summary="sum", current_run_started_at=42,
    io={"kanban_db.task_age": ret({"created_age_seconds": 5, "started_age_seconds": None, "time_to_complete_seconds": None})})
add("_task_dict", task=base_task(id="t2", status="running"),
    io={"kanban_db.task_age": raise_("RuntimeError", "boom")})
add("_task_dict", task=base_task(id="t3"), io={"kanban_db.task_age": raise_("SystemExit", "1")})
add("_task_dict", task=base_task(id="t4"), latest_summary=None, current_run_started_at=None,
    io={"kanban_db.task_age": ret({"created_age_seconds": 1, "started_age_seconds": 2, "time_to_complete_seconds": 3})})
add("_task_dict", task=base_task(id="t5", title="Long"), latest_summary="s",
    io={"kanban_db.task_age": ret({"created_age_seconds": 0, "started_age_seconds": None, "time_to_complete_seconds": None})})
add("_task_dict", task=base_task(id="t6"), io={})

# 23. _links_for
add("_links_for", task_id="t1", io={"conn.execute": [ret([{"parent_id": "p1"}, {"parent_id": "p2"}]), ret([{"child_id": "c1"}])]})
add("_links_for", task_id="t2", io={"conn.execute": [ret([]), ret([])]})
add("_links_for", task_id="t3", io={"conn.execute": [ret([{"parent_id": "p1"}]), ret([])]})
add("_links_for", task_id="t4", io={"conn.execute": [ret([]), ret([{"child_id": "c1"}, {"child_id": "c2"}])]})
add("_links_for", task_id="t5", io={"conn.execute": [raise_("RuntimeError", "sql")]})
add("_links_for", task_id="t6", io={})

# 24. _link_tasks
add("_link_tasks", links={"parents": ["p1", "p2", "p1"], "children": ["c1", "p2"]},
    io={"kanban_db.get_task": [ret({"id": "p1", "title": "T p1", "status": "todo"}),
                                ret({"id": "p2", "title": "T p2", "status": "ready"}),
                                ret({"id": "c1", "title": "T c1", "status": "done"})]})
add("_link_tasks", links={"parents": ["p1"], "children": []}, io={"kanban_db.get_task": [ret(None)]})
add("_link_tasks", links={"parents": ["p1"], "children": ["c1"]},
    io={"kanban_db.get_task": [ret(None), ret({"id": "c1", "title": "C", "status": "todo"})]})
add("_link_tasks", links={"parents": [], "children": []}, io={})
add("_link_tasks", links={"parents": ["p1"], "children": []}, io={"kanban_db.get_task": [raise_("RuntimeError", "x")]})
add("_link_tasks", links={"parents": ["p1"], "children": []}, io={})

# 25. _drag_to
add("_drag_to", task_id="t1", s="ready", io={"kanban_db.get_task": ret({"status": "blocked"}), "kanban_db.unblock_task": ret(True)})
add("_drag_to", task_id="t1", s="todo", io={"kanban_db.get_task": ret({"status": "review"}), "kanban_db.reopen_review_task": ret(True)})
add("_drag_to", task_id="t1", s="ready", io={"kanban_db.get_task": ret({"status": "todo"}), "_set_status_direct": ret(True)})
add("_drag_to", task_id="t1", s="triage", io={"_set_status_direct": ret(True)})
add("_drag_to", task_id="t1", s="ready", io={"kanban_db.get_task": ret({"status": "scheduled"}), "kanban_db.unblock_task": ret(False)})
add("_drag_to", task_id="t1", s="ready", io={"kanban_db.get_task": raise_("RuntimeError", "x")})

# 26. _apply_status
add("_apply_status", task_id="t1", s="running", p={}, unknown_detail="x")
add("_apply_status", task_id="t1", s="bogus", p={}, unknown_detail="unknown status: bogus")
add("_apply_status", task_id="t1", s="done", p={"result": "r", "summary": "s", "metadata": None},
    io={"kanban_db.complete_task": ret(True)})
add("_apply_status", task_id="t1", s="blocked", p={"block_reason": "why"}, io={"kanban_db.block_task": ret(True)})
add("_apply_status", task_id="t1", s="review", p={"summary": "s", "metadata": None, "assignee": "rev"},
    io={"kanban_db.request_review": ret(True)})
add("_apply_status", task_id="t1", s="ready", p={}, io={"kanban_db.get_task": ret({"status": "todo"}), "_set_status_direct": ret(True)})
add("_apply_status", task_id="t1", s="triage", p={}, io={"_set_status_direct": ret(False)})

# 27. _set_priority
add("_set_priority", task_id="t1", priority=3, board="b", io={"kanban_db.edit_task": ret(None)})
add("_set_priority", task_id="t1", priority=0, board="b", io={"kanban_db.edit_task": ret(None)})
add("_set_priority", task_id="t1", priority=-2, board=None, io={"kanban_db.edit_task": ret(None)})
add("_set_priority", task_id="t2", priority=9, board="b2", io={"kanban_db.edit_task": ret(None)})
add("_set_priority", task_id="t1", priority=1, board="b", io={"kanban_db.edit_task": raise_("RuntimeError", "x")})
add("_set_priority", task_id="t1", priority=1, board="b", io={})

# 28. _apply_model_override
add("_apply_model_override", task_id="t1", p={"clear_model_override": False, "model_override": "  gpt  ", "provider_override": "openai"},
    io={"kanban_db.set_model_override": ret(True)})
add("_apply_model_override", task_id="t1", p={"clear_model_override": True, "model_override": "gpt", "provider_override": None},
    io={"kanban_db.set_model_override": ret(True)})
add("_apply_model_override", task_id="t1", p={"clear_model_override": False, "model_override": "   ", "provider_override": None},
    io={"kanban_db.set_model_override": ret(False)})
add("_apply_model_override", task_id="t1", p={"clear_model_override": False, "model_override": None, "provider_override": "p"},
    io={"kanban_db.set_model_override": ret(True)})
add("_apply_model_override", task_id="t1", p={"clear_model_override": False, "model_override": "m", "provider_override": None},
    io={"kanban_db.set_model_override": raise_("ValueError", "bad model")})
add("_apply_model_override", task_id="t1", p={"clear_model_override": False, "model_override": "m", "provider_override": None}, io={})

# 29. _apply_reasoning_effort
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": False, "reasoning_effort": "high"},
    io={"kanban_db.set_reasoning_effort": ret(True)})
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": True, "reasoning_effort": "high"},
    io={"kanban_db.set_reasoning_effort": ret(True)})
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": False, "reasoning_effort": None},
    io={"kanban_db.set_reasoning_effort": ret(False)})
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": False, "reasoning_effort": "none"},
    io={"kanban_db.set_reasoning_effort": ret(True)})
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": False, "reasoning_effort": "low"},
    io={"kanban_db.set_reasoning_effort": raise_("RuntimeError", "x")})
add("_apply_reasoning_effort", task_id="t1", p={"clear_reasoning_effort": False, "reasoning_effort": "low"}, io={})

# 30. _open_parent_refusal
add("_open_parent_refusal", task_id="t1", s="done", io={"kanban_db.unsatisfied_parents": ret([["p1", "todo"], ["p2", "ready"]])})
add("_open_parent_refusal", task_id="t1", s="review", io={"kanban_db.unsatisfied_parents": ret([["p1", "blocked"]])})
add("_open_parent_refusal", task_id="t1", s="ready", io={})
add("_open_parent_refusal", task_id="t1", s="done", io={"kanban_db.unsatisfied_parents": ret([])})
add("_open_parent_refusal", task_id="t1", s="done", io={"kanban_db.unsatisfied_parents": raise_("RuntimeError", "x")})
add("_open_parent_refusal", task_id="t1", s="review", io={})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    data = "".join(json.dumps(c, ensure_ascii=False) + "\n" for c in CASES)
    OUT.write_bytes(data.encode("utf-8"))
    # per-function counts
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases across {len(cnt)} funcs -> {OUT}")
    low = [f for f, n in cnt.items() if n < 5]
    if low:
        print("WARNING funcs with <5 cases:", low)
    for f in sorted(cnt):
        print(f"  {f}: {cnt[f]}")


if __name__ == "__main__":
    main()
