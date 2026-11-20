#!/usr/bin/env python3
"""Generate tests/fixtures/kanban_db_dispatch_flow2.jsonl for the
kanban_db_dispatch dead-worker / crash-accounting remainder FLOW port.

Each case is {"func", "in": {"args", "io", "state"}, ["fail_closed"]}. The io
map scripts every universal seam the flow touches, in call order. A bare LIST
is consumed one element per call; to return a list VERBATIM (e.g. the
[kind, code] pair from _classify_worker_exit, or the prefix tuple from
_log_noise_prefixes) wrap it as {"__script__": true, "ret": [...]}. A plain
scalar/dict is returned verbatim on every call. >=5 cases per ported func,
covering happy paths, branch gates, scripted real exceptions, BaseException
passthrough, context-manager enter/exit/suppress, and fail-closed missing
scripts.

Ported siblings are SEAMED inside flows: _worker_final_output scripts
_exit_summary_marker / _log_noise_prefixes (NOT i18n.t); _classify_dead_worker
scripts _classify_dead_worker_exit / _worker_final_output; _reclaim_dead_workers
scripts _classify_dead_worker. Each sibling's real body runs only in its own
direct cases.

Fixture DATA is ASCII; source literals (em-dash error texts) are produced by the
live bodies, not shipped here. Run: python tests/gen_kanban_db_dispatch_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "kanban_db_dispatch_flow2.jsonl")

CASES = []


def add(func, args=None, io=None, fail_closed=False):
    rec = {"func": func, "in": {"args": args or {}, "io": io or {}, "state": {}}}
    if fail_closed:
        rec["fail_closed"] = True
    CASES.append(rec)


def ret(v):
    """Return a list/value VERBATIM on every call (not a per-call sequence)."""
    return {"__script__": True, "ret": v}


def dead(kind="nonzero_exit", code=1, error_text=None, event_kind="crashed",
         event_payload=None, **flags):
    if error_text is None:
        error_text = f"pid 5 exited with code {code}"
    if event_payload is None:
        event_payload = {"pid": 5, "claimer": "c"}
    d = {"kind": kind, "code": code, "error_text": error_text,
         "event_kind": event_kind, "event_payload": event_payload}
    d.update(flags)
    return d


def raise_spec(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def cursor(rows=None, rowcount=-1):
    return {"rows": rows if rows is not None else [], "rowcount": rowcount}


PREFIXES = ret(["session_id:", "Query:", "Init"])

# ---- describe_suppression ----
add("describe_suppression", {"results": []})
add("describe_suppression", {"results": [None]})
add("describe_suppression", {"results": [
    {"respawn_guarded": [["t1", "active_pr"], ["t2", "recent_success"], ["t3", "active_pr"]],
     "rate_limited": ["t4"], "skipped_locked": True, "memory_pressure": "critical"}]})
add("describe_suppression", {"results": [
    {"respawn_guarded": [["t1", "active_pr"]], "rate_limited": [],
     "skipped_locked": False, "memory_pressure": "elevated"},
    {"respawn_guarded": [["t2", "active_pr"]], "rate_limited": ["t2", "t3"],
     "skipped_locked": True, "memory_pressure": "critical"}]})
add("describe_suppression", {"results": [
    {"respawn_guarded": [], "rate_limited": [], "skipped_locked": False, "memory_pressure": None}]})
add("describe_suppression", {"results": [{}]})
add("describe_suppression", {"results": [
    {"respawn_guarded": [["t1"]], "rate_limited": [], "skipped_locked": False, "memory_pressure": None}]})
add("describe_suppression", {"results": [
    {"respawn_guarded": [], "rate_limited": ["a", "b"], "skipped_locked": False, "memory_pressure": None}]})

# ---- _worker_log_exit_code ----
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": "log\n[kanban-worker-exit] rc=0\n"})
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": "no trailer"})
add("_worker_log_exit_code", {"task_id": "t"},
    {"kb.read_worker_log": "[kanban-worker-exit] rc=1\n[kanban-worker-exit] rc=75\n"})
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": raise_spec("FileNotFoundError", "no log")})
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": raise_spec("SystemExit", "bye")})
add("_worker_log_exit_code", {"task_id": "t"}, {}, fail_closed=True)
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": None})
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": "x[kanban-worker-exit] rc=5"})
add("_worker_log_exit_code", {"task_id": "t"}, {"kb.read_worker_log": "[kanban-worker-exit] rc=12abc"})

# ---- _exit_summary_marker (direct: real body calls i18n.t) ----
add("_exit_summary_marker", {}, {"i18n.t": "Resume hint"})
add("_exit_summary_marker", {}, {"i18n.t": ""})
add("_exit_summary_marker", {}, {"i18n.t": raise_spec("ValueError", "bad key")})
add("_exit_summary_marker", {}, {"i18n.t": raise_spec("KeyboardInterrupt", "int")})
add("_exit_summary_marker", {}, {}, fail_closed=True)
add("_exit_summary_marker", {}, {"i18n.t": None})

# ---- _log_noise_prefixes (direct: real body calls i18n.t) ----
add("_log_noise_prefixes", {}, {"i18n.t": "Initializing"})
add("_log_noise_prefixes", {}, {"i18n.t": ""})
add("_log_noise_prefixes", {}, {"i18n.t": raise_spec("RuntimeError", "i18n down")})
add("_log_noise_prefixes", {}, {}, fail_closed=True)
add("_log_noise_prefixes", {}, {"i18n.t": "Agent starting"})
add("_log_noise_prefixes", {}, {"i18n.t": None})

# ---- _worker_final_output (siblings seamed) ----
add("_worker_final_output", {"task_id": "t"},
    {"kb.read_worker_log": "hello world\n", "_exit_summary_marker": "EXIT", "_log_noise_prefixes": PREFIXES})
add("_worker_final_output", {"task_id": "t"},
    {"kb.read_worker_log": "[kanban-worker-exit] rc=0\nsession_id: abc\nQuery: q\nreal reason\n",
     "_exit_summary_marker": "EXIT", "_log_noise_prefixes": PREFIXES})
add("_worker_final_output", {"task_id": "t"}, {"kb.read_worker_log": ""})
add("_worker_final_output", {"task_id": "t"}, {"kb.read_worker_log": raise_spec("FileNotFoundError", "no log")})
add("_worker_final_output", {"task_id": "t"}, {"kb.read_worker_log": raise_spec("SystemExit", "bye")})
add("_worker_final_output", {"task_id": "t"},
    {"kb.read_worker_log": "before\nEXIT summary\nafter", "_exit_summary_marker": "EXIT", "_log_noise_prefixes": PREFIXES})
add("_worker_final_output", {"task_id": "t"},
    {"kb.read_worker_log": "a" * 500, "_exit_summary_marker": "EXIT", "_log_noise_prefixes": PREFIXES})
add("_worker_final_output", {"task_id": "t"}, {}, fail_closed=True)
add("_worker_final_output", {"task_id": "t"},
    {"kb.read_worker_log": "abc", "_exit_summary_marker": "", "_log_noise_prefixes": PREFIXES})
add("_worker_final_output", {"task_id": "t", "board": "b"},
    {"kb.read_worker_log": "line one\nline two\n", "_exit_summary_marker": "EXIT", "_log_noise_prefixes": PREFIXES})

# ---- run_outcome ----
add("run_outcome", {"dead": dead(kind="rate_limited", code=75, event_kind="rate_limited", rate_limited=True)})
add("run_outcome", {"dead": dead(rate_limited=False)})
add("run_outcome", {"dead": dead()})
add("run_outcome", {"dead": dead(rate_limited=1)})
add("run_outcome", {"dead": {"code": 1, "error_text": "e", "event_kind": "crashed", "event_payload": {}}})
add("run_outcome", {"dead": dead(extra_key=1)})
add("run_outcome", {"dead": dead(protocol_violation=True, rate_limited=False)})

# ---- _classify_dead_worker (siblings seamed) ----
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": dead(), "_worker_final_output": "boom"})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": dead(kind="rate_limited", code=75, event_kind="rate_limited", rate_limited=True)})
add("_classify_dead_worker", {"pid": 5, "claimer": "c"},
    {"_classify_dead_worker_exit": dead()})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": dead(), "_worker_final_output": ""})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": raise_spec("RuntimeError", "classify failed")})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": dead(), "_worker_final_output": "it's fine"})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"}, {}, fail_closed=True)
add("_classify_dead_worker", {"pid": 5, "claimer": "c"},
    {"_classify_dead_worker_exit": {"code": 1, "error_text": "e", "event_kind": "crashed", "event_payload": {}}})
add("_classify_dead_worker", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_dead_worker_exit": dead(), "_worker_final_output": "a\nb"})

# ---- _classify_dead_worker_exit ----
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c"},
    {"_classify_worker_exit": ret(["signaled", 9])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "exit_code": 75},
    {"_classify_worker_exit": ret(["unknown", None]), "_exit_code_kind": ret(["rate_limited", 75])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_worker_exit": ret(["unknown", None]), "_worker_log_exit_code": 0,
     "_exit_code_kind": ret(["clean_exit", 0])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "task_id": "t1"},
    {"_classify_worker_exit": ret(["unknown", None]), "_worker_log_exit_code": None})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c"},
    {"_classify_worker_exit": ret(["clean_exit", 0])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "exit_code": 76},
    {"_classify_worker_exit": ret(["unknown", None]), "_exit_code_kind": ret(["terminal_provider", 76])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "exit_code": "75"},
    {"_classify_worker_exit": ret(["unknown", None]), "_exit_code_kind": ret(["rate_limited", 75])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c", "exit_code": "bad"},
    {"_classify_worker_exit": ret(["unknown", None])})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c"},
    {"_classify_worker_exit": raise_spec("SystemExit", "bye")})
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c"}, {}, fail_closed=True)
add("_classify_dead_worker_exit", {"pid": 5, "claimer": "c"},
    {"_classify_worker_exit": ret(["unknown", None])})

# ---- _reclaim_dead_workers ----
ROW = {"id": "t1", "worker_pid": 5, "worker_started_at": "fp",
       "claim_lock": "host:abc", "started_at": 100, "assignee": "sage"}


def reclaim_io(rows=None, dead_rec=None, rowcount=1, extra_conns=None, **over):
    io = {
        "kb.write_txn.__enter__": None,
        "kb.write_txn.__exit__": False,
        "conn.execute": [cursor(rows if rows is not None else [ROW]), cursor([], rowcount)]
                        + (extra_conns or []),
        "kb._host_prefix": "host:",
        "kb._row_get": [100, "fp"],
        "kb._to_epoch": 100,
        "time.time": 1000,
        "kb._resolve_crash_grace_seconds": 30,
        "_worker_alive": False,
        "owner_reclaim_blocked": False,
        "managed_run_exit_code": None,
        "_classify_dead_worker": dead_rec if dead_rec is not None else dead(),
        "apply_attempt_note": None,
        "kb._retry_status_for_run": "ready",
        "kb._end_run": 42,
        "kb._append_event": None,
    }
    io.update(over)
    return io


add("_reclaim_dead_workers", {},
    {"kb.write_txn.__enter__": None, "kb.write_txn.__exit__": False,
     "conn.execute": cursor([]), "kb._host_prefix": "host:"})
add("_reclaim_dead_workers", {"board": "b"}, reclaim_io())
add("_reclaim_dead_workers", {},
    reclaim_io(dead_rec=dead(kind="clean_exit", code=0, error_text="x" * 600,
                             event_kind="protocol_violation", protocol_violation=True),
               extra_conns=[cursor([], 1)]))
add("_reclaim_dead_workers", {},
    reclaim_io(dead_rec=dead(kind="rate_limited", code=75, event_kind="rate_limited", rate_limited=True),
               extra_conns=[cursor([], 1)]))
io_rc0 = reclaim_io(rowcount=0)
del io_rc0["kb._end_run"], io_rc0["kb._append_event"]
add("_reclaim_dead_workers", {}, io_rc0)
add("_reclaim_dead_workers", {},
    {"kb.write_txn.__enter__": None, "kb.write_txn.__exit__": False,
     "conn.execute": cursor([dict(ROW, claim_lock="other:abc")]), "kb._host_prefix": "host:"})
add("_reclaim_dead_workers", {}, reclaim_io(rows=[ROW], **{"time.time": 110}))
add("_reclaim_dead_workers", {}, reclaim_io(**{"_worker_alive": True}))
add("_reclaim_dead_workers", {}, reclaim_io(**{"owner_reclaim_blocked": True}))
add("_reclaim_dead_workers", {},
    {"kb.write_txn.__enter__": raise_spec("RuntimeError", "lock busy")})
add("_reclaim_dead_workers", {},
    reclaim_io(**{"managed_run_exit_code": raise_spec("RuntimeError", "authority gone")}))
add("_reclaim_dead_workers", {},
    reclaim_io(**{"_classify_dead_worker": raise_spec("SystemExit", "bye")}))
add("_reclaim_dead_workers", {},
    {"kb.write_txn.__enter__": None, "kb.write_txn.__exit__": False}, fail_closed=True)
add("_reclaim_dead_workers", {},
    reclaim_io(**{"managed_run_exit_code": raise_spec("RuntimeError", "authority gone"),
                  "kb.write_txn.__exit__": True}))

# ---- _account_crashes ----
add("_account_crashes", {"crash_details": []})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead()]]},
    {"_error_fingerprint": ["fp1", "fp1"], "_record_task_failure": False})
add("_account_crashes", {"crash_details": [
    ["t1", 5, "c", dead(error_text="same error")],
    ["t2", 6, "c", dead(error_text="same error")],
    ["t3", 7, "c", dead(error_text="same error")]]},
    {"_error_fingerprint": ["fp1"] * 6, "_record_task_failure": [True, True, True]})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead(protocol_violation=True, error_text="pv")]]},
    {"_error_fingerprint": "fp1", "_protocol_violation_streak": 1,
     "conn.execute": cursor([{"max_retries": None}], 1), "kb._row_get": None})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead(protocol_violation=True, error_text="pv")]]},
    {"_error_fingerprint": "fp1", "_protocol_violation_streak": 3,
     "conn.execute": cursor([{"max_retries": None}], 1), "kb._row_get": None,
     "_record_task_failure": True})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead(protocol_violation=True, error_text="pv")]]},
    {"_error_fingerprint": "fp1", "_protocol_violation_streak": 2,
     "conn.execute": cursor([{"max_retries": 2}], 1), "kb._row_get": 2,
     "_record_task_failure": True})
add("_account_crashes", {"crash_details": [
    ["t1", 5, "c", dead(kind="terminal_provider", code=76, terminal_provider=True)]]},
    {"_error_fingerprint": "fp1", "_record_task_failure": True})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead(protocol_violation=True, error_text="pv")]]},
    {"_error_fingerprint": "fp1", "_protocol_violation_streak": 3, "conn.execute": cursor([], 0)})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead()]]},
    {"_error_fingerprint": ["fp1", "fp1"], "_record_task_failure": raise_spec("SystemExit", "x")})
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead()]]}, {}, fail_closed=True)
add("_account_crashes", {"crash_details": [["t1", 5, "c", dead(protocol_violation=True, error_text="pv")]]},
    {"_error_fingerprint": "fp1", "_protocol_violation_streak": 5,
     "conn.execute": cursor([{"max_retries": "bad"}], 1), "kb._row_get": "bad"})

# ---- _is_handoff_event ----
add("_is_handoff_event", {"kind": "comment", "payload": None})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": {"assignee": "b", "from": "a", "source": "cli"}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": {"assignee": "b", "from": "b"}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": {"assignee": "b", "from": "a", "source": "kanban.default_assignee"}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": {"assignee": "b"}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"}, {"kb._json_or": None})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"}, {"kb._json_or": [1, 2]})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"}, {"kb._json_or": {"assignee": ""}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": {"assignee": "b", "from": None}})
add("_is_handoff_event", {"kind": "assigned", "payload": "p"}, {}, fail_closed=True)
add("_is_handoff_event", {"kind": "assigned", "payload": "p"},
    {"kb._json_or": raise_spec("SystemExit", "bye")})

# ---- adopt_worker_pid ----
AROW = {"status": "running", "current_run_id": 7, "worker_pid": None, "claim_lock": "host:x"}


def adopt_io(row=None, fp="fp", conns=None, **over):
    io = {
        "_process_fingerprint": fp,
        "kb.write_txn.__enter__": None,
        "kb.write_txn.__exit__": False,
        "conn.execute": conns if conns is not None else [cursor([row if row is not None else AROW])],
        "kb._host_prefix": "host:",
        "kb._append_event": None,
    }
    io.update(over)
    return io


add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(conns=[cursor([], 0)]))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(row=dict(AROW, status="ready")))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(row=dict(AROW, current_run_id=8)))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(conns=[cursor([AROW]), cursor([], 1), cursor([], 1)]))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(row=dict(AROW, worker_pid=9)))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(row=dict(AROW, claim_lock="other:x")))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    adopt_io(fp=None, conns=[cursor([AROW]), cursor([], 1), cursor([], 1)]))
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": "bad"}, {})
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    {"_process_fingerprint": "fp", "kb.write_txn.__enter__": raise_spec("RuntimeError", "lock busy")})
add("adopt_worker_pid", {"task_id": "t1", "run_id": 7, "pid": 5},
    {"_process_fingerprint": "fp", "kb.write_txn.__enter__": None, "kb.write_txn.__exit__": False},
    fail_closed=True)
add("adopt_worker_pid", {"task_id": "t1", "run_id": "bad", "pid": 5},
    adopt_io(conns=[cursor([AROW])]))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, c in counts.items() if c < 5]
    if thin:
        raise SystemExit(f"THIN COVERAGE (<5): {thin}")


if __name__ == "__main__":
    main()
