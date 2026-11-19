#!/usr/bin/env python3
"""gen_kanban_adapter_flow_cases.py — companion generator for the kanban
adapter UNCLAIMED-remainder FLOW port (src/cli/kanban_adapter_flow.c).

Emits tests/fixtures/kanban_adapter_flow.jsonl — the scripted-seam case file
the runner auto-feeds to tests/t_port_kanban_adapter_flow.c (>=5 cases per
ported func; 33 funcs). Every script shape mirrors the oracle's recording
stubs: {"ret": v} / {"raise": {"type","message"}} / a per-call queue LIST;
execute scripts carry {"rows": [...], "fetch_raise": {...}} cursor specs;
"import:<module>" gates carry {"import_raise": {...}} (None-in-sys.modules
halt, message byte-pinned to CPython).

--check-sql verifies every KA_SQL_* macro in the port .c against the LIVE
kanban_db.py AST (static concat / f-string static parts / AugAssign frags).

Usage: python tests/gen_kanban_adapter_flow_cases.py [--check-sql] [--sql-only]
"""
import ast
import base64
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "kanban_db.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "kanban_db.py"
PORT = REPO / "src" / "cli" / "kanban_adapter_flow.c"
OUT = HERE / "fixtures" / "kanban_adapter_flow.jsonl"

CASES = []


def case(func, inp=None, io=None):
    CASES.append({"func": func, "in": {**(inp or {}), "io": io or {}}})


def ret(v):
    return {"ret": v}


def raise_(t, m=""):
    return {"raise": {"type": t, "message": m}}


def rows(rs, fetch_raise=None):
    spec = {"rows": rs}
    if fetch_raise is not None:
        spec["fetch_raise"] = fetch_raise
    return ret(spec)


def b64(b):
    return {"__bytes__": base64.b64encode(b).decode("ascii")}


def halt(mod):
    return {"import_raise": {"type": "ModuleNotFoundError",
                             "message": f"import of {mod} halted; None in sys.modules"}}


# ── _lossy_text ──────────────────────────────────────────────────────────────
case("_lossy_text", {"value": "hello"})
case("_lossy_text", {"value": 42})
case("_lossy_text", {"value": None})
case("_lossy_text", {"value": [1, "a"]})
case("_lossy_text", {"value": {"a": 1}})
case("_lossy_text", {"value": b64("héllo😀".encode("utf-8"))})
case("_lossy_text", {"value": b64(b"\xff\xfe")})
case("_lossy_text", {"value": b64(b"\xe0\xa0")})
case("_lossy_text", {"value": b64(b"a\x80b\xc2\x41\xf0\x90\x80")})
case("_lossy_text", {"value": b64(b"")})
case("_lossy_text", {"value": {"__bytes__": "aGk=", "extra": 1}})

# ── _row_get ─────────────────────────────────────────────────────────────────
case("_row_get", {"row": {"a": 1, "b": 2}, "col": "a"})
case("_row_get", {"row": {"a": 1}, "col": "z", "default": "d"})
case("_row_get", {"row": None, "col": "a", "default": 7})
case("_row_get", {"row": {"a": 1}, "col": "z"})
case("_row_get", {"row": {"a": None}, "col": "a", "default": "d"})
case("_row_get", {"row": [1, 2], "col": "a"})
case("_row_get", {"row": {"a": {"n": [1]}}, "col": "a", "default": {"x": 1}})

# ── _json_or ─────────────────────────────────────────────────────────────────
case("_json_or", {"value": '{"a": 1, "b": [2, 3]}'})
case("_json_or", {"value": "[1, 2, 3]"})
case("_json_or", {"value": "42"})
case("_json_or", {"value": '"hi"'})
case("_json_or", {"value": "true"})
case("_json_or", {"value": "null", "default": {"d": 1}})
case("_json_or", {"value": "", "default": "D"})
case("_json_or", {"value": None, "default": [1]})
case("_json_or", {"value": "{bad", "default": None})
case("_json_or", {"value": "01", "default": "strict"})
case("_json_or", {"value": 5, "default": "ns"})
case("_json_or", {"value": [1], "default": "ns"})
case("_json_or", {"value": " 1 ", "default": 0})
case("_json_or", {"value": '"\\q"', "default": "esc"})
case("_json_or", {"value": '{"a": 1, "a": 2}'})

