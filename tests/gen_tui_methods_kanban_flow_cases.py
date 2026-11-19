#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_kanban_flow.jsonl for the TUI kanban
method FLOW port (src/gateway/tui_methods_kanban_flow.c).

The task named tui_gateway/methods_kanban.py (nonexistent); the real defining
modules are gateway/session_kanban.py and gateway/session_controls.py
(kanban_run). Each case: {"func": <live def name>, "in": {"args": {...},
"io": {...}, ["state": {...}]}, ["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (use it for list-valued
results); {"__script__": true, "raise": {"type", "message"}} raises.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; POSIX paths; integral numbers.
Getter scripts (kb.get_task) stay in {null, non-empty record} — a live Task
is always truthy. turns_done.wait sequences always terminate (a constant
false would spin the watch loop forever). Payload/kanban_json strings are
valid JSON.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_kanban_flow.jsonl"

CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False):
    inp = {"args": args or {}, "io": io or {}}
    if state is not None:
        inp["state"] = state
    c = {"func": func, "in": inp}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_script(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def jdump(v):
    return json.dumps(v, separators=(",", ":"))


# ── shared fixture records ──
CONN = {
    "native_owner": True,
    "actor": {"capabilities": ["session:create"], "subject": "sub1",
              "profile_id": "/h/p1"},
    "authority": {"profile_id": "/h/p1"},
}
PARAMS = {"board": "b1", "task_id": "t1", "run_id": 5, "claim_lock": "L1"}
TASK = {
    "id": "t1", "status": "running", "current_run_id": 5, "claim_lock": "L1",
    "assignee": "/h/p1", "skills": ["s1"], "workspace_path": "/ws/t1",
    "model_override": None, "provider_override": None, "reasoning_effort": None,
    "branch_name": "br1", "tenant": "ten1", "goal_mode": False,
    "goal_max_turns": None, "title": "Title", "body": "Body",
    "max_runtime_seconds": 600,
}
RUN_ROW = {"status": "running", "task_id": "t1", "claim_lock": "L1"}
REQ_ID = "kanban:" + jdump(["b1", "t1", 5])
BOUND_ROW = [jdump({"request_id": REQ_ID})]
POLICY = {
    "model": "m1", "toolsets": ["core"], "request_json": jdump({"a": 1}),
    "source": "cli", "platform": "cli", "kanban_json": "{}",
}
KCTX = {
    "board": "b1", "task_id": "t1", "run_id": 5, "claim_lock": "L1",
    "db": "/boards/b1.db", "workspace": "/ws/t1", "workspaces_root": "/ws",
    "profile": "/h/p1", "branch": "br1", "tenant": "ten1", "goal_mode": False,
    "goal_max_turns": None, "max_runtime_seconds": 600, "skills": ["s1"],
    "context": "worker-ctx",
}
POLICY_K = dict(POLICY, kanban_json=jdump(KCTX))
STORE = {"scope": {"profile_dir": "/h/p1"}, "sessions": {}, "bindings": {},
         "admissions": {}}
REF = {"session_id": "s1"}
RECEIPT = {"admission_id": "a1", "sequence": 1, "status": "queued"}
RESULT = {"final_response": "done", "messages": [{"role": "assistant"}]}
FRAME_K = {"policy": {"kanban_json": jdump(KCTX), "source": "kanban"}}


def bkp_io(**over):
    """Happy-path build_kanban_policy scripts (no db param, no overrides)."""
    io = {
        "kb._require_slug": "b1",
        "kb.kanban_db_path": "/boards/b1.db",
        "path.resolve": ["/boards/b1.db", "/h/p1", "/h/p1", "/ws"],
        "path.as_uri": "file:///boards/b1.db",
        "sqlite3.connect": None,
        "kb.get_task": TASK,
        "conn.execute": [RUN_ROW, BOUND_ROW],
        "resolve_profile_env": "/h/p1",
        "kb.build_worker_context": "worker-ctx",
        "kb._retry_status_for_run": "none",
        "conn.close": None,
        "build_policy": POLICY,
        "kb.workspaces_root": "/ws",
    }
    io.update(over)
    return io


# ── build_kanban_policy ──
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     bkp_io())
case("build_kanban_policy",
     {"connection": CONN, "params": dict(PARAMS, db="/custom/b1.db"), "config": {"k": 1}},
     bkp_io(**{
         "path.resolve": ["/custom/b1.db", "/h/p1", "/h/p1", "/ws"],
         "kb.get_task": dict(TASK, model_override="m2", provider_override="p2",
                             reasoning_effort="high", skills=["s1", "s2"]),
         "kb._retry_status_for_run": "review",
         "build_policy": dict(POLICY, model=None),
         "policy.config": {"cfg": 1},
         "resolve_gateway_model": "gm1",
     }))
