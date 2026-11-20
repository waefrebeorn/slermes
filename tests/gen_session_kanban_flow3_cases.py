#!/usr/bin/env python3
"""Generate tests/fixtures/session_kanban_flow3.jsonl — deterministic cases for
the session_kanban remainder FLOW port (hermes_cli/kanban_db.py +
kanban_db_dispatch.py + process_identity.py + session_keys.py). >=5 cases per
function, covering decision branches, seam call order, exception paths and
fail-closed script gaps.

Each case: {"func", "in": {"args", "state", "io"}, "fail_closed"?: true}.
io maps seam-name -> result (plain value | call-order list | __script__ raise).
ASCII fixture data; source literals live in the port/oracle.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_kanban_flow3.jsonl")
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


def raise_spec(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


# ── _normalize_board_slug ──
add("_normalize_board_slug", args={"slug": "  My-Board_2  "})
add("_normalize_board_slug", args={"slug": ""})
add("_normalize_board_slug", args={"slug": "   "})
add("_normalize_board_slug", args={"slug": None})
add("_normalize_board_slug", args={"slug": "-bad"})
add("_normalize_board_slug", args={"slug": "_bad"})
add("_normalize_board_slug", args={"slug": "a" * 65})
add("_normalize_board_slug", args={"slug": "a" * 64})
add("_normalize_board_slug", args={"slug": "bad slug!"})
add("_normalize_board_slug", args={"slug": 5})
add("_normalize_board_slug", args={"slug": True})
add("_normalize_board_slug", args={"slug": "a\x1cb"})
add("_normalize_board_slug", args={})   # missing slug -> TypeError

# ── _require_slug ──
add("_require_slug", args={"slug": "x"}, io={"_normalize_board_slug": "x"})
add("_require_slug", args={"slug": "x"}, io={"_normalize_board_slug": None})
add("_require_slug", args={"slug": "x"}, io={"_normalize_board_slug": ""})
add("_require_slug", args={"slug": "x"}, io={"_normalize_board_slug": raise_spec("ValueError", "bad")})
add("_require_slug", args={"slug": "x"}, fail_closed=True)
add("_require_slug", args={"slug": "x"}, io={"_normalize_board_slug": "ok"})

# ── _board_path ──
KB_PARTS = ["kanban.db"]
WS_PARTS = ["kanban", "workspaces"]
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {"HERMES_KANBAN_DB": "/pinned.db"}},
    io={"_explicit_board_slug": None, "path.expanduser": "/pinned.db"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": "b", "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {"HERMES_KANBAN_DB": "/pinned.db"}},
    io={"_explicit_board_slug": "b", "_explicit_board_intent_pinned": True, "path.expanduser": "/pinned.db"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": "b", "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {"HERMES_KANBAN_DB": "/pinned.db"}},
    io={"_explicit_board_slug": "b", "_explicit_board_intent_pinned": False, "board_dir": "/boards/b"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {}},
    io={"_explicit_board_slug": None, "get_current_board": "default", "kanban_home": "/root"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {}},
    io={"_explicit_board_slug": None, "get_current_board": "myboard", "board_dir": "/boards/myboard"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": "default", "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {}},
    io={"_explicit_board_slug": "default", "kanban_home": "/root"})
add("_board_path", args={"env_var": None, "board": None, "default_parts": WS_PARTS, "leaf": "workspaces"},
    state={"environ": {}},
    io={"_explicit_board_slug": None, "get_current_board": "default", "kanban_home": "/root"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {"HERMES_KANBAN_DB": "  /p.db  "}},
    io={"_explicit_board_slug": None, "path.expanduser": "/p.db"})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    state={"environ": {}}, io={"_explicit_board_slug": raise_spec("ValueError", "x")})
add("_board_path", args={"env_var": "HERMES_KANBAN_DB", "board": None, "default_parts": KB_PARTS, "leaf": "kanban.db"},
    fail_closed=True)

# ── kanban_db_path ──
add("kanban_db_path", args={"board": None}, io={"_board_path": "/x/kanban.db"})
add("kanban_db_path", args={"board": "b"}, io={"_board_path": "/boards/b/kanban.db"})
add("kanban_db_path", args={}, io={"_board_path": "/x/kanban.db"})
add("kanban_db_path", args={"board": "b"}, io={"_board_path": raise_spec("OSError", "y")})
add("kanban_db_path", args={"board": None}, fail_closed=True)
add("kanban_db_path", args={"board": ""}, io={"_board_path": "/x/kanban.db"})

# ── workspaces_root ──
add("workspaces_root", args={"board": None}, io={"_board_path": "/root/kanban/workspaces"})
add("workspaces_root", args={"board": "b"}, io={"_board_path": "/boards/b/workspaces"})
add("workspaces_root", args={}, io={"_board_path": "/root/kanban/workspaces"})
add("workspaces_root", args={"board": "b"}, io={"_board_path": raise_spec("OSError", "y")})
add("workspaces_root", args={"board": None}, fail_closed=True)

# ── get_task ──
add("get_task", args={"task_id": "t1"},
    io={"conn.execute": {"id": "t1", "status": "running"}, "task.from_row": {"id": "t1", "status": "running"}})
add("get_task", args={"task_id": "t1"}, io={"conn.execute": None})
add("get_task", args={"task_id": "t1"}, io={"conn.execute": {}})
add("get_task", args={"task_id": "t1"}, io={"conn.execute": raise_spec("sqlite3.Error", "db")})
add("get_task", args={"task_id": "t1"},
    io={"conn.execute": {"id": "t1"}, "task.from_row": raise_spec("ValueError", "row")})
add("get_task", args={"task_id": "t1"}, fail_closed=True)
add("get_task", args={})   # missing task_id -> TypeError

# ── _json_or ──
add("_json_or", args={"value": '{"a": 1}'})
add("_json_or", args={"value": "null"})
add("_json_or", args={"value": ""})
add("_json_or", args={"value": None})
add("_json_or", args={"value": "01"})
add("_json_or", args={"value": "abc"})
add("_json_or", args={"value": "[1,2]"})
add("_json_or", args={"value": "  {}  "})
add("_json_or", args={"value": 5})
add("_json_or", args={"value": [1]})
add("_json_or", args={"value": '{"a":1}', "default": {"d": 1}})
add("_json_or", args={"value": "bad", "default": {"d": 1}})
add("_json_or", args={"value": '"\\u0041"'})
add("_json_or", args={"value": "1.0"})
add("_json_or", args={"value": "[1,]"})
add("_json_or", args={})   # missing value -> TypeError

# ── _json_or_null ──
add("_json_or_null", args={"obj": {"a": 1}})
add("_json_or_null", args={"obj": {}})
add("_json_or_null", args={"obj": []})
add("_json_or_null", args={"obj": None})
add("_json_or_null", args={"obj": 0})
add("_json_or_null", args={"obj": [1, True, None, "x"]})
add("_json_or_null", args={"obj": {"k": "a\nb"}})
add("_json_or_null", args={"obj": "hi"})
add("_json_or_null", args={"obj": 5})
add("_json_or_null", args={"obj": False})

# ── _append_event ──
add("_append_event", args={"task_id": "t1", "kind": "worker_bound", "payload": {"pid": 5}, "run_id": 3},
    io={"_json_or_null": '{"pid": 5}', "time.time": 1000.9, "conn.execute": None})
add("_append_event", args={"task_id": "t1", "kind": "worker_bound", "payload": None, "run_id": 3},
    io={"_json_or_null": None, "time.time": 1000, "conn.execute": None})
add("_append_event", args={"task_id": "t1", "kind": "claimed"},
    io={"_json_or_null": None, "time.time": 5, "conn.execute": None})
add("_append_event", args={"task_id": "t1", "kind": "k", "payload": {"a": 1}},
    io={"_json_or_null": '{"a": 1}', "time.time": raise_spec("OSError", "clock")})
add("_append_event", args={"task_id": "t1", "kind": "k", "payload": {"a": 1}},
    io={"_json_or_null": raise_spec("ValueError", "p")})
add("_append_event", args={"task_id": "t1", "kind": "k"},
    io={"_json_or_null": None, "time.time": 1, "conn.execute": raise_spec("sqlite3.Error", "w")})
add("_append_event", args={"task_id": "t1", "kind": "k"}, fail_closed=True)
add("_append_event", args={"kind": "k"})   # missing task_id -> TypeError

# ── _current_run_id ──
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"current_run_id": 5}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"current_run_id": "7"}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"current_run_id": None}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"current_run_id": 0}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": None})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"current_run_id": "abc"}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": {"other": 1}})
add("_current_run_id", args={"task_id": "t1"}, io={"conn.execute": raise_spec("sqlite3.Error", "r")})

# ── _latest_event ──
add("_latest_event", args={"task_id": "t1", "kind": "claimed"}, io={"conn.execute": {"payload": "x"}})
add("_latest_event", args={"task_id": "t1", "kind": "claimed", "run_id": 3}, io={"conn.execute": {"payload": "x"}})
add("_latest_event", args={"task_id": "t1", "kind": "claimed", "run_id": "4"}, io={"conn.execute": {"payload": "x"}})
add("_latest_event", args={"task_id": "t1", "kind": "claimed", "run_id": "abc"})
add("_latest_event", args={"task_id": "t1", "kind": "claimed"}, io={"conn.execute": None})
add("_latest_event", args={"task_id": "t1", "kind": "claimed"}, io={"conn.execute": raise_spec("sqlite3.Error", "e")})
add("_latest_event", args={"task_id": "t1"})   # missing kind -> TypeError

# ── _json_dict ──
add("_json_dict", args={"value": '{"a":1}'}, io={"_json_or": {"a": 1}})
add("_json_dict", args={"value": "x"}, io={"_json_or": {}})
add("_json_dict", args={"value": "x"}, io={"_json_or": [1]})
add("_json_dict", args={"value": "x"}, io={"_json_or": None})
add("_json_dict", args={"value": "x"}, io={"_json_or": raise_spec("ValueError", "j")})
add("_json_dict", args={"value": "x"}, fail_closed=True)

# ── _row_get ──
add("_row_get", args={"row": {"a": 1}, "col": "a"})
add("_row_get", args={"row": {"a": 1}, "col": "b"})
add("_row_get", args={"row": {"a": 1}, "col": "b", "default": 5})
add("_row_get", args={"row": None, "col": "a"})
add("_row_get", args={"row": None, "col": "a", "default": 7})
add("_row_get", args={"row": 5, "col": "a"})
add("_row_get", args={"row": "x", "col": "a"})
add("_row_get", args={"row": {"a": None}, "col": "a"})

# ── _retry_status_for_run ──
add("_retry_status_for_run", args={"task_id": "t1"},
    io={"_current_run_id": 3, "_latest_event": {"payload": "p"},
        "_row_get": '{"source_status":"review"}', "_json_dict": {"source_status": "review"}})
add("_retry_status_for_run", args={"task_id": "t1", "run_id": 3},
    io={"_latest_event": {"payload": "p"}, "_row_get": '{"source_status":"ready"}',
        "_json_dict": {"source_status": "ready"}})
add("_retry_status_for_run", args={"task_id": "t1"}, io={"_current_run_id": None})
add("_retry_status_for_run", args={"task_id": "t1", "run_id": 3},
    io={"_latest_event": {"payload": "p"}, "_row_get": "{}", "_json_dict": {}})
add("_retry_status_for_run", args={"task_id": "t1", "run_id": 3},
    io={"_latest_event": raise_spec("sqlite3.Error", "e")})
add("_retry_status_for_run", args={"task_id": "t1"}, io={"_current_run_id": raise_spec("OSError", "c")})
add("_retry_status_for_run", args={"task_id": "t1"}, fail_closed=True)

# ── goal_run_status ──
add("goal_run_status", args={"task_id": "t1"}, io={"get_task": {"status": "running", "current_run_id": 3}})
add("goal_run_status", args={"task_id": "t1"}, io={"get_task": None})
add("goal_run_status", args={"task_id": "t1", "expected_run_id": 3},
    io={"get_task": {"status": "running", "current_run_id": 3}, "conn.execute": {"outcome": "completed"}})
add("goal_run_status", args={"task_id": "t1", "expected_run_id": 3},
    io={"get_task": {"status": "running", "current_run_id": 3}, "conn.execute": {"outcome": None}})
add("goal_run_status", args={"task_id": "t1", "expected_run_id": 3},
    io={"get_task": {"status": "running", "current_run_id": 4}, "conn.execute": {"outcome": None}})
add("goal_run_status", args={"task_id": "t1", "expected_run_id": 3},
    io={"get_task": {"status": "running", "current_run_id": 3}, "conn.execute": {"outcome": "weird"}})
add("goal_run_status", args={"task_id": "t1", "expected_run_id": 3},
    io={"get_task": {"status": "running", "current_run_id": 3}, "conn.execute": None})
add("goal_run_status", args={"task_id": "t1"},
    io={"get_task": {"status": "ready", "current_run_id": 3}, "conn.execute": {"kind": "changes_requested"}})
add("goal_run_status", args={"task_id": "t1"},
    io={"get_task": {"status": "ready", "current_run_id": 3}, "conn.execute": {"kind": "other"}})
add("goal_run_status", args={"task_id": "t1"},
    io={"get_task": {"status": "ready", "current_run_id": 3}, "conn.execute": None})
add("goal_run_status", args={"task_id": "t1"}, io={"get_task": {"current_run_id": 3}})
add("goal_run_status", args={"task_id": "t1"}, io={"get_task": raise_spec("sqlite3.Error", "g")})

# ── _worker_terminal_timeout_env ──
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": None, "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 0, "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": -5, "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": "abc", "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": "100", "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": "80"})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": "50"})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": " 60 "})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": "bad"})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": ""})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 100, "current_timeout": True})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 31, "current_timeout": None})
add("_worker_terminal_timeout_env", args={"max_runtime_seconds": 10, "current_timeout": None})

# ── _process_fingerprint ──
add("_process_fingerprint", args={"pid": 5}, io={"get_process_start_time": 100, "current_instantiation_epoch": "epoch1"})
add("_process_fingerprint", args={"pid": 5}, io={"get_process_start_time": None})
add("_process_fingerprint", args={"pid": "5"}, io={"get_process_start_time": 100, "current_instantiation_epoch": "e"})
add("_process_fingerprint", args={"pid": "abc"})
add("_process_fingerprint", args={"pid": 5}, io={"get_process_start_time": raise_spec("OSError", "s")})
add("_process_fingerprint", args={"pid": 5},
    io={"get_process_start_time": 100, "current_instantiation_epoch": raise_spec("RuntimeError", "e")})
add("_process_fingerprint", args={"pid": 5}, fail_closed=True)
add("_process_fingerprint", args={"pid": 5}, io={"get_process_start_time": 100, "current_instantiation_epoch": 7})

# ── _pid_recycled ──
add("_pid_recycled", args={"pid": 5, "started_at": None})
add("_pid_recycled", args={"pid": 0, "started_at": "x"})
add("_pid_recycled", args={"pid": 5, "started_at": "unverified"})
add("_pid_recycled", args={"pid": 5, "started_at": "epoch|100"}, io={"_process_fingerprint": "epoch|100"})
add("_pid_recycled", args={"pid": 5, "started_at": "epoch|100"}, io={"_process_fingerprint": "other|99"})
add("_pid_recycled", args={"pid": 5, "started_at": "epoch|100"}, io={"_process_fingerprint": None})
add("_pid_recycled", args={"pid": 5, "started_at": 100},
    io={"get_process_start_time": 100, "_start_times_agree": True})
add("_pid_recycled", args={"pid": 5, "started_at": 100}, io={"get_process_start_time": None})
add("_pid_recycled", args={"pid": 5, "started_at": 100},
    io={"get_process_start_time": 100, "_start_times_agree": False})
add("_pid_recycled", args={"pid": 5, "started_at": 100},
    io={"get_process_start_time": 100, "_start_times_agree": raise_spec("TypeError", "t")})
add("_pid_recycled", args={"pid": 5, "started_at": 100},
    io={"get_process_start_time": 100, "_start_times_agree": raise_spec("ValueError", "v")})
add("_pid_recycled", args={"pid": 5, "started_at": 100},
    io={"get_process_start_time": 100, "_start_times_agree": raise_spec("OSError", "o")})
add("_pid_recycled", args={"pid": "abc", "started_at": "e|1"})

# ── _worker_alive ──
add("_worker_alive", args={"pid": 5, "started_at": None}, io={"_pid_alive": False})
add("_worker_alive", args={"pid": 5, "started_at": "unverified"}, io={"_pid_alive": True})
add("_worker_alive", args={"pid": 5, "started_at": "e|1"}, io={"_pid_alive": True, "_pid_recycled": False})
add("_worker_alive", args={"pid": 5, "started_at": "e|1"}, io={"_pid_alive": True, "_pid_recycled": True})
add("_worker_alive", args={"pid": 5, "started_at": None}, io={"_pid_alive": raise_spec("OSError", "a")})
add("_worker_alive", args={"pid": 5, "started_at": None}, fail_closed=True)

# ── process_birth ──
add("process_birth", args={"proc": {}}, io={"proc.create_time": 123.5})
add("process_birth", args={"proc": {}}, state={"psutil": {"MACOS": True}}, io={"proc.create_time": 100})
add("process_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"MACOS": True}},
    io={"proc._proc.create_time": 200})
add("process_birth", args={"proc": {"_proc": None}}, state={"psutil": {"MACOS": True}},
    io={"proc.create_time": 100})
add("process_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"MACOS": True}},
    io={"proc._proc.create_time": raise_spec("TypeError", "m"), "proc.create_time": 100})
add("process_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"MACOS": True}},
    io={"proc._proc.create_time": None, "proc.create_time": 100})
add("process_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"MACOS": True}},
    io={"proc._proc.create_time": raise_spec("ValueError", "m")})
add("process_birth", args={"proc": {}}, io={"proc.create_time": "123.5"})
add("process_birth", args={"proc": {}}, io={"proc.create_time": "abc"})
add("process_birth", args={"proc": {}}, io={"proc.create_time": raise_spec("OSError", "c")})
add("process_birth", args={})   # missing proc -> TypeError

# ── worker_birth ──
add("worker_birth", args={"proc": {}}, state={"psutil": {"LINUX": True}})
add("worker_birth", args={"proc": {"_proc": None}}, state={"psutil": {"LINUX": True}})
add("worker_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"LINUX": True}},
    io={"proc._proc.create_time": 300})
add("worker_birth", args={"proc": {}}, io={"process_birth": 100})
add("worker_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"LINUX": True}},
    io={"proc._proc.create_time": raise_spec("OSError", "b")})
add("worker_birth", args={"proc": {}}, fail_closed=True)
add("worker_birth", args={"proc": {"_proc": {}}}, state={"psutil": {"LINUX": True}},
    io={"proc._proc.create_time": "abc"})

# ── profile_from_session_key ──
add("profile_from_session_key", args={"session_key": "agent:beta:x"},
    io={"profile_from_session_key_namespace": "beta"})
add("profile_from_session_key", args={"session_key": "agent::x"},
    io={"profile_from_session_key_namespace": "default"})
add("profile_from_session_key", args={"session_key": "agent:main~:x"},
    io={"profile_from_session_key_namespace": "main"})
add("profile_from_session_key", args={"session_key": ""})
add("profile_from_session_key", args={"session_key": None})
add("profile_from_session_key", args={"session_key": "other:beta:x"})
add("profile_from_session_key", args={"session_key": "agent"})
add("profile_from_session_key", args={"session_key": 5})
add("profile_from_session_key", args={"session_key": "agent:b:x"},
    io={"profile_from_session_key_namespace": raise_spec("ValueError", "n")})
add("profile_from_session_key", args={"session_key": "agent:b:x"}, fail_closed=True)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    thin = [f for f, n in counts.items() if n < 5]
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if thin:
        print(f"THIN (<5): {thin}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
