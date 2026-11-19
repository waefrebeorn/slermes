#!/usr/bin/env python3
"""Generate tests/fixtures/kanban_tools_flow.jsonl for the kanban_tools FLOW
port oracle. Each case: {"func": <live name>, "in": {...}} with scripted
seams in in.io ({"ret": ...} / {"raise": {"type","message"}}; lists are
per-call queues) and the os.environ mirror in in.state.env.

Run: python tests/gen_kanban_tools_flow_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "kanban_tools_flow.jsonl"

TE = {"ret": "ERR_PAYLOAD"}  # tool_error script: payload is arbitrary str
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def dc(ret=None, raise_=None):
    return {"raise": raise_} if raise_ else {"ret": ret}


RT = {"type": "RuntimeError", "message": "boom"}
SE = {"type": "SystemExit", "message": "bye"}

# ── _delegation_ctx ─────────────────────────────────────────────────────────
add("_delegation_ctx", predicate="is_delegated_child_context", default=False,
    io={"delegation_ctx": dc(True)})
add("_delegation_ctx", predicate="is_delegated_child_context", default=False,
    io={"delegation_ctx": dc(False)})
add("_delegation_ctx", predicate="is_dispatcher_owned_worker_context",
    default=True, io={"delegation_ctx": dc(raise_=RT)})
add("_delegation_ctx", predicate="is_delegated_child_context", default=False,
    io={"delegation_ctx": dc(raise_=SE)})
add("_delegation_ctx", predicate="is_delegated_child_context", default=True,
    state={"delegation_import_fail": True}, io={})
add("_delegation_ctx", predicate="is_delegated_child_process_context",
    default=False, io={"delegation_ctx": dc(None)})

# ── _is_dispatcher_owned_worker ─────────────────────────────────────────────
add("_is_dispatcher_owned_worker", io={"delegation_ctx": dc(True)})
add("_is_dispatcher_owned_worker", io={"delegation_ctx": dc(False)})
add("_is_dispatcher_owned_worker",
    io={"delegation_ctx": dc(raise_={"type": "ValueError", "message": "bad"})})
add("_is_dispatcher_owned_worker", io={"delegation_ctx": dc(raise_=SE)})
add("_is_dispatcher_owned_worker", state={"delegation_import_fail": True}, io={})

# ── _visible ────────────────────────────────────────────────────────────────
add("_visible", to_env_worker=True,
    io={"delegation_ctx": dc(True), "profile_has_kanban_toolset": {"ret": True}})
add("_visible", to_env_worker=True, state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": [dc(False), dc(True)]})
add("_visible", to_env_worker=False, state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": [dc(False), dc(True)]})
add("_visible", to_env_worker=True, state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": [dc(False), dc(False)],
        "profile_has_kanban_toolset": {"ret": True}})
add("_visible", to_env_worker=True,
    io={"delegation_ctx": dc(False), "profile_has_kanban_toolset": {"ret": False}})
add("_visible", to_env_worker=False,
    io={"delegation_ctx": dc(raise_=RT), "profile_has_kanban_toolset": {"ret": True}})

# ── _check ──────────────────────────────────────────────────────────────────
add("_check", cond=True, message="boom", io={"tool_error": TE})
add("_check", cond=False, message="boom", io={"tool_error": TE})
add("_check", cond=0, message="boom", io={"tool_error": TE})
add("_check", cond="", message="boom", io={"tool_error": TE})
add("_check", cond=[], message="boom", io={"tool_error": TE})
add("_check", cond=None, message="boom", io={"tool_error": TE})

# ── _require_text ───────────────────────────────────────────────────────────
add("_require_text", args={"body": "hello"}, name="body", io={"tool_error": TE})
add("_require_text", args={"body": "  "}, name="body", io={"tool_error": TE})
add("_require_text", args={}, name="body", io={"tool_error": TE})
add("_require_text", args={"body": ""}, name="body", message="need body",
    io={"tool_error": TE})
add("_require_text", args={"n": 5}, name="n", io={"tool_error": TE})
add("_require_text", args={"n": 0}, name="n", io={"tool_error": TE})
add("_require_text", args={"d": {"a": 1}}, name="d", io={"tool_error": TE})
add("_require_text", args={}, name="body", message="", io={"tool_error": TE})
add("_require_text", args={"body": " x "}, name="body", io={"tool_error": TE})

# ── _coerce_str_list ────────────────────────────────────────────────────────
add("_coerce_str_list", value=None, name="skills", what="skill names",
    io={"tool_error": TE})
add("_coerce_str_list", value="one", name="skills", what="skill names",
    io={"tool_error": TE})
add("_coerce_str_list", value=["a", "b"], name="skills", what="skill names",
    io={"tool_error": TE})
add("_coerce_str_list", value=[" a ", "", "b", "  "], name="skills",
    what="skill names", strip=True, io={"tool_error": TE})
add("_coerce_str_list", value=5, name="skills", what="skill names",
    io={"tool_error": TE})
add("_coerce_str_list", value={"a": 1}, name="skills", what="skill names",
    io={"tool_error": TE})
add("_coerce_str_list", value=[1, " 2 ", ""], name="artifacts",
    what="file paths", strip=True, io={"tool_error": TE})
add("_coerce_str_list", value=True, name="parents", what="task ids",
    io={"tool_error": TE})

# ── _require_dict_metadata ──────────────────────────────────────────────────
add("_require_dict_metadata", metadata=None, io={"tool_error": TE})
add("_require_dict_metadata", metadata={}, io={"tool_error": TE})
add("_require_dict_metadata", metadata={"a": 1}, io={"tool_error": TE})
add("_require_dict_metadata", metadata="x", io={"tool_error": TE})
add("_require_dict_metadata", metadata=[1], io={"tool_error": TE})
add("_require_dict_metadata", metadata=5, io={"tool_error": TE})

# ── _merge_artifacts ────────────────────────────────────────────────────────
add("_merge_artifacts", metadata=None, artifacts=["a.txt"], io={"tool_error": TE})
add("_merge_artifacts", metadata={"artifacts": ["a", " b "]},
    artifacts=["b", "c", ""], io={"tool_error": TE})
add("_merge_artifacts", metadata={"artifacts": "old"}, artifacts=["new"],
    io={"tool_error": TE})
add("_merge_artifacts", metadata={"x": 1}, artifacts=[], io={"tool_error": TE})
add("_merge_artifacts", metadata="bad", artifacts=["a"], io={"tool_error": TE})
add("_merge_artifacts", metadata={"artifacts": ["a", "a"]}, artifacts=["a"],
    io={"tool_error": TE})
add("_merge_artifacts", metadata={}, artifacts=[1, " p "], io={"tool_error": TE})
add("_merge_artifacts", metadata={"artifacts": [1, 2]}, artifacts=[],
    io={"tool_error": TE})

# ── _opt_int ────────────────────────────────────────────────────────────────
add("_opt_int", value=None)
add("_opt_int", value=None, default=5)
add("_opt_int", value="42")
add("_opt_int", value=" -7 ")
add("_opt_int", value="1_000")
add("_opt_int", value="abc")
add("_opt_int", value="")
add("_opt_int", value=3.9)
add("_opt_int", value=True)
add("_opt_int", value=[])
add("_opt_int", value="_1")
add("_opt_int", value="1_")

# ── _fields ─────────────────────────────────────────────────────────────────
add("_fields", obj=None, names=["id", "status"])
add("_fields", obj={"id": "t1", "status": "done", "x": 9}, names=["id", "status"])
add("_fields", obj={"id": "t1"}, names=["id", "title"])
add("_fields", obj="abc", names=["id"])
add("_fields", obj={"a": 1}, names=[])
add("_fields", obj={"a": 1}, names=["a", "a"])

# ── _redact ─────────────────────────────────────────────────────────────────
add("_redact", value="secret x", io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact", value=0, io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact", value=None, io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact", value=[1, "a"], io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact", value="x", io={"redact_sensitive_text": {"raise": RT}})

# ── _redact_opt ─────────────────────────────────────────────────────────────
add("_redact_opt", value="", io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact_opt", value=0, io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact_opt", value=None, io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact_opt", value=[], io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact_opt", value="x", io={"redact_sensitive_text": {"ret": "[R]"}})
add("_redact_opt", value={"a": 1}, io={"redact_sensitive_text": {"ret": "[R]"}})

# ── _own_task_env ───────────────────────────────────────────────────────────
add("_own_task_env", task_id="", var="HERMES_SESSION_ID",
    state={"env": {"HERMES_SESSION_ID": "s1"}}, io={"owned_kanban_task": {"ret": "t1"}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    state={"env": {"HERMES_SESSION_ID": "s1"}}, io={"owned_kanban_task": {"ret": "t1"}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    state={"env": {"HERMES_SESSION_ID": "s1"}}, io={"owned_kanban_task": {"ret": "t2"}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    state={"env": {"HERMES_SESSION_ID": "s1"}}, io={"owned_kanban_task": {"ret": None}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    io={"owned_kanban_task": {"ret": "t1"}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    io={"owned_kanban_task": {"raise": RT}})
add("_own_task_env", task_id="t1", var="HERMES_SESSION_ID",
    state={"env": {"HERMES_SESSION_ID": "s1"}}, io={"owned_kanban_task": {"ret": 5}})

# ── _reject_inherited_task_mutation ─────────────────────────────────────────
add("_reject_inherited_task_mutation", tid="t1", env_tid="",
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1", env_tid="t1",
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1", env_tid="t1",
    io={"owned_kanban_task": {"ret": "w"}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t2", env_tid="t1",
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1",
    state={"env": {"HERMES_KANBAN_TASK": " t1 "}},
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1",
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1", env_tid="t1",
    io={"owned_kanban_task": {"ret": ""}, "tool_error": TE})
add("_reject_inherited_task_mutation", tid="t1",
    state={"env": {"HERMES_KANBAN_TASK": "t9"}},
    io={"owned_kanban_task": {"ret": None}, "tool_error": TE})

# ── _require_task_id ────────────────────────────────────────────────────────
add("_require_task_id", args={"task_id": "t9"}, io={"tool_error": TE})
add("_require_task_id", args={"task_id": "  "},
    state={"env": {"HERMES_KANBAN_TASK": "t5"}},
    io={"delegation_ctx": [dc(False), dc(True)], "tool_error": TE})
add("_require_task_id", args={}, state={"env": {"HERMES_KANBAN_TASK": "t5"}},
    io={"delegation_ctx": dc(True), "tool_error": TE})
add("_require_task_id", args={}, state={"env": {"HERMES_KANBAN_TASK": "t5"}},
    io={"delegation_ctx": [dc(False), dc(False)], "tool_error": TE})
add("_require_task_id", args={},
    io={"delegation_ctx": [dc(False), dc(True)], "tool_error": TE})
add("_require_task_id", args={"task_id": 7}, io={"tool_error": TE})
add("_require_task_id", args={}, state={"env": {"HERMES_KANBAN_TASK": "   "}},
    io={"delegation_ctx": [dc(False), dc(True)], "tool_error": TE})
add("_require_task_id", args={"task_id": None},
    state={"env": {"HERMES_KANBAN_TASK": " t5 "}},
    io={"delegation_ctx": [dc(False), dc(True)], "tool_error": TE})

# ── _worker_guard ───────────────────────────────────────────────────────────
add("_worker_guard", tool_name="kanban_complete", args={"task_id": "t1"},
    state={"env": {"HERMES_KANBAN_TASK": "t1", "HERMES_KANBAN_RUN_ID": "7"}},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": "t1"},
        "tool_error": TE})
add("_worker_guard", tool_name="kanban_complete", args={"task_id": "t1"},
    state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": dc(True), "tool_error": TE})
add("_worker_guard", tool_name="kanban_block", args={},
    io={"delegation_ctx": [dc(False), dc(False), dc(True)], "tool_error": TE})
add("_worker_guard", tool_name="kanban_complete", args={"task_id": "t2"},
    state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": "t1"},
        "tool_error": TE})
add("_worker_guard", tool_name="kanban_complete", args={"task_id": "t1"},
    state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": None},
        "tool_error": TE})
add("_worker_guard", tool_name="kanban_schedule", args={"task_id": "t1"},
    state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": "t1"},
        "tool_error": TE})
add("_worker_guard", tool_name="kanban_heartbeat", args={"task_id": "t1"},
    state={"env": {"HERMES_KANBAN_TASK": "t1"}},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": "t1"},
        "tool_error": TE})
add("_worker_guard", tool_name="kanban_complete", args={"task_id": "t9"},
    io={"delegation_ctx": dc(False), "owned_kanban_task": {"ret": None},
        "tool_error": TE})

# ── _kanban_handler ─────────────────────────────────────────────────────────
SCH = {"ret": {"parameters": {"properties": {
    "task_id": {}, "body": {}, "board": {}}}}}
add("_kanban_handler", tool_name="kanban_comment",
    args={"task_id": "t1", "body": "x"},
    io={"registry.get_schema": SCH, "inner_fn": {"ret": "{\"ok\": true}"},
        "tool_error": TE})
add("_kanban_handler", tool_name="kanban_comment",
    args={"task_id": "t1", "bogus": 1},
    io={"registry.get_schema": SCH, "tool_error": TE})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": SCH,
        "inner_fn": {"raise": {"type": "_Reject", "message": "PAYLOAD"}},
        "tool_error": TE})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": SCH,
        "inner_fn": {"raise": {"type": "ValueError", "message": "bad"}},
        "tool_error": {"ret": "rendered"}})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": SCH,
        "inner_fn": {"raise": RT}, "tool_error": {"ret": "rendered"}})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": SCH,
        "inner_fn": {"raise": {"type": "SystemExit", "message": "x"}},
        "tool_error": TE})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": {"ret": {"parameters": {}}},
        "tool_error": {"ret": "rendered"}})
add("_kanban_handler", tool_name="kanban_create",
    args={"title": "t", "assignee": "a", "session_id": "s"},
    io={"registry.get_schema": {"ret": {"parameters": {"properties": {
            "title": {}, "assignee": {}}}}},
        "inner_fn": {"ret": "OK"}, "tool_error": TE})
add("_kanban_handler", tool_name="kanban_attach_url",
    args={"url": "u", "title": "t"},
    io={"registry.get_schema": {"ret": {"parameters": {"properties": {
            "url": {}, "filename": {}}}}},
        "inner_fn": {"ret": "OK"}, "tool_error": TE})
add("_kanban_handler", tool_name="kanban_comment", args={"body": "x"},
    io={"registry.get_schema": {"ret": None}, "tool_error": {"ret": "rendered"}})
add("_kanban_handler", tool_name="kanban_comment",
    args={"zeta": 1, "alpha": 2},
    io={"registry.get_schema": SCH, "tool_error": TE})

# ── _ok_landed ──────────────────────────────────────────────────────────────
add("_ok_landed", task_id="t1", default_status="blocked",
    io={"kb.latest_run": {"ret": {"id": 3}},
        "kb.get_task": {"ret": {"status": "done"}}})
add("_ok_landed", task_id="t1", default_status="blocked",
    io={"kb.latest_run": {"ret": None},
        "kb.get_task": {"ret": {"status": "review"}}})
add("_ok_landed", task_id="t1", default_status="scheduled",
    io={"kb.latest_run": {"ret": {"id": 3}}, "kb.get_task": {"ret": None}})
add("_ok_landed", task_id="t1", default_status="blocked",
    extra={"reason": "r", "block_kind": "needs_input"},
    io={"kb.latest_run": {"ret": {"id": 3}},
        "kb.get_task": {"ret": {"status": "blocked"}}})
add("_ok_landed", task_id="t1", default_status="blocked",
    io={"kb.latest_run": {"ret": {"xid": 1}},
        "kb.get_task": {"ret": {"status": "done"}}})
add("_ok_landed", task_id="t1", default_status="blocked",
    io={"kb.latest_run": {"ret": {"id": 3}},
        "kb.get_task": {"ret": {"status": None}}})
add("_ok_landed", task_id="t1", default_status="blocked",
    extra={"implementer": "dev"},
    io={"kb.latest_run": {"ret": None}, "kb.get_task": {"ret": None}})
add("_ok_landed", task_id="t1", default_status="blocked",
    extra={"note": "h\u00e9llo"},
    io={"kb.latest_run": {"ret": None},
        "kb.get_task": {"ret": {"status": "done"}}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    for fn, n in sorted(counts.items()):
        print(f"  {fn}: {n}")
    assert all(n >= 5 for n in counts.values()), "each func needs >=5 cases"


if __name__ == "__main__":
    main()