case("build_kanban_policy",
     {"connection": dict(CONN, native_owner=False), "params": PARAMS, "config": {}})
case("build_kanban_policy",
     {"connection": dict(CONN, actor=dict(CONN["actor"], profile_id="/h/other")),
      "params": PARAMS, "config": {}})
case("build_kanban_policy",
     {"connection": CONN, "params": dict(PARAMS, run_id="5"), "config": {}})
case("build_kanban_policy",
     {"connection": CONN, "params": dict(PARAMS, board=""), "config": {}})
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     bkp_io(**{"kb.get_task": dict(TASK, status="todo")}))
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     bkp_io(**{"kb._require_slug": raise_script("ValueError", "bad slug")}))
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     bkp_io(**{"kb._require_slug": raise_script("SystemExit", "9")}))
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     bkp_io(**{"conn.execute": [RUN_ROW, [jdump({"other": 1})]]}))
case("build_kanban_policy", {"connection": CONN, "params": PARAMS, "config": {}},
     {"kb._require_slug": "b1"}, fail_closed=True)

# ── run_task ──
def rt_io_new(**over):
    io = {
        "authority_persistence": STORE,
        "store_location": None,
        "asyncio.to_thread": "/h/p1",
        "local_session_id": "s1",
        "load_gateway_config": {"cfg": 1},
        "build_kanban_policy": script([POLICY_K, {}]),
        "create_local_session": REF,
        "tracked_write": None,
        "kbc.connect_closing": None,
        "kb.write_txn": None,
        "conn.execute": script([jdump({"session_id": "s1"})]),
        "conn.close": None,
        "connection.resume": None,
        "authority.submit": RECEIPT,
    }
    io.update(over)
    return io


case("run_task", {"connection": CONN, "params": PARAMS}, rt_io_new())
case("run_task", {"connection": CONN, "params": PARAMS},
     rt_io_new(**{
         "authority_persistence": dict(STORE, sessions={"s1": {}}),
         "store.bindings.receipt": {"policy": {"p": 1}},
         "restore_policy": POLICY_K,
         "restore_local_session": REF,
         "asyncio.to_thread": ["/h/p1", {"payload": {"finite": True,
                                                      "kanban_card_v1": True}}],
         "conn.execute": script(None),
         "kb.get_task": TASK,
         "os.getpid": 9,
         "psutil.Process": None,
         "worker_birth": "birth1",
         "kb._append_event": None,
     }))
case("run_task", {"connection": dict(CONN, native_owner=False), "params": PARAMS})
case("run_task", {"connection": CONN, "params": dict(PARAMS, claim_lock="OTHER")},
     rt_io_new(**{
         "authority_persistence": dict(STORE, sessions={"s1": {}}),
         "store.bindings.receipt": {"policy": {"p": 1}},
         "restore_policy": POLICY_K,
     }))
case("run_task", {"connection": CONN, "params": PARAMS},
     rt_io_new(**{"conn.execute": script([jdump({"session_id": "other"})])}))
case("run_task", {"connection": CONN, "params": PARAMS},
     rt_io_new(**{"tracked_write": raise_script("KeyboardInterrupt", "")}))