# ── _json_dict ───────────────────────────────────────────────────────────────
case("_json_dict", {"value": '{"a": 1}'})
case("_json_dict", {"value": "[1, 2]"})
case("_json_dict", {"value": "5"})
case("_json_dict", {"value": "{bad"})
case("_json_dict", {"value": None})
case("_json_dict", {"value": ""})

# ── _env_int ─────────────────────────────────────────────────────────────────
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("42")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("abc")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret(" 7 ")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("-5")})
case("_env_int", {"name": "N", "default": 9, "minimum": -10}, {"environ.get": ret("-5")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("1_000")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": ret("+3")})
case("_env_int", {"name": "N", "default": 9}, {"environ.get": raise_("OSError", "env")})

# ── _first_line ──────────────────────────────────────────────────────────────
case("_first_line", {"text": "hello world", "limit": 5})
case("_first_line", {"text": "  \n first \n second", "limit": 100})
case("_first_line", {"text": "", "limit": 5})
case("_first_line", {"text": None, "limit": 5})
case("_first_line", {"text": "   ", "limit": 5})
case("_first_line", {"text": "abcdef", "limit": 0})
case("_first_line", {"text": "abcdef", "limit": -2})
case("_first_line", {"text": "abcdef", "limit": -100})
case("_first_line", {"text": "a\rb", "limit": 10})
case("_first_line", {"text": "a\x85b", "limit": 10})
case("_first_line", {"text": "héllo", "limit": 3})
case("_first_line", {"text": "a\r\nb", "limit": 10})

# ── _opt_int ─────────────────────────────────────────────────────────────────
case("_opt_int", {"value": None})
case("_opt_int", {"value": 5})
case("_opt_int", {"value": 2.9})
case("_opt_int", {"value": -2.9})
case("_opt_int", {"value": "42"})
case("_opt_int", {"value": " 7 "})
case("_opt_int", {"value": True})
case("_opt_int", {"value": "abc"})
case("_opt_int", {"value": [1]})
case("_opt_int", {"value": "1_2"})

# ── _json_or_null ────────────────────────────────────────────────────────────
case("_json_or_null", {"obj": {"a": 1, "b": [True, None]}})
case("_json_or_null", {"obj": [1, 2, 3]})
case("_json_or_null", {"obj": "hi"})
case("_json_or_null", {"obj": 0})
case("_json_or_null", {"obj": ""})
case("_json_or_null", {"obj": []})
case("_json_or_null", {"obj": {}})
case("_json_or_null", {"obj": None})
case("_json_or_null", {"obj": False})
case("_json_or_null", {"obj": {"k": 0.5}})
case("_json_or_null", {"obj": {"x": {"y": [1, "z"]}}})
case("_json_or_null", {"obj": {"é": "😀"}})
case("_json_or_null", {"obj": 7})

# ── _substantive_text ────────────────────────────────────────────────────────
for v in (None, "", "  ", "x", 0, False, [], {}, " a "):
    case("_substantive_text", {"value": v})

# ── _nonblank_str ────────────────────────────────────────────────────────────
for v in ("x", " x ", "", "  ", None, 5, [1]):
    case("_nonblank_str", {"value": v})

# ── _cleaned_artifact_paths ──────────────────────────────────────────────────
case("_cleaned_artifact_paths", {"metadata": None})
case("_cleaned_artifact_paths", {"metadata": [1]})
case("_cleaned_artifact_paths", {"metadata": {"other": 1}})
case("_cleaned_artifact_paths", {"metadata": {"artifacts": "x"}})
case("_cleaned_artifact_paths", {"metadata": {"artifacts": [" a ", "", "b", 5, None, "  "]}})
case("_cleaned_artifact_paths", {"metadata": {"artifacts": []}})
case("_cleaned_artifact_paths", {"metadata": {"artifacts": ["a/b.txt", " c "]}})

# ── _retention_seconds ───────────────────────────────────────────────────────
case("_retention_seconds", {"older_than_seconds": 0})
case("_retention_seconds", {"older_than_seconds": 5})
case("_retention_seconds", {"older_than_seconds": -1})
case("_retention_seconds", {"older_than_seconds": "10"})
case("_retention_seconds", {"older_than_seconds": "-3"})
case("_retention_seconds", {"older_than_seconds": 2.9})
case("_retention_seconds", {"older_than_seconds": True})
case("_retention_seconds", {"older_than_seconds": "abc"})
case("_retention_seconds", {"older_than_seconds": None})

# ── _slug_or_default ─────────────────────────────────────────────────────────
case("_slug_or_default", {"board": "x"}, {"_normalize_board_slug": ret("x")})
case("_slug_or_default", {"board": None}, {"_normalize_board_slug": ret(None)})
case("_slug_or_default", {"board": ""}, {"_normalize_board_slug": ret("")})
case("_slug_or_default", {"board": "bad"}, {"_normalize_board_slug": raise_("ValueError", "invalid board slug")})
case("_slug_or_default", {"board": "B"}, {"_normalize_board_slug": ret("b")})
case("_slug_or_default", {"board": "x"}, {"_normalize_board_slug": raise_("SystemExit", "bye")})

# ── _require_slug ────────────────────────────────────────────────────────────
case("_require_slug", {"slug": "x"}, {"_normalize_board_slug": ret("x")})
case("_require_slug", {"slug": ""}, {"_normalize_board_slug": ret(None)})
case("_require_slug", {"slug": "  "}, {"_normalize_board_slug": ret("")})
case("_require_slug", {"slug": "bad"}, {"_normalize_board_slug": raise_("ValueError", "invalid board slug 'bad'")})
case("_require_slug", {"slug": "abc"}, {"_normalize_board_slug": ret("abc")})

# ── _host_prefix ─────────────────────────────────────────────────────────────
case("_host_prefix", {}, {"_claimer_id": ret("host:123")})
case("_host_prefix", {}, {"_claimer_id": ret("host")})
case("_host_prefix", {}, {"_claimer_id": ret(":123")})
case("_host_prefix", {}, {"_claimer_id": ret("a:b:c")})
case("_host_prefix", {}, {"_claimer_id": raise_("RuntimeError", "no host")})
case("_host_prefix", {}, {"_claimer_id": ret(None)})

# ── _board_meta_for ──────────────────────────────────────────────────────────
case("_board_meta_for", {"board": "b"}, {"read_board_metadata": ret({"x": 1})})
case("_board_meta_for", {"board": None}, {"get_current_board": ret("cur"), "read_board_metadata": ret({"m": 2})})
case("_board_meta_for", {"board": ""}, {"get_current_board": ret("cur"), "read_board_metadata": ret({})})
case("_board_meta_for", {"board": None}, {"get_current_board": raise_("RuntimeError", "no board")})
case("_board_meta_for", {"board": "b"}, {"read_board_metadata": raise_("OSError", "io")})
case("_board_meta_for", {"board": "b"}, {"read_board_metadata": ret({})})

# ── _hook_profile_name ───────────────────────────────────────────────────────
case("_hook_profile_name", {}, {"get_active_profile_name": ret("alice")})
case("_hook_profile_name", {}, {"get_active_profile_name": raise_("ValueError", "bad")})
case("_hook_profile_name", {}, {"get_active_profile_name": raise_("SystemExit", "bye")})
case("_hook_profile_name", {}, {"import:hermes_cli.profiles": halt("hermes_cli.profiles")})
case("_hook_profile_name", {}, {"import:hermes_cli.profiles": {"import_raise": {"type": "SystemExit", "message": "bye"}}})
case("_hook_profile_name", {}, {"get_active_profile_name": raise_("KeyboardInterrupt", "")})

# ── _fire_task_hook ──────────────────────────────────────────────────────────
case("_fire_task_hook", {"event": "created", "task_id": "t1", "task": {"assignee": "w"}, "run_id": None},
     {"get_current_board": ret("default"), "_fire_kanban_lifecycle_hook": ret(None)})
case("_fire_task_hook", {"event": "done", "task_id": "t2", "task": None, "run_id": 7},
     {"get_current_board": ret("b"), "_fire_kanban_lifecycle_hook": ret(None)})
case("_fire_task_hook", {"event": "blocked", "task_id": "t3", "task": {"assignee": None}, "run_id": None},
     {"get_current_board": ret("default"), "_fire_kanban_lifecycle_hook": ret(None)})
case("_fire_task_hook", {"event": "created", "task_id": "t4", "task": {"assignee": "w"}, "run_id": None,
                         "fields": {"extra": 1, "note": "n"}},
     {"get_current_board": ret("default"), "_fire_kanban_lifecycle_hook": ret(None)})
case("_fire_task_hook", {"event": "created", "task_id": "t5", "task": {"assignee": "w"}, "run_id": None},
     {"get_current_board": raise_("RuntimeError", "no board")})
case("_fire_task_hook", {"event": "created", "task_id": "t6", "task": {"assignee": "w"}, "run_id": None},
     {"get_current_board": ret("default"), "_fire_kanban_lifecycle_hook": raise_("OSError", "hook")})
case("_fire_task_hook", {"event": "created", "task_id": "t7", "task": {}, "run_id": None},
     {"get_current_board": ret("default")})
case("_fire_task_hook", {"event": "created", "task_id": "t8", "task": {"assignee": "w"}, "run_id": None},
     {"get_current_board": ret("default"), "_fire_kanban_lifecycle_hook": raise_("SystemExit", "bye")})

# ── _explicit_board_slug ─────────────────────────────────────────────────────
case("_explicit_board_slug", {"board": "b"}, {"_normalize_board_slug": ret("b")})
case("_explicit_board_slug", {"board": "bad"}, {"_normalize_board_slug": raise_("ValueError", "invalid")})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": ret(None)})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": ret("  ")})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": ret(" B "), "_normalize_board_slug": ret("b")})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": ret("B"), "_normalize_board_slug": raise_("ValueError", "invalid")})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": ret("B"), "_normalize_board_slug": raise_("TypeError", "not str")})
case("_explicit_board_slug", {"board": None}, {"current_board_override.get": raise_("RuntimeError", "ctx")})
case("_explicit_board_slug", {"board": ""}, {"_normalize_board_slug": ret(None)})

