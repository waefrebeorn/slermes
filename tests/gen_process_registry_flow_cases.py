#!/usr/bin/env python3
"""Generate tests/fixtures/process_registry_flow.jsonl for the
process_registry_flow oracle. Each case is {"func", "in"}; "in" carries the
registry/session state, flow params and the "io" seam scripts (single value
reused, or array consumed per call). >=5 cases per flow, branch-covering.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "process_registry_flow.jsonl")

DEFAULTS = {
    "id": "", "command": "", "task_id": "", "owner_task_id": "", "session_key": "",
    "sandbox_key": "", "cwd": None, "pid": None, "pid_scope": "host",
    "host_start_time": None, "started_at": 0.0, "exited": False, "exited_at": 0.0,
    "exit_code": None, "completion_reason": "exited", "termination_source": "",
    "output_buffer": "", "max_output_chars": 200000, "total_output_chars": 0,
    "detached": False, "systemd_unit": "", "output_log": "", "notify_on_complete": False,
    "handoff_note": "", "persist_on_release": False, "completion_output_chars": 0,
    "_finish_claimed": False, "_scope_stop_pending": False,
    "_completion_event_set": False, "_reader_finish_requested": False,
    "_reader_thread": None, "process": None, "_pty": None, "env_ref": None,
}


def sess(id, **kw):
    d = dict(DEFAULTS)
    d["id"] = id
    d.update(kw)
    return d


def proc(pid=100, stdout=True, stderr=True, stdin=True, pgid=None):
    return {"pid": pid, "stdout": {} if stdout else None, "stderr": {} if stderr else None,
            "stdin": {} if stdin else None, "_hermes_owned_pgid": pgid}


def state(sessions=None, running=None, finished=None, consumed=None, polled=None,
          queue=None, restored=False):
    sessions = sessions or {}
    return {"sessions": sessions, "running": running or [], "finished": finished or [],
            "completion_consumed": consumed or [], "poll_observed": polled or [],
            "completion_queue": queue or [], "completions_restored": restored}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def rais(kind, msg):
    return {"__raise__": kind, "msg": msg}


# ── append_locked ──
add("append_locked", session=sess("p1", output_buffer="abc", max_output_chars=10, total_output_chars=3), text="de")
add("append_locked", session=sess("p1", output_buffer="abc", max_output_chars=5, total_output_chars=3), text="defgh")
add("append_locked", session=sess("p1", output_buffer="x", max_output_chars=5, total_output_chars=1), text="")
add("append_locked", session=sess("p1", output_buffer="\u03b1\u03b2", max_output_chars=3, total_output_chars=2), text="\u03b3\u03b4\u03b5")
add("append_locked", session=sess("p1", output_buffer="ab", max_output_chars=4, total_output_chars=2), text="cd")
add("append_locked", session=sess("p1", output_buffer="abc", max_output_chars=0, total_output_chars=3), text="d")

# ── append_output_if_running ──
add("append_output_if_running", session=sess("p1", exited=False, output_buffer="a", max_output_chars=10), text="b")
add("append_output_if_running", session=sess("p1", exited=True, output_buffer="a"), text="b")
add("append_output_if_running", session=sess("p1", exited=False, output_buffer="abc", max_output_chars=4, total_output_chars=3), text="de")
add("append_output_if_running", session=sess("p1", exited=False, output_buffer="a"), text="")
add("append_output_if_running", session=sess("p1", exited=True, exit_code=0, output_buffer="a"), text="ignored")

# ── mark_exited ──
add("mark_exited", session=sess("p1", exited=False, termination_source="old"), exit_code=0, reason="exited", source="kill")
add("mark_exited", session=sess("p1", exited=True, exit_code=1, termination_source="old"), exit_code=2, reason="killed", source="new")
add("mark_exited", session=sess("p1", exited=False, termination_source="old"), exit_code=3, reason="killed", source="")
add("mark_exited", session=sess("p1", exited=False, termination_source="old"), exit_code=None, reason="lost", source="backend_lost")
add("mark_exited", session=sess("p1", exited=False, termination_source=""), exit_code=0, reason="exited", source="")
add("mark_exited", session=sess("p1", exited=False), exit_code=-1, reason="exited", source="process.kill")

# ── detached_host_fate ──
add("detached_host_fate", state=state(), pid=123, expected_start=999, io={"host_pid_is_ours": True})
add("detached_host_fate", state=state(), pid=0, expected_start=None, io={"host_pid_is_ours": False})
add("detached_host_fate", state=state(), pid=123, expected_start=999, io={"host_pid_is_ours": False, "is_host_pid_alive": False})
add("detached_host_fate", state=state(), pid=123, expected_start=None, io={"host_pid_is_ours": False, "is_host_pid_alive": True})
add("detached_host_fate", state=state(), pid=123, expected_start=999, io={"host_pid_is_ours": False, "is_host_pid_alive": True, "safe_host_start_time": None})
add("detached_host_fate", state=state(), pid=123, expected_start=999, io={"host_pid_is_ours": False, "is_host_pid_alive": True, "safe_host_start_time": 999})
add("detached_host_fate", state=state(), pid=123, expected_start=999, io={"host_pid_is_ours": False, "is_host_pid_alive": True, "safe_host_start_time": 111})
add("detached_host_fate", state=state(), pid=None, expected_start=999, io={"host_pid_is_ours": False})

# ── close_reused_detached ──
add("close_reused_detached", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", now=500.0,
    io={"write_checkpoint": None, "completion_event_set": None})
add("close_reused_detached", state=state({"p1": sess("p1", exited=True)}, running=["p1"]), sid="p1", now=500.0, io={})
add("close_reused_detached", state=state({"p1": sess("p1", exited=False)}, finished=["p1"]), sid="p1", now=700.0,
    io={"write_checkpoint": None, "completion_event_set": None})
add("close_reused_detached", state=state({"p1": sess("p1", exited=False), "p2": sess("p2")}, running=["p2", "p1"], finished=["p2"]),
    sid="p1", now=900.0, io={"write_checkpoint": None, "completion_event_set": None})
add("close_reused_detached", state=state({"p1": sess("p1", exited=False, exited_at=10.0)}, running=["p1"]), sid="p1", now=0.0,
    io={"write_checkpoint": None, "completion_event_set": None})

# ── prune_uncollected_detached ──
add("prune_uncollected_detached", state=state({"p1": sess("p1", systemd_unit="hermes-worker-x.scope")}, running=["p1"]), sid="p1",
    io={"write_checkpoint": None, "completion_event_set": None})
add("prune_uncollected_detached", state=state({"p1": sess("p1", systemd_unit="")}, running=["p1"]), sid="p1",
    io={"write_checkpoint": None, "completion_event_set": None})
add("prune_uncollected_detached", state=state({"p1": sess("p1", systemd_unit="")}, running=["p1"], finished=["p1"]), sid="p1",
    io={"write_checkpoint": None, "completion_event_set": None})
add("prune_uncollected_detached", state=state({"p1": sess("p1", systemd_unit="u.scope"), "p2": sess("p2")}, running=["p1"], finished=["p2", "p1"]), sid="p1",
    io={"write_checkpoint": None, "completion_event_set": None})
add("prune_uncollected_detached", state=state({"p1": sess("p1", systemd_unit="")}, finished=["p1"]), sid="p1",
    io={"write_checkpoint": None, "completion_event_set": None})

# ── uncollected_gone ──
add("uncollected_gone", state=state(), sid=None, io={})
add("uncollected_gone", state=state({"p1": sess("p1", exited=False, detached=True, pid=5)}), sid="p1", io={})
add("uncollected_gone", state=state({"p1": sess("p1", exited=True, exit_code=0, detached=True, pid=5)}), sid="p1", io={})
add("uncollected_gone", state=state({"p1": sess("p1", exited=True, exit_code=None, detached=False, pid=5)}), sid="p1", io={})
add("uncollected_gone", state=state({"p1": sess("p1", exited=True, exit_code=None, detached=True, pid_scope="sandbox", pid=5)}), sid="p1", io={})
add("uncollected_gone", state=state({"p1": sess("p1", exited=True, exit_code=None, detached=True, pid_scope="host", pid=5)}), sid="p1", io={"is_host_pid_alive": False})
add("uncollected_gone", state=state({"p1": sess("p1", exited=True, exit_code=None, detached=True, pid_scope="host", pid=5)}), sid="p1", io={"is_host_pid_alive": True})

# ── ingest_output ──
add("ingest_output", state=state({"p1": sess("p1", exited=False, output_buffer="a", max_output_chars=50)}, running=["p1"]),
    sid="p1", text="b", unless_exited=False, io={"check_watch_patterns": None, "emit_output": None})
add("ingest_output", state=state({"p1": sess("p1", exited=False, output_buffer="a")}, running=["p1"]),
    sid="p1", text="b", unless_exited=True, io={"check_watch_patterns": None, "emit_output": None})
add("ingest_output", state=state({"p1": sess("p1", exited=True, output_buffer="a")}, finished=["p1"]),
    sid="p1", text="b", unless_exited=True, io={})
add("ingest_output", state=state({"p1": sess("p1", exited=True, output_buffer="a")}, finished=["p1"]),
    sid="p1", text="b", unless_exited=False, io={"check_watch_patterns": None, "emit_output": None})
add("ingest_output", state=state({"p1": sess("p1", exited=False, output_buffer="a")}, running=["p1"]),
    sid="p1", text="", unless_exited=True, io={"check_watch_patterns": None, "emit_output": None})

# ── finish_exited ──
add("finish_exited", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", exit_code=0, reason="exited", source="", io={"move_to_finished": True})
add("finish_exited", state=state({"p1": sess("p1", exited=True, exit_code=1)}, finished=["p1"]), sid="p1", exit_code=2, reason="killed", source="new", io={"move_to_finished": False})
add("finish_exited", state=state({"p1": sess("p1", exited=False, termination_source="old")}, running=["p1"]), sid="p1", exit_code=None, reason="lost", source="", io={"move_to_finished": True})
add("finish_exited", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", exit_code=5, reason="exited", source="process.kill", io={"move_to_finished": True})
add("finish_exited", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", exit_code=0, reason="exited", source="", io={"move_to_finished": False})

# ── finish_reader ──
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="Process",
    io={"decoder_flush": "x", "append": None, "wait": None, "exit_code": 0, "move_to_finished": True})
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="PTY",
    io={"decoder_flush": "", "wait": None, "exit_code": 1, "move_to_finished": True})
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="Process",
    io={"decoder_flush": rais("Exception", "decode"), "wait": None, "exit_code": 0, "move_to_finished": True})
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="Process",
    io={"decoder_flush": None, "wait": rais("OSError", "wait boom"), "exit_code": None})
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="PTY",
    io={"decoder_flush": None, "wait": rais("OSError", "wait boom"), "exit_code": 5, "move_to_finished": True})
add("finish_reader", state=state({"p1": sess("p1", exited=False)}, running=["p1"]), sid="p1", label="Process",
    io={"decoder_flush": "tail", "append": rais("ValueError", "append boom"), "wait": None, "exit_code": 0, "move_to_finished": True})

# ── release_finished_handles ──
add("release_finished_handles", state=state({"p1": sess("p1", process=proc())}), sid="p1", is_windows=False, current_thread_id="main",
    io={"stream_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", process=proc(stderr=False))}), sid="p1", is_windows=False, current_thread_id="main",
    io={"stream_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", process=proc())}), sid="p1", is_windows=False, current_thread_id="main",
    io={"stream_close": [rais("OSError", "epipe"), None, None]})
add("release_finished_handles", state=state({"p1": sess("p1", process=proc())}), sid="p1", is_windows=False, current_thread_id="main",
    io={"stream_close": rais("RuntimeError", "bad close")})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={})}), sid="p1", is_windows=False, current_thread_id="main",
    io={"pty_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={}, _reader_thread={"id": "r1", "name": "r1", "alive": True})}),
    sid="p1", is_windows=False, current_thread_id="main", io={})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={}, _reader_thread={"id": "main", "name": "main", "alive": True})}),
    sid="p1", is_windows=False, current_thread_id="main", io={"pty_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={}, _reader_thread={"id": "r1", "name": "r1", "alive": False})}),
    sid="p1", is_windows=False, current_thread_id="main", io={"pty_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={}, _reader_thread={"id": "r1", "name": "r1", "alive": True})}),
    sid="p1", is_windows=True, current_thread_id="main", io={"pty_close": None})
add("release_finished_handles", state=state({"p1": sess("p1", _pty={})}), sid="p1", is_windows=False, current_thread_id="main",
    io={"pty_close": rais("Exception", "pty close")})
add("release_finished_handles", state=state({"p1": sess("p1")}), sid="p1", is_windows=False, current_thread_id="main", io={})

# ── publish_finished ──
PF_IO = {"stream_close": None, "write_checkpoint": None, "producer_fields": {},
         "exit_fields": {"exit_code": 0, "completion_reason": "exited", "termination_source": ""},
         "completion_output": {"output": "done"}, "redact_result": None, "queue_put": None}
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, process=proc(), command="ls", session_key="sk", task_id="t", owner_task_id="o", started_at=10.0)}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main", io=dict(PF_IO))
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, process=proc())}, running=["p1"]),
    sid="p1", was_running=False, is_windows=False, current_thread_id="main",
    io={"stream_close": None, "write_checkpoint": None})
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=False, process=proc())}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main",
    io={"stream_close": None, "write_checkpoint": None})
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, handoff_note="handed up", process=proc())}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main", io=dict(PF_IO))
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, handoff_note="", process=proc())}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main", io=dict(PF_IO))
_pf_override = dict(PF_IO)
_pf_override["producer_fields"] = {"task_id": "override", "origin_ui_session_id": "ui1"}
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, process=proc(), task_id="t")}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main", io=_pf_override)
add("publish_finished", state=state({"p1": sess("p1", notify_on_complete=True, process=proc())}, running=["p1"]),
    sid="p1", was_running=True, is_windows=False, current_thread_id="main",
    io={"stream_close": rais("RuntimeError", "bad"), "write_checkpoint": None})

# ── transfer_ownership ──
add("transfer_ownership", state=state({"p1": sess("p1", owner_task_id="a", exited=False)}, running=["p1"]),
    session_id="p1", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="hi", io={"get": "p1"})
add("transfer_ownership", state=state({"p1": sess("p1", owner_task_id="a")}, running=["p1"]),
    session_id="p1", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="", io={"get": None})
add("transfer_ownership", state=state({"p1": sess("p1", owner_task_id="a", exited=True)}, finished=["p1"]),
    session_id="p1", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="", io={"get": "p1"})
add("transfer_ownership", state=state({"p1": sess("p1", owner_task_id="other", exited=False)}, running=["p1"]),
    session_id="p1", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="", io={"get": "p1"})
add("transfer_ownership", state=state({"p1": sess("p1", owner_task_id="a", exited=False)}, running=["p1"]),
    session_id="p1", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="", io={"get": "p1"})
add("transfer_ownership", state=state({"proc_ab": sess("proc_ab", owner_task_id="a", exited=False)}, running=["proc_ab"]),
    session_id="ab", from_owner="a", to_owner="b", to_task_id="t2", to_session_key="sk2", note="n", io={"get": "proc_ab"})

# ── unread_completions_owned_by ──
def fin(id, owner="o", notify=True, code=0):
    return sess(id, owner_task_id=owner, notify_on_complete=notify, exit_code=code, exited=True)
add("unread_completions_owned_by", state=state({"a": fin("a"), "b": fin("b", owner="x"), "c": fin("c", notify=False), "d": fin("d", code=None), "e": fin("e")},
    finished=["a", "b", "c", "d", "e"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a"), "e": fin("e")}, finished=["a", "e"], consumed=["a", "e"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a", code=None)}, finished=["a"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a", notify=False)}, finished=["a"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a", owner="x")}, finished=["a"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a"), "e": fin("e")}, finished=["a", "e"], polled=["e"]), owner_task_id="o")
add("unread_completions_owned_by", state=state({"a": fin("a"), "b": fin("b"), "c": fin("c")}, finished=["c", "a", "b"]), owner_task_id="o")

# ── track_started ──
TS_IO = {"copy_context": None, "thread_new": None, "prune_if_needed": None, "reader_start": None, "write_checkpoint": None}
add("track_started", state=state({"p1": sess("p1")}), sid="p1", reader_name="proc-reader-p1", io=dict(TS_IO))
add("track_started", state=state({"p1": sess("p1")}), sid="p1", reader_name="proc-reader-p1",
    io={"copy_context": None, "thread_new": None, "prune_if_needed": None, "reader_start": rais("RuntimeError", "can't start new thread")})
add("track_started", state=state({"p1": sess("p1")}), sid="p1", reader_name="proc-pty-reader-p1", io=dict(TS_IO))
add("track_started", state=state({"p1": sess("p1"), "p2": sess("p2")}, running=["p2", "p1"]), sid="p1", reader_name="proc-reader-p1", io=dict(TS_IO))
add("track_started", state=state({"p1": sess("p1")}), sid="p1", reader_name="proc-reader-p1",
    io={"copy_context": None, "thread_new": None, "prune_if_needed": None, "reader_start": None, "write_checkpoint": None})

# ── adopt_local ──
NEW_SESS = sess("proc_new1", command="sleep 5", task_id="t", owner_task_id="o", session_key="sk")
ADOPT_IO = {"new_session": NEW_SESS, "safe_host_start_time": 4242, "take_handoff_group": None,
            "copy_context": None, "thread_new": None, "prune_if_needed": None, "reader_start": None, "write_checkpoint": None}
add("adopt_local", state=state(), proc=proc(pid=55), command="sleep 5", cwd="/w", task_id="t", session_key="sk", owner_task_id="o",
    output_so_far="", notify_on_complete=True, io=dict(ADOPT_IO))
add("adopt_local", state=state(), proc=proc(pid=55), command="sleep 5", cwd="/w", task_id="t", session_key="sk", owner_task_id="o",
    output_so_far="seed output", notify_on_complete=True, io=dict(ADOPT_IO))
_adopt_fail = dict(ADOPT_IO)
_adopt_fail["reader_start"] = rais("RuntimeError", "can't start new thread")
_adopt_fail["return_handoff_group"] = None
add("adopt_local", state=state(), proc=proc(pid=55, pgid=77), command="sleep 5", cwd="/w", task_id="t", session_key="sk", owner_task_id="o",
    output_so_far="", notify_on_complete=True, io=_adopt_fail)
add("adopt_local", state=state(), proc=proc(pid=55), command="sleep 5", cwd=None, task_id="t", session_key="sk", owner_task_id="o",
    output_so_far="", notify_on_complete=False, io=dict(ADOPT_IO))
add("adopt_local", state=state(), proc=proc(pid=0), command="x", cwd="/w", task_id="", session_key="", owner_task_id="",
    output_so_far="abc", notify_on_complete=True, io=dict(ADOPT_IO))

# ── restore_completions ──
add("restore_completions", state=state(restored=False), io={"hosted_scope": None, "set_home_override": "tok", "restore_undelivered": 7, "reset_home_override": None})
add("restore_completions", state=state(restored=True), io={})
add("restore_completions", state=state(restored=False), io={"hosted_scope": "hosted"})
add("restore_completions", state=state(restored=False), io={"hosted_scope": ""})
add("restore_completions", state=state(restored=False), io={"hosted_scope": None, "set_home_override": "tok", "restore_undelivered": rais("RuntimeError", "db down"), "reset_home_override": None})
add("restore_completions", state=state(restored=False), io={"hosted_scope": None, "set_home_override": None, "restore_undelivered": 0, "reset_home_override": None})
add("restore_completions", state=state(restored=False), io={"hosted_scope": None, "set_home_override": "tok", "restore_undelivered": 3, "reset_home_override": None})

# ── signal_kill ──
add("signal_kill", state=state({"p1": sess("p1", _pty={}, pid=5)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"pty_terminate": None})
add("signal_kill", state=state({"p1": sess("p1", _pty={}, pid=5)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"pty_terminate": rais("Exception", "term"), "os_kill": None})
add("signal_kill", state=state({"p1": sess("p1", _pty={}, pid=0)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"pty_terminate": rais("Exception", "term")})
add("signal_kill", state=state({"p1": sess("p1", process=proc(pid=9), host_start_time=111)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"terminate_host_pid": None})
add("signal_kill", state=state({"p1": sess("p1", env_ref={}, pid=9)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"env_execute": None})
add("signal_kill", state=state({"p1": sess("p1", env_ref={}, pid=0)}, running=["p1"]), sid="p1", consume_output=False, now=1.0, io={})
add("signal_kill", state=state({"p1": sess("p1", detached=True, pid_scope="host", pid=9, host_start_time=111)}, running=["p1"]),
    sid="p1", consume_output=False, now=1.0, io={"host_pid_is_ours": True, "terminate_host_pid": None})
add("signal_kill", state=state({"p1": sess("p1", detached=True, pid_scope="host", pid=9, host_start_time=111, output_buffer="out")}, running=["p1"]),
    sid="p1", consume_output=True, now=800.0,
    io={"host_pid_is_ours": False, "is_host_pid_alive": False, "stop_systemd_unit": True, "completion_output": {"output": "out"},
        "write_checkpoint": None, "completion_event_set": None})
add("signal_kill", state=state({"p1": sess("p1", detached=True, pid_scope="host", pid=9, host_start_time=111, systemd_unit="u.scope")}, running=["p1"]),
    sid="p1", consume_output=False, now=800.0,
    io={"host_pid_is_ours": False, "is_host_pid_alive": True, "safe_host_start_time": 222, "stop_systemd_unit": False,
        "completion_output": {"output": ""}, "write_checkpoint": None, "completion_event_set": None})
add("signal_kill", state=state({"p1": sess("p1", pid=0)}, running=["p1"]), sid="p1", consume_output=False, now=1.0, io={})
add("signal_kill", state=state({"p1": sess("p1", env_ref={}, pid=9)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"env_execute": rais("RuntimeError", "sandbox down")})
add("signal_kill", state=state({"p1": sess("p1", _pty={}, pid=5)}, running=["p1"]), sid="p1", consume_output=False, now=1.0,
    io={"pty_terminate": rais("Exception", "term"), "os_kill": rais("OSError", "no such process")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "a flow has <5 cases"
    assert len(counts) == 18, "expected 18 flows, got %d" % len(counts)


if __name__ == "__main__":
    main()