case("run_task", {"connection": CONN, "params": PARAMS},
     rt_io_new(**{
         "authority_persistence": dict(STORE, sessions={"s1": {}}),
         "store.bindings.receipt": {"policy": {"p": 1}},
         "restore_policy": POLICY_K,
         "restore_local_session": REF,
         "asyncio.to_thread": ["/h/p1", {"payload": {"other": True}}],
     }))
case("run_task", {"connection": CONN, "params": PARAMS}, {}, fail_closed=True)

# ── admit_claim (closure) ──
AC_ARGS = {"context": {"db": "/boards/b1.db"}, "params": PARAMS, "ref": REF,
           "owner_db": "/h/p1", "request_id": REQ_ID}
case("admit_claim", AC_ARGS,
     {"kbc.connect_closing": None, "kb.write_txn": None,
      "conn.execute": script([jdump({"session_id": "s1"})]), "conn.close": None})
case("admit_claim", AC_ARGS,
     {"kbc.connect_closing": None, "kb.write_txn": None,
      "conn.execute": script([jdump({"session_id": "other"})]), "conn.close": None})
case("admit_claim", AC_ARGS,
     {"kbc.connect_closing": None, "kb.write_txn": None,
      "conn.execute": script(None), "kb.get_task": TASK,
      "os.getpid": 9, "psutil.Process": None, "worker_birth": "birth1",
      "kb._append_event": None, "conn.close": None})
case("admit_claim", AC_ARGS,
     {"kbc.connect_closing": None, "kb.write_txn": None,
      "conn.execute": script(None), "kb.get_task": dict(TASK, claim_lock="X"),
      "conn.close": None})
case("admit_claim", AC_ARGS,
     {"kbc.connect_closing": None, "kb.write_txn": None,
      "conn.execute": raise_script("SystemExit", "3"), "conn.close": None})
case("admit_claim", AC_ARGS, {}, fail_closed=True)

# ── bind_worker_context ──
def bwc_io(**over):
    io = {
        "kbd._worker_terminal_timeout_env": [None, None],
        "kbc.connect_closing": None,
        "kb.write_txn": None,
        "kb.get_task": TASK,
        "os.getpid": [9, 9],
        "kbd._process_fingerprint": "fp1",
        "kb._append_event": None,
        "conn.execute": script([jdump({"expires": 100}), 50]),
        "conn.close": None,
        "watch_submitter": None,
        "os.chdir": None,
    }
    io.update(over)
    return io


case("bind_worker_context", {"frame": FRAME_K}, bwc_io(), state={"environ": {}})
case("bind_worker_context",
     {"frame": {"policy": {"kanban_json": "null", "source": "kanban"}}})
case("bind_worker_context",
     {"frame": {"policy": {"kanban_json": jdump(KCTX), "source": "cli"}}})
case("bind_worker_context", {"frame": FRAME_K},
     bwc_io(**{"kb.get_task": dict(TASK, status="done")}), state={"environ": {}})
case("bind_worker_context", {"frame": FRAME_K},
     bwc_io(**{"conn.execute": script([jdump({"expires": "soon"}), 50])}),
     state={"environ": {}})
case("bind_worker_context", {"frame": FRAME_K},
     bwc_io(**{"kbd._worker_terminal_timeout_env": ["30", "60"]}),
     state={"environ": {"TERMINAL_TIMEOUT": "old"}})
case("bind_worker_context",
     {"frame": {"policy": {"kanban_json": jdump(dict(KCTX, branch="", tenant="",
                                                       goal_mode=True)),
                           "source": "kanban"}}},
     bwc_io(), state={"environ": {}})
case("bind_worker_context", {"frame": FRAME_K},
     bwc_io(**{"kb.get_task": raise_script("SystemExit", "1")}),
     state={"environ": {}})
case("bind_worker_context", {"frame": FRAME_K}, {}, fail_closed=True)

# ── _watch_submitter ──
case("_watch_submitter", {"db": "/boards/b1.db", "run_id": 5},
     {"threading.Thread": None, "thread.start": None})
case("_watch_submitter", {"db": "/boards/b2.db", "run_id": 6},
     {"threading.Thread": None, "thread.start": None})
