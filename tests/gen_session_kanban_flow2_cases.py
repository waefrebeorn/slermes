#!/usr/bin/env python3
"""Generate tests/fixtures/session_kanban_flow2.jsonl — deterministic cases for
the session_kanban remainder FLOW port (agent/kanban_turn_recovery.py +
hermes_cli/turn_exit.py). >=5 cases per function, covering decision branches,
seam call order, exception paths and fail-closed script gaps.

Each case: {"func", "in": {"args", "state", "io"}, "fail_closed"?: true}.
io maps seam-name -> result (plain value | call-order list | __script__ raise).
ASCII fixture data; source literals (em-dash/ellipsis) live in the port/oracle.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_kanban_flow2.jsonl")

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


# ── kanban_task_id ──
add("kanban_task_id", state={})
add("kanban_task_id", state={"environ": {"HERMES_KANBAN_TASK": "t1"}})
add("kanban_task_id", state={"environ": {"HERMES_KANBAN_TASK": "  t2  "}})
add("kanban_task_id", state={"environ": {"HERMES_KANBAN_TASK": "   "}})
add("kanban_task_id", state={"environ": {"HERMES_KANBAN_TASK": ""}})
add("kanban_task_id", state={"environ": {"HERMES_KANBAN_TASK": "\x1ct3\x1f"}})

# ── _resolve_task_id ──
add("_resolve_task_id", args={"task_id": None}, io={"kanban_task_id": "t1"})
add("_resolve_task_id", args={"task_id": "t2"})
add("_resolve_task_id", args={"task_id": "  "})
add("_resolve_task_id", args={"task_id": " t3 "})
add("_resolve_task_id", args={"task_id": None}, io={"kanban_task_id": None})
add("_resolve_task_id", args={"task_id": 5})

# ── kanban_turn_recovery_enabled ──
add("kanban_turn_recovery_enabled", args={"task_id": "t1"},
    io={"_resolve_task_id": "t1", "max_recovery_attempts": 3})
add("kanban_turn_recovery_enabled", args={"task_id": None}, io={"_resolve_task_id": None})
add("kanban_turn_recovery_enabled", args={"task_id": "t1"},
    io={"_resolve_task_id": "t1", "max_recovery_attempts": 0})
add("kanban_turn_recovery_enabled", args={"task_id": "t1"},
    io={"_resolve_task_id": raise_spec("ValueError", "x")})
add("kanban_turn_recovery_enabled", args={"task_id": "t1"},
    io={"_resolve_task_id": "t1", "max_recovery_attempts": raise_spec("OSError", "y")})
add("kanban_turn_recovery_enabled", args={"task_id": "t1"},
    io={"_resolve_task_id": "t1", "max_recovery_attempts": 10})

# ── max_recovery_attempts ──
for raw, _ in [(None, 3), ("5", 5), ("0", 0), ("false", 0), ("OFF", 0), (" 7 ", 7),
               ("12", 10), ("-3", 0), ("abc", 3), ("1_0", 10), ("", 3), ("+4", 4)]:
    st = {} if raw is None else {"environ": {"HERMES_KANBAN_TURN_RECOVERY": raw}}
    add("max_recovery_attempts", state=st)

# ── recovery_delay_seconds ──
for at in (1, 2, 3, 4, 0, -5):
    add("recovery_delay_seconds", args={"attempt": at})
add("recovery_delay_seconds", args={})   # missing positional -> TypeError

# ── _turn_exit_is_terminal ──
add("_turn_exit_is_terminal", args={"result": {"turn_exit_reason": "max_iterations_reached(3/5)"}})
add("_turn_exit_is_terminal", args={"result": {"turn_exit_reason": "other"}})
add("_turn_exit_is_terminal", args={"result": {}})
add("_turn_exit_is_terminal", args={"result": {"turn_exit_reason": None}})
add("_turn_exit_is_terminal", args={"result": {"turn_exit_reason": "max_iterations_reached"}})
add("_turn_exit_is_terminal", args={"result": None})
add("_turn_exit_is_terminal", args={"result": {"turn_exit_reason": 0}})

# ── should_recover_turn ──
GOOD = {"failed": True, "failure_retryable": True}
BASE_IO = {"kanban_turn_recovery_enabled": True, "max_recovery_attempts": 3,
           "_turn_exit_is_terminal": False}
add("should_recover_turn", args={"result": GOOD, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": GOOD, "attempt": 0},
    io={**BASE_IO, "kanban_turn_recovery_enabled": False})
add("should_recover_turn", args={"result": GOOD, "attempt": 3}, io=BASE_IO)
add("should_recover_turn", args={"result": None, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": {**GOOD, "interrupted": True}, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": GOOD, "attempt": 0},
    io={**BASE_IO, "_turn_exit_is_terminal": True})
add("should_recover_turn", args={"result": {"failed": False, "failure_retryable": True}, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": {"failed": True, "failure_retryable": False}, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": {**GOOD, "failure_reason": "rate_limit"}, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": {**GOOD, "failure_reason": "timeout"}, "attempt": 0}, io=BASE_IO)
add("should_recover_turn", args={"result": GOOD}, io=BASE_IO)   # missing attempt -> TypeError
add("should_recover_turn", args={"result": {"failed": 1, "failure_retryable": True}, "attempt": 0}, io=BASE_IO)

# ── _kanban_db_path ──
add("_kanban_db_path", state={})
add("_kanban_db_path", state={"environ": {"HERMES_KANBAN_DB": "/boards/main.db"}})
add("_kanban_db_path", state={"environ": {"HERMES_KANBAN_DB": "  /b.db  "}})
add("_kanban_db_path", state={"environ": {"HERMES_KANBAN_DB": "   "}})
add("_kanban_db_path", state={"environ": {"HERMES_KANBAN_DB": ""}})

# ── _now ──
add("_now", io={"time.time": 1000.9})
add("_now", io={"time.time": 0})
add("_now", io={"time.time": -5.5})
add("_now", io={"time.time": raise_spec("OSError", "clock")})
add("_now", io={"time.time": 1234567890.123})

# ── worker_claim_is_live ──
TASKROW = {"status": "running", "worker_pid": 9, "current_run_id": 5,
           "claim_lock": "L", "claim_expires": 2000}
RUNROW = {"ended_at": None, "worker_pid": 9, "claim_expires": 2000}
WCL_ARGS = {"task_id": "t1", "db_path": "/boards/main.db", "run_id": "5", "claim_lock": "L"}
WCL_IO = {"_resolve_task_id": "t1", "path.exists": True, "_now": 1000,
          "path.as_uri": "file:///boards/main.db", "sqlite3.connect": "conn",
          "conn.execute": [TASKROW, RUNROW], "os.getpid": 9, "conn.close": None}


def wcl(args=None, io=None, state=None, fail_closed=False):
    a = dict(WCL_ARGS)
    if args is not None:
        a = args
    i = dict(WCL_IO)
    if io is not None:
        i.update(io)
    add("worker_claim_is_live", args=a, io=i, state=state, fail_closed=fail_closed)


wcl()                                                        # full live -> True
wcl(io={"_resolve_task_id": None})                           # no task -> False
wcl(args={"task_id": "t1"}, io={"_kanban_db_path": None,
                                "log.warning": None})        # db null -> warn
wcl(io={"path.exists": False, "log.warning": None})          # db missing -> warn
wcl(args={"task_id": "t1", "db_path": "/b.db", "claim_lock": "L"},
    io={"log.warning": None})                                # run_id missing -> warn
wcl(io={"conn.execute": [None]})                             # task row null
wcl(io={"conn.execute": [{**TASKROW, "status": "blocked"}, RUNROW]})
wcl(io={"conn.execute": [{**TASKROW, "worker_pid": 8}, RUNROW]})   # pid mismatch
wcl(io={"conn.execute": [{**TASKROW, "current_run_id": 6}, RUNROW]})
wcl(io={"conn.execute": [{**TASKROW, "claim_lock": "X"}, RUNROW]})
wcl(io={"conn.execute": [{**TASKROW, "claim_expires": 500}, RUNROW]})  # expired
wcl(io={"conn.execute": [TASKROW, None]})                    # run row null
wcl(io={"conn.execute": [TASKROW, {**RUNROW, "ended_at": 123}]})
wcl(args={**WCL_ARGS, "verify_pid": False})                  # skip pid checks
wcl(io={"conn.execute": raise_spec("ValueError", "db"), "log.warning": None})     # except -> warn(exc_info)
wcl(io={"sqlite3.connect": raise_spec("OSError", "open"), "log.warning": None})   # except, no close
wcl(io={"conn.execute": [{**TASKROW, "worker_pid": "abc"}, RUNROW], "log.warning": None})  # int coercion
wcl(args={}, state={"environ": {"HERMES_KANBAN_TASK": "t1", "HERMES_KANBAN_DB": "/b.db",
                                "HERMES_KANBAN_RUN_ID": "5", "HERMES_KANBAN_CLAIM_LOCK": "L"}},
    io={"_kanban_db_path": "/b.db"})                         # env-pin path
wcl(io={"conn.execute": raise_spec("SystemExit", "1")})      # BaseException passthrough

# ── _truncate ──
add("_truncate", args={"text": "a\nb"})
add("_truncate", args={"text": "  hi  "})
add("_truncate", args={"text": None})
add("_truncate", args={"text": "abcdefgh", "limit": 5})
add("_truncate", args={"text": "abc", "limit": 5})
add("_truncate", args={"text": "abcde", "limit": 5})
add("_truncate", args={"text": "abcdef", "limit": 1})
add("_truncate", args={"text": 5})

# ── build_recovery_nudge ──
add("build_recovery_nudge", args={"result": {"error": "boom"}, "attempt": 1, "max_attempts": 3, "task_id": "t1"},
    io={"_truncate": "boom", "_resolve_task_id": "t1"})
add("build_recovery_nudge", args={"result": {}, "attempt": 1, "max_attempts": 3},
    io={"_truncate": "", "_resolve_task_id": None})
add("build_recovery_nudge", args={"result": {"final_response": "resp"}, "attempt": 2, "max_attempts": 3, "task_id": "t2"},
    io={"_truncate": "resp", "_resolve_task_id": "t2"})
add("build_recovery_nudge", args={"result": None, "attempt": 1, "max_attempts": 3, "task_id": "t3"},
    io={"_resolve_task_id": "t3"})
add("build_recovery_nudge", args={"result": {}})             # missing attempt+max
add("build_recovery_nudge", args={"result": {}, "attempt": 1})   # missing max
add("build_recovery_nudge", args={"result": {"error": "x"}, "attempt": 1, "max_attempts": 3},
    io={"_truncate": raise_spec("ValueError", "t")})
add("build_recovery_nudge", args={"result": {"error": "a\nb"}, "attempt": 1, "max_attempts": 3, "task_id": "t1"},
    io={"_truncate": "a b", "_resolve_task_id": "t1"})

# ── _emit_recovery_skipped ── (emit is a REQUIRED positional arg)
add("_emit_recovery_skipped", args={"emit": None, "task_id": "t1"}, io={"_resolve_task_id": "t1", "log.warning": None})
add("_emit_recovery_skipped", args={"emit": True, "task_id": "t1"},
    io={"_resolve_task_id": "t1", "log.warning": None, "emit": None})
add("_emit_recovery_skipped", args={"emit": True, "task_id": "t1"},
    io={"_resolve_task_id": "t1", "log.warning": None, "emit": raise_spec("ValueError", "e"), "log.debug": None})
add("_emit_recovery_skipped", args={"emit": True, "task_id": "t1"},
    io={"_resolve_task_id": "t1", "log.warning": None, "emit": raise_spec("SystemExit", "1")})
add("_emit_recovery_skipped", args={"emit": None, "task_id": None}, io={"_resolve_task_id": None, "log.warning": None})
add("_emit_recovery_skipped", args={"emit": None, "task_id": "t1"},
    io={"_resolve_task_id": "t1", "log.warning": raise_spec("OSError", "log")})
add("_emit_recovery_skipped", args={"task_id": "t1"})        # missing emit -> TypeError

# ── recover_failed_kanban_turns ──
R = {"failed": True, "failure_retryable": True}
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True},
    io={"get_result": None, "should_recover_turn": False})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "task_id": "t1"},
    io={"get_result": R, "should_recover_turn": True, "claim_check": False, "_emit_recovery_skipped": None})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "task_id": "t1"},
    io={"get_result": [R, R], "should_recover_turn": [True, False], "claim_check": [True, True],
        "recovery_delay_seconds": 15, "_resolve_task_id": "t1", "max_recovery_attempts": [3, 3],
        "log.warning": None, "time.sleep": None, "build_recovery_nudge": "nudge", "turn_fn": None})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "task_id": "t1"},
    io={"get_result": [R, R], "should_recover_turn": [True, False], "worker_claim_is_live": [True, True],
        "recovery_delay_seconds": 15, "_resolve_task_id": "t1", "max_recovery_attempts": [3, 3],
        "log.warning": None, "time.sleep": None, "build_recovery_nudge": "nudge", "turn_fn": None})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "task_id": "t1"},
    io={"get_result": R, "should_recover_turn": True, "claim_check": [True, False],
        "recovery_delay_seconds": 15, "_resolve_task_id": "t1", "max_recovery_attempts": 3,
        "log.warning": None, "time.sleep": None, "_emit_recovery_skipped": None})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "emit": True, "task_id": "t1"},
    io={"get_result": [R, R], "should_recover_turn": [True, False], "claim_check": [True, True],
        "recovery_delay_seconds": 15, "_resolve_task_id": "t1", "max_recovery_attempts": [3, 3],
        "log.warning": None, "emit": raise_spec("ValueError", "e"), "log.debug": None,
        "time.sleep": None, "build_recovery_nudge": "nudge", "turn_fn": None})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "sleep_fn": True, "task_id": "t1"},
    io={"get_result": [R, R], "should_recover_turn": [True, False], "claim_check": [True, True],
        "recovery_delay_seconds": 15, "_resolve_task_id": "t1", "max_recovery_attempts": [3, 3],
        "log.warning": None, "sleep_fn": None, "build_recovery_nudge": "nudge", "turn_fn": None})
add("recover_failed_kanban_turns", args={})                  # missing positionals
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True},
    io={"get_result": R, "should_recover_turn": raise_spec("ValueError", "s")})
add("recover_failed_kanban_turns", args={"turn_fn": True, "get_result": True, "claim_check": True, "task_id": "t1"},
    io={"get_result": [R, R, R], "should_recover_turn": [True, True, False],
        "claim_check": [True, True, True, True], "recovery_delay_seconds": [15, 45],
        "_resolve_task_id": ["t1", "t1"], "max_recovery_attempts": [3, 3, 3, 3],
        "log.warning": [None, None], "time.sleep": [None, None],
        "build_recovery_nudge": ["n1", "n2"], "turn_fn": [None, None]})

# ── turn_exit_code ──
add("turn_exit_code", args={"result": {"completed": True}, "kanban_worker": True})
add("turn_exit_code", args={"result": None, "kanban_worker": True})
add("turn_exit_code", args={"result": None, "kanban_worker": True, "credentials_rate_limited": True})
add("turn_exit_code", args={"result": None, "kanban_worker": True, "credentials_terminal": True})
add("turn_exit_code", args={"result": None, "kanban_worker": False, "credentials_rate_limited": True})
add("turn_exit_code", args={"result": {"interrupted": True}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "rate_limit"}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "auth"}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "other"}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "auth"}, "kanban_worker": False})
add("turn_exit_code", args={"result": {"partial": True}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"completed": False}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"completed": 0}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "timeout"}, "kanban_worker": True,
                            "transient_reasons": ["timeout"]})
add("turn_exit_code", args={"result": {}})                   # missing kanban_worker
add("turn_exit_code", args={"result": {"interrupted": 1}, "kanban_worker": True})
add("turn_exit_code", args={"result": {"failed": True, "failure_reason": "billing"}, "kanban_worker": True})

# ── credential_failure_flags ──
add("credential_failure_flags", args={"exc": None})
add("credential_failure_flags", args={"exc": [{"kind": "AuthError", "message": "m", "relogin_required": True}]},
    io={"is_rate_limited_auth_error": False})
add("credential_failure_flags", args={"exc": [{"kind": "AuthError", "message": "m", "relogin_required": False}]},
    io={"is_rate_limited_auth_error": True})
add("credential_failure_flags", args={"exc": [{"kind": "ValueError", "message": "v"}]})
add("credential_failure_flags",
    args={"exc": [{"kind": "RuntimeError", "message": "wrap", "cause": 1},
                  {"kind": "AuthError", "message": "a", "relogin_required": True}]},
    io={"is_rate_limited_auth_error": False})
add("credential_failure_flags",
    args={"exc": [{"kind": "ValueError", "message": "v", "context": 1},
                  {"kind": "AuthError", "message": "a", "relogin_required": False}]},
    io={"is_rate_limited_auth_error": True})
add("credential_failure_flags", args={"exc": [{"kind": "ValueError", "message": "v", "context": 0}]})
add("credential_failure_flags",
    args={"exc": [{"kind": "ValueError", "message": "v", "cause": 1, "context": 2},
                  {"kind": "AuthError", "message": "a", "relogin_required": False},
                  {"kind": "AuthError", "message": "b", "relogin_required": True}]},
    io={"is_rate_limited_auth_error": False})
add("credential_failure_flags", args={"exc": [{"kind": "AuthError", "message": "m"}]},
    io={"is_rate_limited_auth_error": raise_spec("OSError", "r")})
add("credential_failure_flags", args={"exc": [{"kind": "AuthError", "message": "m"}]}, fail_closed=True)
add("credential_failure_flags", args={"exc": []})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"THIN COVERAGE: {thin}")


if __name__ == "__main__":
    main()