# ── _explicit_board_intent_pinned ────────────────────────────────────────────
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(True)})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(False), "explicit_board_intent_is_pinned": ret(True)})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(False), "explicit_board_intent_is_pinned": ret(False)})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(None), "explicit_board_intent_is_pinned": ret(True)})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(False), "import:agent.delegation_context": halt("agent.delegation_context")})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": raise_("RuntimeError", "ctx")})
case("_explicit_board_intent_pinned", {}, {"pin_first.get": ret(False), "explicit_board_intent_is_pinned": raise_("OSError", "x")})

# ── _board_path ──────────────────────────────────────────────────────────────
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret("/pin/db"), "current_board_override.get": ret(None), "expanduser": ret("/pin/db")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": "b", "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret("/pin/db"), "_normalize_board_slug": ret("b"), "pin_first.get": ret(True), "expanduser": ret("/pin/db")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": "b", "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret("/pin/db"), "_normalize_board_slug": ret("b"), "pin_first.get": ret(False),
      "explicit_board_intent_is_pinned": ret(False), "board_dir": ret("/boards/b")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": "b", "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret("/pin/db"), "_normalize_board_slug": ret("b"), "pin_first.get": ret(False),
      "explicit_board_intent_is_pinned": ret(True), "expanduser": ret("/pin/db")})