case("_watch_submitter", {"db": "/boards/b1.db", "run_id": 5},
     {"threading.Thread": None, "thread.start": raise_script("RuntimeError", "no thread")})
case("_watch_submitter", {"db": "/boards/b1.db", "run_id": 5},
     {"threading.Thread": None, "thread.start": raise_script("SystemExit", "1")})
case("_watch_submitter", {"db": "/boards/b1.db", "run_id": 5}, {}, fail_closed=True)

# ── watch (closure) ──
def watch_io(**over):
    io = {
        "path.resolve": "/boards/b1.db",
        "path.as_uri": "file:///boards/b1.db",
        "sqlite3.connect": None,
        "turns_done.wait": [False, True],
        "conn.execute": script([7, 100]),
        "os.getpid": 9,
        "kbd._worker_alive": False,
        "os._exit": None,
        "conn.close": None,
    }
    io.update(over)
    return io


case("watch", {"db": "/boards/b1.db", "run_id": 5}, watch_io())
case("watch", {"db": "/boards/b1.db", "run_id": 5},
     watch_io(**{"turns_done.wait": True}))
case("watch", {"db": "/boards/b1.db", "run_id": 5},
     watch_io(**{"os.getpid": 7}))
case("watch", {"db": "/boards/b1.db", "run_id": 5},
     watch_io(**{"conn.execute": raise_script("sqlite3.Error", "db gone")}))
case("watch", {"db": "/boards/b1.db", "run_id": 5},
     watch_io(**{"conn.execute": script(None)}))
case("watch", {"db": "/boards/b1.db", "run_id": 5},
     watch_io(**{"kbd._worker_alive": True}))
case("watch", {"db": "/boards/b1.db", "run_id": 5}, {}, fail_closed=True)

# ── run_worker_turns ──
RWT_ARGS = {"agent": {"session_id": "s1"}, "frame": {"text": "hi"}, "history": []}
case("run_worker_turns", RWT_ARGS,
     {"worker_turn_scope": None, "arm_surface_turn": None,
      "run_worker_turns_inner": RESULT})
case("run_worker_turns", RWT_ARGS,
     {"worker_turn_scope": None, "arm_surface_turn": None,
      "run_worker_turns_inner": {"final_response": "x"}})
case("run_worker_turns", RWT_ARGS,
     {"worker_turn_scope": None,
      "arm_surface_turn": raise_script("RuntimeError", "arm failed")})
case("run_worker_turns", RWT_ARGS,
     {"worker_turn_scope": None, "arm_surface_turn": None,
      "run_worker_turns_inner": raise_script("SystemExit", "2")})
case("run_worker_turns", RWT_ARGS, {}, fail_closed=True)

# ── _worker_workspace ──
WW_FRAME = {"scope": {"session_id": "s1"}, "policy": {"p": 1},
            "route": "agent:beta:s1", "home": "/h/beta"}
def ww_io(**over):
    io = {
        "restore_policy": POLICY,
        "served_profile_name": "beta",
        "profile_from_session_key": "beta",
        "set_session_vars": "tok1",
        "bind_turn_workspace": None,
        "clear_session_vars": None,
    }
    io.update(over)
    return io


case("_worker_workspace", {"frame": WW_FRAME}, ww_io())
case("_worker_workspace", {"frame": {"scope": {}}})
case("_worker_workspace", {"frame": dict(WW_FRAME, route=None)},
     ww_io(**{"profile_from_session_key": "main"}))
case("_worker_workspace", {"frame": WW_FRAME},
     ww_io(**{"bind_turn_workspace": raise_script("RuntimeError", "bind failed")}))
case("_worker_workspace", {"frame": WW_FRAME},
     ww_io(**{"served_profile_name": None, "profile_from_session_key": ""}))
case("_worker_workspace", {"frame": WW_FRAME}, {}, fail_closed=True)

# ── _run_worker_turns ──
FRAME_NULL = {"policy": {"kanban_json": None}, "text": "hello",
              "turn_author": {"id": "op"}}
FRAME_CTX = {"policy": {"kanban_json": jdump({"task_id": "t1"})}, "text": "hello"}
def rwt_inner_io(**over):
    io = {
        "run_task_turns": RESULT,
        "turn_exit_code": 0,
        "kb.redact_review_value": "redacted",
        "record_worker_result": None,
    }
    io.update(over)
    return io


case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_NULL, "history": []},
     {"worker_workspace": "s1", "worker_display_kwargs": {"display": True},
      "agent.run_conversation": RESULT})
case("_run_worker_turns",
     {"agent": {"session_id": "s1"},
      "frame": {"policy": {"kanban_json": "null"}, "text": "hello"},
      "history": []},
     {"worker_workspace": None, "worker_display_kwargs": {},
      "agent.run_conversation": RESULT})
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     rwt_inner_io())
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     rwt_inner_io(**{
         "run_task_turns": raise_script("RuntimeError", "turn boom"),
         "credential_failure_flags": {"credentials_terminal": True},
         "turn_exit_code": 78,
     }))
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     rwt_inner_io(**{"run_task_turns": raise_script("SystemExit", "4")}))
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     rwt_inner_io(**{"run_task_turns": "plain-string", "kb.redact_review_value": "r"}))
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     rwt_inner_io(**{"kb.redact_review_value": raise_script("ValueError", "redact"),
                     "credential_failure_flags": {}}))
case("_run_worker_turns",
     {"agent": {"session_id": "s1"}, "frame": FRAME_CTX, "history": []},
     {}, fail_closed=True)

# ── record_start_failure ──
case("record_start_failure",
     {"frame": FRAME_K, "exc": {"type": "RuntimeError", "message": "boom"}},
     {"credential_failure_flags": {"credentials_terminal": True},
      "turn_exit_code": 78, "record_worker_result": None})
case("record_start_failure",
     {"frame": {"policy": {"kanban_json": "null"}},
      "exc": {"type": "RuntimeError", "message": "boom"}})
case("record_start_failure",
     {"frame": FRAME_K, "exc": {"type": "ValueError", "message": "x"}},
     {"credential_failure_flags": {}, "turn_exit_code": 1,
      "record_worker_result": None})
case("record_start_failure",
     {"frame": FRAME_K, "exc": {"type": "RuntimeError", "message": "boom"}},
     {"credential_failure_flags": raise_script("SystemExit", "5"),
      "turn_exit_code": 1, "record_worker_result": None})
case("record_start_failure",
     {"frame": FRAME_K, "exc": {"type": "RuntimeError", "message": "boom"}},
     {}, fail_closed=True)

# ── _record_worker_result ──
RWR_ARGS = {"context": KCTX, "code": 0, "last_output": "tail"}
def rwr_io(**over):
    io = {
        "turns_done.set": None,
        "kbc.connect_closing": None,
        "kb.write_txn": None,
        "bound_worker_matches": True,
        "os.getpid": 9,
        "kb._append_event": None,
        "conn.close": None,
    }
    io.update(over)
    return io


case("_record_worker_result", RWR_ARGS, rwr_io(), state={"_TURNS_DONE": False})
case("_record_worker_result", RWR_ARGS, rwr_io(**{"bound_worker_matches": False}))
case("_record_worker_result", RWR_ARGS,
     rwr_io(**{"bound_worker_matches": raise_script("SystemExit", "6")}))
case("_record_worker_result", RWR_ARGS,
     rwr_io(**{"kb._append_event": raise_script("RuntimeError", "append failed")}))
case("_record_worker_result", RWR_ARGS, {}, fail_closed=True)
case("_record_worker_result", {"context": KCTX, "code": 75, "last_output": ""},
     rwr_io(), state={"_TURNS_DONE": False})

# ── record_worker_loss ──
RWL_ARGS = {"policy": {"kanban_json": jdump(KCTX)}, "reason": "lost",
            "pid": 9, "bound": True}