case("_board_path", {"env_var": None, "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"current_board_override.get": ret(None), "get_current_board": ret("default"), "kanban_home": ret("/root")})
case("_board_path", {"env_var": None, "board": None, "default_parts": ["kanban", "workspaces"], "leaf": "workspaces"},
     {"current_board_override.get": ret(None), "get_current_board": ret("other"), "board_dir": ret("/boards/other")})
case("_board_path", {"env_var": "", "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"current_board_override.get": ret(None), "get_current_board": ret("default"), "kanban_home": ret("/root")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret(""), "current_board_override.get": ret(None), "get_current_board": ret("default"), "kanban_home": ret("/root")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": ret("/a/./b/"), "current_board_override.get": ret(None), "expanduser": ret("/home/u/x")})
case("_board_path", {"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"environ.get": raise_("OSError", "env")})
case("_board_path", {"env_var": None, "board": "bad", "default_parts": ["kanban.db"], "leaf": "kanban.db"},
     {"_normalize_board_slug": raise_("ValueError", "invalid")})
case("_board_path", {"env_var": None, "board": None, "default_parts": ["kanban", "logs"], "leaf": "logs"},
     {"current_board_override.get": ret("scoped"), "_normalize_board_slug": ret("scoped"), "board_dir": ret("/boards/scoped")})

# ── _link ────────────────────────────────────────────────────────────────────
case("_link", {"parent_id": "p", "child_id": "c"}, {"execute": rows([])})
case("_link", {"parent_id": "p", "child_id": "c"}, {"execute": raise_("OperationalError", "locked")})
case("_link", {"parent_id": "t_a", "child_id": "t_b"}, {"execute": rows([])})
case("_link", {"parent_id": "", "child_id": ""}, {"execute": rows([])})
case("_link", {"parent_id": "p", "child_id": "p"}, {"execute": rows([])})

# ── _linked_ids ──────────────────────────────────────────────────────────────
case("_linked_ids", {"want": "child_id", "where": "parent_id", "task_id": "t1"},
     {"execute": rows([{"child_id": "c1"}, {"child_id": "c2"}])})
case("_linked_ids", {"want": "child_id", "where": "parent_id", "task_id": "t1"}, {"execute": rows([])})
case("_linked_ids", {"want": "child_id", "where": "parent_id", "task_id": "t1"},
     {"execute": rows([{"other": "x"}])})
case("_linked_ids", {"want": "child_id", "where": "parent_id", "task_id": "t1"},
     {"execute": raise_("OperationalError", "locked")})
case("_linked_ids", {"want": "child_id", "where": "parent_id", "task_id": "t1"},
     {"execute": rows([{"child_id": "c1"}], fetch_raise={"type": "OperationalError", "message": "fetch"})})
case("_linked_ids", {"want": "parent_id", "where": "child_id", "task_id": "t2"},
     {"execute": rows([{"parent_id": "p1"}])})

# ── _require_task ────────────────────────────────────────────────────────────
case("_require_task", {"task_id": "t1"}, {"execute": rows([{"1": 1}])})
case("_require_task", {"task_id": "t1"}, {"execute": rows([])})
case("_require_task", {"task_id": "t1"}, {"execute": rows([{}])})
case("_require_task", {"task_id": "t1"}, {"execute": raise_("OperationalError", "locked")})
case("_require_task", {"task_id": "t1"}, {"execute": rows([{"1": 1}], fetch_raise={"type": "OperationalError", "message": "f"})})
case("_require_task", {"task_id": "t_missing"}, {"execute": rows([])})

# ── _task_rows ───────────────────────────────────────────────────────────────
case("_task_rows", {"table": "task_comments", "task_id": "t1", "order": "created_at"},
     {"execute": rows([{"id": 1, "body": "a"}, {"id": 2, "body": "b"}])})
case("_task_rows", {"table": "task_events", "task_id": "t1", "order": "id"}, {"execute": rows([])})
case("_task_rows", {"table": "task_runs", "task_id": "t1", "order": "id"}, {"execute": raise_("OperationalError", "x")})
case("_task_rows", {"table": "task_comments", "task_id": "t1", "order": "id"},
     {"execute": rows([{"id": 1}], fetch_raise={"type": "OperationalError", "message": "f"})})
case("_task_rows", {"table": "task_attachments", "task_id": "t9", "order": "created_at"},
     {"execute": rows([{"filename": "f.txt"}])})

# ── _latest_event ────────────────────────────────────────────────────────────
case("_latest_event", {"task_id": "t1", "kind": "completed"}, {"execute": rows([{"payload": "{}"}])})
case("_latest_event", {"task_id": "t1", "kind": "completed", "run_id": 5}, {"execute": rows([{"payload": "p"}])})
case("_latest_event", {"task_id": "t1", "kind": "completed", "run_id": "7"}, {"execute": rows([{"payload": "p"}])})
case("_latest_event", {"task_id": "t1", "kind": "completed", "run_id": "abc"})
case("_latest_event", {"task_id": "t1", "kind": "blocked"}, {"execute": rows([])})
case("_latest_event", {"task_id": "t1", "kind": "blocked"}, {"execute": raise_("OperationalError", "x")})
case("_latest_event", {"task_id": "t1", "kind": "blocked", "run_id": 2},
     {"execute": rows([{"payload": "p"}], fetch_raise={"type": "OperationalError", "message": "f"})})

# ── _counts_by_assignee ──────────────────────────────────────────────────────
case("_counts_by_assignee", {}, {"execute": rows([
    {"assignee": "a", "status": "todo", "n": 2},
    {"assignee": "a", "status": "done", "n": 1},
    {"assignee": "b", "status": "todo", "n": 3}])})
case("_counts_by_assignee", {}, {"execute": rows([])})
case("_counts_by_assignee", {}, {"execute": rows([
    {"assignee": "a", "status": "todo", "n": 2},
    {"assignee": "a", "status": "todo", "n": 5}])})
case("_counts_by_assignee", {}, {"execute": rows([{"assignee": "a", "status": "todo"}])})
case("_counts_by_assignee", {}, {"execute": rows([{"assignee": "a", "status": "todo", "n": "5"}])})
case("_counts_by_assignee", {}, {"execute": raise_("OperationalError", "x")})
case("_counts_by_assignee", {}, {"execute": rows([{"assignee": "a", "status": "todo", "n": 1}],
                                                 fetch_raise={"type": "OperationalError", "message": "it"})})

# ── current_run_started_ats ──────────────────────────────────────────────────
case("current_run_started_ats", {"task_ids": []})
case("current_run_started_ats", {"task_ids": ["t1", "t2"]},
     {"execute": rows([{"task_id": "t1", "started_at": 10}, {"task_id": "t2", "started_at": 20}])})
case("current_run_started_ats", {"task_ids": ["t1", "t2"]},
     {"execute": rows([{"task_id": "t1", "started_at": 10}])})
case("current_run_started_ats", {"task_ids": ["t1"]},
     {"execute": rows([{"task_id": "t1", "started_at": 10}, {"task_id": "t1", "started_at": 99}])})
case("current_run_started_ats", {"task_ids": ["t1"]}, {"execute": rows([{"task_id": "t1"}])})
case("current_run_started_ats", {"task_ids": ["t1"]}, {"execute": raise_("OperationalError", "x")})
case("current_run_started_ats", {"task_ids": ["t1", "t2", "t3"]},
     {"execute": rows([{"task_id": "t2", "started_at": 7}])})

# ── _end_or_synthesize_run ───────────────────────────────────────────────────
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True},
     {"_end_run": ret(5)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": False},
     {"_end_run": ret(None)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True,
                                "summary": "s", "metadata": {"k": 1}},
     {"_end_run": ret(None), "_synthesize_ended_run": ret(9)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "failed", "status": "failed", "synthesize": True,
                                "profile": "p"},
     {"_end_run": ret(None), "_synthesize_ended_run": ret(3)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True,
                                "profile": None},
     {"_end_run": ret(None), "_synthesize_ended_run": ret(4)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True},
     {"_end_run": ret(0)})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True},
     {"_end_run": raise_("OperationalError", "x")})
case("_end_or_synthesize_run", {"task_id": "t1", "outcome": "done", "status": "done", "synthesize": True},
     {"_end_run": ret(None), "_synthesize_ended_run": raise_("OperationalError", "y")})

# ── _gate_created_cards ──────────────────────────────────────────────────────
case("_gate_created_cards", {"task_id": "t1", "created_cards": None})
case("_gate_created_cards", {"task_id": "t1", "created_cards": []})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["c1"]},
     {"_verify_created_cards": ret([["c1"], []])})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["c1", "p1"], "preview_text": "line1\nline2"},
     {"_verify_created_cards": ret([["c1"], ["p1"]]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["p1"], "preview_text": ""},
     {"_verify_created_cards": ret([[], ["p1"]]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["p1"], "preview_text": None},
     {"_verify_created_cards": ret([[], ["p1"]]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["c1"]},
     {"_verify_created_cards": ret(None)})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["c1"]},
     {"_verify_created_cards": ret([["a"]])})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["c1"]},
     {"_verify_created_cards": raise_("OperationalError", "x")})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["p1"], "preview_text": "s"},
     {"_verify_created_cards": ret([[], ["p1"]]),
      "write_txn.enter": ret(None), "_append_event": raise_("OperationalError", "ev"), "write_txn.exit": ret(False)})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["p1"]},
     {"_verify_created_cards": ret([[], ["p1"]]), "write_txn.enter": raise_("OperationalError", "enter")})