def rwl_io(**over):
    io = {
        "kbc.connect_closing": None,
        "kb.write_txn": None,
        "conn.execute": script(["running", 5]),
        "kb._append_event": None,
        "conn.close": None,
    }
    io.update(over)
    return io


case("record_worker_loss", RWL_ARGS, rwl_io())
case("record_worker_loss", {"policy": {"kanban_json": "null"}, "reason": "x"})
case("record_worker_loss", {"policy": {"kanban_json": "{}"}, "reason": "x"})
case("record_worker_loss", RWL_ARGS, rwl_io(**{"conn.execute": script(["done", 5])}))
case("record_worker_loss", RWL_ARGS,
     rwl_io(**{"conn.execute": raise_script("ValueError", "bad db"),
               "log.exception": None}))
case("record_worker_loss", RWL_ARGS,
     rwl_io(**{"conn.execute": raise_script("SystemExit", "7")}))
case("record_worker_loss", RWL_ARGS, {}, fail_closed=True)
case("record_worker_loss",
     {"policy": {"kanban_json": jdump(dict(KCTX, run_id="5"))},
      "reason": "r" * 300, "pid": None, "bound": False},
     rwl_io())

# ── _bound_worker_matches ──
BWM_ARGS = {"conn": "conn", "context": {"task_id": "t1", "run_id": 5,
                                         "claim_lock": "L1"}}
case("_bound_worker_matches", BWM_ARGS,
     {"conn.execute": script([jdump({"pid": 9, "claim_lock": "L1"})]),
      "os.getpid": 9})
case("_bound_worker_matches", BWM_ARGS,
     {"conn.execute": script([jdump({"pid": 8, "claim_lock": "L1"})]),
      "os.getpid": 9})
case("_bound_worker_matches", BWM_ARGS,
     {"conn.execute": script(None), "os.getpid": 9})
case("_bound_worker_matches", BWM_ARGS,
     {"conn.execute": script([jdump({"pid": 9, "claim_lock": "OTHER"})]),
      "os.getpid": 9})
case("_bound_worker_matches", BWM_ARGS,
     {"conn.execute": script([jdump({"pid": 9, "claim_lock": "L1"})]),
      "os.getpid": raise_script("SystemExit", "8")})
case("_bound_worker_matches", BWM_ARGS, {}, fail_closed=True)

# ── _bound_claim_is_live ──
BCL_ARGS = {"context": {"task_id": "t1", "db": "/boards/b1.db", "run_id": 5,
                         "claim_lock": "L1"}}
def bcl_io(**over):
    io = {
        "worker_claim_is_live": True,
        "path.resolve": "/boards/b1.db",
        "path.as_uri": "file:///boards/b1.db",
        "sqlite3.connect": None,
        "bound_worker_matches": True,
        "conn.close": None,
    }
    io.update(over)
    return io


case("_bound_claim_is_live", BCL_ARGS, bcl_io())
case("_bound_claim_is_live", BCL_ARGS, bcl_io(**{"worker_claim_is_live": False}))
case("_bound_claim_is_live", BCL_ARGS,
     bcl_io(**{"sqlite3.connect": raise_script("sqlite3.Error", "locked")}))
case("_bound_claim_is_live", BCL_ARGS,
     bcl_io(**{"bound_worker_matches": raise_script("sqlite3.Error", "gone")}))
case("_bound_claim_is_live", BCL_ARGS,
     bcl_io(**{"bound_worker_matches": raise_script("RuntimeError", "boom")}))
case("_bound_claim_is_live", BCL_ARGS, {}, fail_closed=True)

# ── worker_result ──
WR_ARGS = {"path": "/boards/b1.db",
           "params": {"run_id": 5, "task_id": "t1", "claim_lock": "L1"}}
def wr_io(**over):
    io = {
        "path.resolve": "/boards/b1.db",
        "path.as_uri": "file:///boards/b1.db",
        "sqlite3.connect": None,
        "conn.execute": script([jdump({"claim_lock": "L1", "exit_code": 0})]),
        "conn.close": None,
    }
    io.update(over)
    return io


case("worker_result", WR_ARGS, wr_io())
case("worker_result", WR_ARGS,
     wr_io(**{"conn.execute": script([jdump({"claim_lock": "OTHER"})])}))
case("worker_result", WR_ARGS, wr_io(**{"conn.execute": script(None)}))
case("worker_result",
     {"path": "/boards/b1.db", "params": {"run_id": 5, "task_id": "t1"}},
     wr_io())
case("worker_result", WR_ARGS,
     wr_io(**{"conn.execute": raise_script("SystemExit", "9")}))
case("worker_result", WR_ARGS, {}, fail_closed=True)

# ── worker_exit_code ──
case("worker_exit_code", WR_ARGS, {"worker_result": {"exit_code": 3}})
case("worker_exit_code", WR_ARGS, {"worker_result": {}})
case("worker_exit_code", WR_ARGS, {"worker_result": {"exit_code": None}})
case("worker_exit_code", WR_ARGS,
     {"worker_result": raise_script("RuntimeError", "read failed")})
case("worker_exit_code", WR_ARGS, {}, fail_closed=True)

# ── _stored_conversation ──
SC_ARGS = {"agent": {"session_id": "s1", "_session_db": {}}}
case("_stored_conversation", SC_ARGS,
     {"agent_interface": None,
      "agent.get_messages_as_conversation": script([{"role": "user"}])})
case("_stored_conversation", SC_ARGS,
     {"agent_interface": {"messages": {}},
      "store.read_conversation": script([{}, {}]),
      "message.as_openai_message": [{"role": "user"}, {"role": "assistant"}]})
case("_stored_conversation", SC_ARGS,
     {"agent_interface": {"messages": {}},
      "store.read_conversation": script([])})
case("_stored_conversation", SC_ARGS,
     {"agent_interface": {"messages": {}},
      "store.read_conversation": raise_script("RuntimeError", "read failed")})
case("_stored_conversation", SC_ARGS,
     {"agent_interface": raise_script("SystemExit", "10")})
case("_stored_conversation", SC_ARGS, {}, fail_closed=True)

# ── _run_task_turns ──
RTT_CTX = {"skills": ["s1"], "context": "ctx", "task_id": "t1",
           "goal_mode": False, "goal_text": "goal", "goal_max_turns": None}
RTT_ARGS = {"agent": {"session_id": "s1"}, "frame": {"text": "do it"},
            "history": [], "context": RTT_CTX}
def rtt_io(**over):
    io = {
        "build_preloaded_skills_prompt": script(["SKILLS", ["s1"], []]),
        "agent.run_conversation": RESULT,
        "recover_failed_kanban_turns": None,
    }
    io.update(over)
    return io


case("_run_task_turns", RTT_ARGS, rtt_io())
case("_run_task_turns", RTT_ARGS,
     rtt_io(**{"build_preloaded_skills_prompt": script(["", [], ["s2"]])}))
case("_run_task_turns",
     dict(RTT_ARGS, context=dict(RTT_CTX, goal_mode=True)),
     rtt_io(**{"turn_exit_code": 0, "run_kanban_goal_loop": None}))
case("_run_task_turns",
     dict(RTT_ARGS, context=dict(RTT_CTX, goal_mode=True)),
     rtt_io(**{"turn_exit_code": 1}))
case("_run_task_turns",
     dict(RTT_ARGS, context=dict(RTT_CTX, goal_mode=True, goal_max_turns=3)),
     rtt_io(**{"turn_exit_code": 0, "run_kanban_goal_loop": None}))
case("_run_task_turns",
     dict(RTT_ARGS, context=dict(RTT_CTX, context="")),
     rtt_io(**{"build_preloaded_skills_prompt": script(["", ["s1"], []])}))
case("_run_task_turns", RTT_ARGS,
     rtt_io(**{"agent.run_conversation": raise_script("SystemExit", "11")}))
case("_run_task_turns", RTT_ARGS, {}, fail_closed=True)

# ── recover (closure) ──
REC_ARGS = {"agent": {"session_id": "s1"},
            "result": {"messages": [{"role": "user"}], "final_response": "old"},
            "nudge": "retry"}