case("_gate_created_cards", {"task_id": "t1", "created_cards": ["p1"]},
     {"_verify_created_cards": ret([[], ["p1"]]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": raise_("RuntimeError", "exit")})

# ── _flag_phantom_prose_refs ─────────────────────────────────────────────────
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": None, "result": None, "verified_cards": []})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "", "result": "", "verified_cards": []})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "see t_a", "result": None, "verified_cards": []},
     {"_scan_prose_for_phantom_ids": ret([])})
case("_flag_phantom_prose_refs", {"task_id": "t1", "run_id": 5, "summary": "see t_a", "result": None, "verified_cards": []},
     {"_scan_prose_for_phantom_ids": ret(["t_a"]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "a", "result": "b", "verified_cards": ["t_a"]},
     {"_scan_prose_for_phantom_ids": ret(["t_a", "t_b"]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "t_a", "result": None, "verified_cards": ["t_a"]},
     {"_scan_prose_for_phantom_ids": ret(["t_a"])})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "x", "result": None, "verified_cards": []},
     {"_scan_prose_for_phantom_ids": raise_("OperationalError", "x")})
case("_flag_phantom_prose_refs", {"task_id": "t1", "summary": "t_z", "result": None, "verified_cards": []},
     {"_scan_prose_for_phantom_ids": ret(["t_z"]),
      "write_txn.enter": ret(None), "_append_event": raise_("OperationalError", "ev"), "write_txn.exit": ret(False)})
case("_flag_phantom_prose_refs", {"task_id": "t1", "run_id": None, "summary": "t_a t_a", "result": None, "verified_cards": []},
     {"_scan_prose_for_phantom_ids": ret(["t_a", "t_a"]),
      "write_txn.enter": ret(None), "_append_event": ret(None), "write_txn.exit": ret(False)})

# ── _git_out ─────────────────────────────────────────────────────────────────
case("_git_out", {"cwd": "/repo", "args": ["rev-parse", "HEAD"]},
     {"subprocess.run": ret({"returncode": 0, "stdout": " out \n"})})
case("_git_out", {"cwd": "/repo", "args": ["status"]},
     {"subprocess.run": ret({"returncode": 1, "stdout": "x"})})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": ret({"returncode": 0, "stdout": ""})})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": ret({"returncode": 0, "stdout": "   "})})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": ret({"returncode": 0, "stdout": None})})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": raise_("OSError", "no git")})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": raise_("SystemExit", "bye")})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": raise_("ValueError", "bad")})