case("recover", REC_ARGS, {"agent.run_conversation": {"final_response": "new"}})
case("recover", dict(REC_ARGS, result={}),
     {"stored_conversation": script([{"role": "user"}]),
      "agent.run_conversation": {"final_response": "new"}})
case("recover", dict(REC_ARGS, result="not-a-dict"),
     {"stored_conversation": script([]),
      "agent.run_conversation": {"final_response": "new"}})
case("recover", REC_ARGS,
     {"agent.run_conversation": raise_script("RuntimeError", "model down")})
case("recover", REC_ARGS, {}, fail_closed=True)

# ── status (closure) ──
ST_ARGS = {"context": {"task_id": "t1", "run_id": 5}}
case("status", ST_ARGS,
     {"kbc.connect_closing": None, "kb.goal_run_status": "ready",
      "conn.close": None})
case("status", ST_ARGS,
     {"kbc.connect_closing": None, "kb.goal_run_status": None,
      "conn.close": None})
case("status", ST_ARGS,
     {"kbc.connect_closing": None,
      "kb.goal_run_status": raise_script("RuntimeError", "status failed"),
      "conn.close": None})
case("status", ST_ARGS,
     {"kbc.connect_closing": None,
      "kb.goal_run_status": raise_script("SystemExit", "12"),
      "conn.close": None})
case("status", {"context": {"run_id": 5}},
     {"kbc.connect_closing": None, "conn.close": None})
case("status", ST_ARGS, {}, fail_closed=True)

# ── block (closure) ──
BL_ARGS = {"context": {"task_id": "t1", "run_id": 5}, "reason": "blocked"}
case("block", BL_ARGS,
     {"kbc.connect_closing": None, "kb.block_task": None, "conn.close": None})
case("block", dict(BL_ARGS, reason=None),
     {"kbc.connect_closing": None, "kb.block_task": None, "conn.close": None})
case("block", BL_ARGS,
     {"kbc.connect_closing": None,
      "kb.block_task": raise_script("RuntimeError", "block failed"),
      "conn.close": None})
case("block", BL_ARGS,
     {"kbc.connect_closing": None,
      "kb.block_task": raise_script("SystemExit", "13"), "conn.close": None})
case("block", BL_ARGS, {}, fail_closed=True)

# ── turn (closure) ──
TURN_ARGS = {"agent": {"session_id": "s1"}, "result": {"final_response": "old"},
             "context": RTT_CTX, "prompt": "next"}
case("turn", TURN_ARGS,
     {"stored_conversation": script([{"role": "user"}]),
      "agent.run_conversation": {"final_response": "new"}})
case("turn", dict(TURN_ARGS, result={}),
     {"stored_conversation": script([]),
      "agent.run_conversation": {"other": 1}})
case("turn", TURN_ARGS,
     {"stored_conversation": raise_script("RuntimeError", "store down")})
case("turn", TURN_ARGS,
     {"stored_conversation": script([]),
      "agent.run_conversation": raise_script("ValueError", "bad turn")})
case("turn", TURN_ARGS, {}, fail_closed=True)

# ── kanban_run ──
KR_ARGS = {"connection": CONN, "ref": None, "params": PARAMS}
case("kanban_run", KR_ARGS,
     {"run_task": {"session_id": "s1", "receipt": RECEIPT}})
case("kanban_run", KR_ARGS, {"run_task": {"session_id": "s2", "receipt": {}}})
case("kanban_run", KR_ARGS,
     {"run_task": raise_script("AuthorityRefusal", "permission_denied")})
case("kanban_run", KR_ARGS, {"run_task": raise_script("SystemExit", "14")})
case("kanban_run", KR_ARGS, {}, fail_closed=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [k for k, v in counts.items() if v < 5]
    print(f"gen_tui_methods_kanban_flow_cases: {len(CASES)} cases, {len(counts)} funcs")
    if thin:
        print(f"THIN (<5): {thin}")
        raise SystemExit(1)
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()