case("_git_out", {"cwd": "/repo", "args": []},
     {"subprocess.run": ret({"stdout": "x"})})
case("_git_out", {"cwd": "/repo", "args": ["log", "-1"], "timeout": 10},
     {"subprocess.run": ret({"returncode": 0, "stdout": "abc"})})

# ── _project_branch_name ─────────────────────────────────────────────────────
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": "T"},
     {"branch_name_for": ret("slug/t1")})
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": None},
     {"branch_name_for": ret("slug/t1")})
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": ""},
     {"branch_name_for": ret("slug/t1")})
case("_project_branch_name", {"project_obj": None, "task_id": "t1", "title": "T"},
     {"branch_name_for": raise_("ValueError", "bad slug")})
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": "T"},
     {"branch_name_for": raise_("SystemExit", "bye")})
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": "T"},
     {"import:hermes_cli.projects_db": halt("hermes_cli.projects_db")})
case("_project_branch_name", {"project_obj": {"id": "p1"}, "task_id": "t1", "title": "T"},
     {"import:hermes_cli.projects_db": {"import_raise": {"type": "SystemExit", "message": "bye"}}})


# ── SQL verification (--check-sql) ───────────────────────────────────────────

def c_unescape(body):
    """Decode a C string-literal body (concatenated literals already joined)."""
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == "\\":
            i += 1
            e = body[i]
            if e == "n": out.append("\n")
            elif e == "r": out.append("\r")
            elif e == "t": out.append("\t")
            elif e == "\\": out.append("\\")
            elif e == '"': out.append('"')
            elif e == "x":
                j = i + 1
                h = ""
                while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                    h += body[j]; j += 1
                out.append(chr(int(h, 16)))
                i = j - 1
            else: out.append(e)
        else:
            out.append(c)
        i += 1
    return "".join(out)


def port_macros():
    text = PORT.read_text(encoding="utf-8")
    macros = {}
    for m in re.finditer(r"#define\s+(KA_SQL_[A-Z_]+)\s+((?:[^\n]*\\\n)*[^\n]*)", text):
        name, body = m.group(1), m.group(2)
        lits = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
        macros[name] = c_unescape("".join(lits))
    return macros


def const_str(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        l, r = const_str(node.left), const_str(node.right)
        if l is not None and r is not None:
            return l + r
    return None


def fstring_consts(node):
    if isinstance(node, ast.JoinedStr):
        return [v.value for v in node.values if isinstance(v, ast.Constant)]
    return None


def live_sql():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    fns = {n.name: n for n in tree.body if isinstance(n, ast.FunctionDef)}
    want = {}

    def exec_arg(name):
        for node in ast.walk(fns[name]):
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
                    and node.func.attr == "execute" and node.args:
                return node.args[0]
        return None

    want["KA_SQL_LINK"] = const_str(exec_arg("_link"))
    want["KA_SQL_REQUIRE_TASK"] = const_str(exec_arg("_require_task"))
    # _latest_event: Assign base + AugAssign run + execute concat order
    base = run = order = None
    for node in ast.walk(fns["_latest_event"]):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == "sql":
                    base = const_str(node.value)
        if isinstance(node, ast.AugAssign) and isinstance(node.target, ast.Name) \
                and node.target.id == "sql":
            run = const_str(node.value)
    ea = exec_arg("_latest_event")
    if isinstance(ea, ast.BinOp):
        order = const_str(ea.right)
    want["KA_SQL_LATEST_BASE"] = base
    want["KA_SQL_LATEST_RUN"] = run
    want["KA_SQL_LATEST_ORDER"] = order
    want["KA_SQL_COUNTS"] = const_str(exec_arg("_counts_by_assignee"))
    cs = fstring_consts(exec_arg("current_run_started_ats"))
    if cs and len(cs) >= 2:
        want["KA_SQL_RUN_STARTED_PRE"] = cs[0]
        want["KA_SQL_RUN_STARTED_POST"] = cs[-1]
    cs = fstring_consts(exec_arg("_linked_ids"))
    if cs and len(cs) >= 3:
        want["KA_SQL_LINKED_PRE"] = cs[0]
        want["KA_SQL_LINKED_MID"] = cs[1]
        want["KA_SQL_LINKED_POST"] = cs[2]
    cs = fstring_consts(exec_arg("_task_rows"))
    if cs and len(cs) >= 2:
        want["KA_SQL_TASK_ROWS_PRE"] = cs[0]
        want["KA_SQL_TASK_ROWS_MID"] = cs[1]
    return want


def check_sql():
    macros = port_macros()
    want = live_sql()
    bad = 0
    for name, w in sorted(want.items()):
        got = macros.get(name)
        if w is None:
            print(f"SQLSKIP {name}: live extraction failed")
            bad += 1
        elif got != w:
            print(f"SQLFAIL {name}:\n  live={w!r}\n  port={got!r}")
            bad += 1
        else:
            print(f"SQLOK   {name}")
    extra = set(macros) - set(want)
    if extra:
        print("SQLWARN unverified macros:", sorted(extra))
    print(f"CHECK-SQL kanban_adapter_flow: {len(want)} fragments, {bad} mismatches")
    return 0 if bad == 0 else 1


def main():
    if "--check-sql" in sys.argv or "--sql-only" in sys.argv:
        rc = check_sql()
        if "--sql-only" in sys.argv:
            return rc
    # per-func coverage floor
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        print("FATAL: funcs with <5 cases:", thin)
        return 2
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"GEN kanban_adapter_flow: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())