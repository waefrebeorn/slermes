#!/usr/bin/env python3
"""Oracle for the background-process lifecycle FLOW cores
(src/tools/process_registry_flow.c).

Drives the LIVE tools/process_registry.py methods with SCRIPTED seams: the
targets are AST-extracted from the quarry (no heavy import), module constants are
seeded, and every host-PID probe / checkpoint write / scope stop / signal / env
exec / decoder flush / thread start / queue put / config-scope read / redaction /
log dependency is replaced by a recording stub returning the same scripted
results the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered {ev,args,ret} trace) plus the return value, propagated exception and
mutated registry/session state.

Input: stdin JSON lines from t_port_process_registry_flow:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace","state"|"session"}}
Summary: ORACLE process_registry_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
from contextlib import suppress
from dataclasses import dataclass, field
from typing import Any, Dict, List, Literal, NamedTuple, Optional

for _stream in (sys.stdout, sys.stdin):
    try:
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
ANN_FLAG = __future__.annotations.compiler_flag

CUR = {"func": None, "in": {}, "trace": [], "counts": {}, "threads": {},
       "reg": None, "session": None}
LIVE = {}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def mk_exc(kind, msg):
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, or array consumed per call."""
    io = CUR["in"].get("io", {}) or {}
    script = io.get(name)
    if isinstance(script, list):
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        if not script:
            return None
        return script[min(i, len(script) - 1)]
    return script


def seam_ret(name, script):
    if name == "new_session":
        return (script or {}).get("id", "") if isinstance(script, dict) else ""
    return script


def record(name, args, script):
    rec({"ev": name, "args": args, "ret": seam_ret(name, script)})


def maybe_raise(script):
    if is_raise(script):
        raise mk_exc(script["__raise__"], script.get("msg", ""))


# ── AST extraction ──
def extract_consts(rel, ns):
    p = os.path.join(QUARRY, rel)
    src = open(p, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        tg = []
        if isinstance(node, ast.Assign):
            tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            tg = [node.target.id]
        if tg:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
            except Exception:
                pass
    return tree, src, p


def extract_methods(tree, src, p, class_name, wanted, ns):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == class_name:
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in wanted:
                    mm = copy.copy(m)
                    mm.decorator_list = []
                    exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


REG_TARGETS = [
    "_detached_host_fate", "_close_reused_detached", "_prune_uncollected_detached",
    "_uncollected_gone", "_ingest_output", "_finish_exited", "_finish_reader",
    "_release_finished_handles", "_publish_finished", "transfer_ownership",
    "unread_completions_owned_by", "_track_started", "adopt_local",
    "restore_completions", "_signal_kill", "_stop_scope",
]
SESS_TARGETS = ["_append_locked", "append_output_if_running", "mark_exited", "append_output"]


def build_ns():
    mod = types.ModuleType("process_registry_extracted")
    sys.modules["process_registry_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "process_registry_extracted", "json": json, "os": os,
               "dataclass": dataclass, "field": field, "suppress": suppress,
               "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
               "NamedTuple": NamedTuple, "Optional": Optional})
    import platform
    import signal
    import threading
    import time
    ns["platform"] = platform
    ns["signal"] = signal
    ns["threading"] = threading
    ns["time"] = time
    tree, src, p = extract_consts("tools/process_registry.py", ns)
    extract_methods(tree, src, p, "ProcessRegistry", set(REG_TARGETS), ns)
    extract_methods(tree, src, p, "ProcessSession", set(SESS_TARGETS), ns)
    for nm in REG_TARGETS + SESS_TARGETS:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
        LIVE[nm] = ns[nm]
    return ns


def fake_module(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            sys.modules[pn] = types.ModuleType(pn)
    m = sys.modules[name]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


# ── doubles ──
class CM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class LogDouble:
    def _log(self, level, fmt, *a, **k):
        msg = fmt % a if a else fmt
        record("log", {"level": level, "msg": msg}, None)

    def debug(self, fmt, *a, **k):
        self._log("debug", fmt, *a, **k)

    def info(self, fmt, *a, **k):
        self._log("info", fmt, *a, **k)

    def warning(self, fmt, *a, **k):
        self._log("warning", fmt, *a, **k)

    def error(self, fmt, *a, **k):
        self._log("error", fmt, *a, **k)


class EventDouble:
    def __init__(self, sid, set_=False):
        self.sid = sid
        self._set = set_

    def is_set(self):
        return self._set

    def set(self):
        self._set = True
        record("completion_event_set", {"session_id": self.sid}, consume("completion_event_set"))


def thread_double(id_, name=None, alive=False):
    t = CUR["threads"].get(id_)
    if t is None:
        t = ThreadDouble(id_, name, alive)
        CUR["threads"][id_] = t
    return t


class ThreadDouble:
    def __init__(self, id_, name=None, alive=False, daemon=False):
        self.id = id_
        self.name = name or id_
        self._alive = alive
        self.daemon = daemon

    def is_alive(self):
        return self._alive

    def start(self):
        sc = consume("reader_start")
        record("reader_start", {"name": self.name}, sc)
        maybe_raise(sc)
        self._alive = True


class ThreadFactory:
    def __call__(self, target=None, args=(), daemon=False, name=None, **kw):
        sc = consume("thread_new")
        record("thread_new", drop_none({"name": name, "daemon": daemon}), sc)
        maybe_raise(sc)
        return thread_double(name, name, False)


class StreamDouble:
    def __init__(self, sid, which):
        self.sid = sid
        self.which = which

    def close(self):
        sc = consume("stream_close")
        record("stream_close", {"session_id": self.sid, "stream": self.which}, sc)
        maybe_raise(sc)


class ProcDouble:
    def __init__(self, sid, d):
        self.pid = d.get("pid")
        self.stdout = StreamDouble(sid, "stdout") if d.get("stdout") is not None else None
        self.stderr = StreamDouble(sid, "stderr") if d.get("stderr") is not None else None
        self.stdin = StreamDouble(sid, "stdin") if d.get("stdin") is not None else None
        self._hermes_owned_pgid = d.get("_hermes_owned_pgid")


class PtyDouble:
    def __init__(self, sid):
        self.sid = sid

    def close(self):
        sc = consume("pty_close")
        record("pty_close", {"session_id": self.sid}, sc)
        maybe_raise(sc)

    def terminate(self, force=False):
        sc = consume("pty_terminate")
        record("pty_terminate", {"session_id": self.sid, "force": force}, sc)
        maybe_raise(sc)


class EnvDouble:
    def __init__(self, sid):
        self.sid = sid

    def execute(self, command, cwd=None, timeout=None, **kw):
        sc = consume("env_execute")
        record("env_execute", drop_none({"command": command, "cwd": cwd, "timeout": timeout}), sc)
        maybe_raise(sc)
        return sc if not is_raise(sc) else None


class QueueDouble:
    def __init__(self):
        self.items = []

    def put(self, notification):
        record("queue_put", {"notification": notification, "key_order": list(notification.keys())},
               consume("queue_put"))
        self.items.append(notification)


DEFAULTS = {
    "id": "", "command": "", "task_id": "", "owner_task_id": "", "session_key": "",
    "sandbox_key": "", "cwd": None, "pid": None, "pid_scope": "host",
    "host_start_time": None, "started_at": 0.0, "exited": False, "exited_at": 0.0,
    "exit_code": None, "completion_reason": "exited", "termination_source": "",
    "output_buffer": "", "max_output_chars": 200000, "total_output_chars": 0,
    "detached": False, "systemd_unit": "", "output_log": "", "notify_on_complete": False,
    "handoff_note": "", "persist_on_release": False, "completion_output_chars": 0,
    "_finish_claimed": False, "_scope_stop_pending": False,
}
SCALAR_FIELDS = list(DEFAULTS)


def mk_session(d):
    s = types.SimpleNamespace()
    for k in SCALAR_FIELDS:
        setattr(s, k, d.get(k, DEFAULTS[k]))
    sid = s.id
    s._lock = CM()
    s._completion_event = EventDouble(sid, d.get("_completion_event_set", False))
    s._reader_finish_requested = EventDouble(sid, d.get("_reader_finish_requested", False))
    rt = d.get("_reader_thread")
    s._reader_thread = thread_double(rt["id"], rt.get("name"), rt.get("alive", False)) if rt else None
    p = d.get("process")
    s.process = ProcDouble(sid, p) if p is not None else None
    s._pty = PtyDouble(sid) if d.get("_pty") is not None else None
    s.env_ref = EnvDouble(sid) if d.get("env_ref") is not None else None
    s._append_locked = types.MethodType(LIVE["_append_locked"], s)
    s.append_output_if_running = types.MethodType(LIVE["append_output_if_running"], s)
    s.mark_exited = types.MethodType(LIVE["mark_exited"], s)
    s.append_output = types.MethodType(LIVE["append_output"], s)
    return s


def proc_state(p):
    if p is None:
        return None
    return {"pid": p.pid,
            "stdout": {} if p.stdout is not None else None,
            "stderr": {} if p.stderr is not None else None,
            "stdin": {} if p.stdin is not None else None,
            "_hermes_owned_pgid": p._hermes_owned_pgid}


def sess_state(s):
    d = {k: getattr(s, k) for k in SCALAR_FIELDS}
    d["_completion_event_set"] = s._completion_event.is_set()
    d["_reader_finish_requested"] = s._reader_finish_requested.is_set()
    rt = s._reader_thread
    d["_reader_thread"] = {"id": rt.id, "name": rt.name, "alive": rt.is_alive()} if rt is not None else None
    d["process"] = proc_state(s.process)
    d["_pty"] = {} if s._pty is not None else None
    d["env_ref"] = {} if s.env_ref is not None else None
    return d


def install_reg_seams(reg):
    def host_pid_is_ours(pid, expected_start):
        sc = consume("host_pid_is_ours")
        record("host_pid_is_ours", drop_none({"pid": pid, "expected_start": expected_start}), sc)
        maybe_raise(sc)
        return bool(sc)

    def is_host_pid_alive(pid):
        sc = consume("is_host_pid_alive")
        record("is_host_pid_alive", drop_none({"pid": pid}), sc)
        maybe_raise(sc)
        return bool(sc)

    def safe_host_start_time(pid):
        sc = consume("safe_host_start_time")
        record("safe_host_start_time", drop_none({"pid": pid}), sc)
        maybe_raise(sc)
        return sc

    def write_checkpoint():
        record("write_checkpoint", {}, consume("write_checkpoint"))

    def move_to_finished(session):
        sc = consume("move_to_finished")
        record("move_to_finished", {"session_id": session.id}, sc)
        maybe_raise(sc)
        return bool(sc)

    def check_watch_patterns(session, text):
        record("check_watch_patterns", {"session_id": session.id, "text": text}, consume("check_watch_patterns"))

    def emit_output(session, text):
        record("emit_output", {"session_id": session.id, "text": text}, consume("emit_output"))

    def prune_if_needed():
        record("prune_if_needed", {}, consume("prune_if_needed"))

    def get(session_id):
        sc = consume("get")
        record("get", {"session_id": session_id}, sc)
        maybe_raise(sc)
        return reg.sessions.get(sc) if sc else None

    def new_session(command, task_id, owner_task_id, session_key, cwd):
        sc = consume("new_session")
        record("new_session", drop_none({"command": command, "task_id": task_id,
                                         "owner_task_id": owner_task_id, "session_key": session_key,
                                         "cwd": cwd}), sc)
        maybe_raise(sc)
        d = sc if isinstance(sc, dict) else {}
        s = mk_session(d)
        reg.sessions[s.id] = s
        return s

    def take_handoff_group(session, proc):
        record("take_handoff_group", drop_none({"session_id": session.id, "proc": proc_state(proc)}),
               consume("take_handoff_group"))

    def return_handoff_group(session, proc, owned_pgid):
        record("return_handoff_group", drop_none({"session_id": session.id, "proc": proc_state(proc),
                                                  "handed_pgid": owned_pgid}), consume("return_handoff_group"))

    def terminate_host_pid(pid, expected_start=None):
        record("terminate_host_pid", drop_none({"pid": pid, "expected_start": expected_start}),
               consume("terminate_host_pid"))

    def exit_fields(session):
        sc = consume("exit_fields")
        record("exit_fields", {"session_id": session.id}, sc)
        maybe_raise(sc)
        return sc if isinstance(sc, dict) else {}

    reg._host_pid_is_ours = host_pid_is_ours
    reg._is_host_pid_alive = is_host_pid_alive
    reg._safe_host_start_time = safe_host_start_time
    reg._write_checkpoint = write_checkpoint
    reg._move_to_finished = move_to_finished
    reg._check_watch_patterns = check_watch_patterns
    reg._emit_output = emit_output
    reg._prune_if_needed = prune_if_needed
    reg.get = get
    reg._new_session = new_session
    reg._take_handoff_group = take_handoff_group
    reg._return_handoff_group = return_handoff_group
    reg._terminate_host_pid = terminate_host_pid
    reg._exit_fields = exit_fields
    reg._reader_loop = lambda *a, **k: None


def mk_registry(state):
    reg = types.SimpleNamespace()
    reg._lock = CM()
    reg.sessions = {}
    for sid, sd in (state.get("sessions", {}) or {}).items():
        reg.sessions[sid] = mk_session(sd)
    reg._running = {}
    for sid in state.get("running", []):
        reg._running[sid] = reg.sessions[sid]
    reg._finished = {}
    for sid in state.get("finished", []):
        reg._finished[sid] = reg.sessions[sid]
    reg._completion_consumed = set(state.get("completion_consumed", []))
    reg._poll_observed = set(state.get("poll_observed", []))
    reg.completion_queue = QueueDouble()
    reg.completion_queue.items = list(state.get("completion_queue", []))
    reg._completions_restored = state.get("completions_restored", False)
    for nm in REG_TARGETS:
        if nm == "_stop_scope":   # live @staticmethod: bind plain (no self)
            setattr(reg, nm, LIVE[nm])
        else:
            setattr(reg, nm, types.MethodType(LIVE[nm], reg))
    install_reg_seams(reg)
    return reg


def reg_state(reg):
    return {
        "sessions": {sid: sess_state(s) for sid, s in reg.sessions.items()},
        "running": list(reg._running.keys()),
        "finished": list(reg._finished.keys()),
        "completion_consumed": sorted(reg._completion_consumed),
        "poll_observed": sorted(reg._poll_observed),
        "completion_queue": list(reg.completion_queue.items),
        "completions_restored": reg._completions_restored,
    }


def install_stubs(ns):
    ns["logger"] = LogDouble()
    ns["time"] = types.SimpleNamespace(time=lambda: cin("now", 0.0),
                                       monotonic=lambda: cin("monotonic", 0.0),
                                       sleep=lambda s: None)
    ns["threading"] = types.SimpleNamespace(Thread=ThreadFactory(),
                                            current_thread=lambda: thread_double(cin("current_thread_id", "main")),
                                            Lock=CM, Event=EventDouble)
    ns["os"] = types.SimpleNamespace(kill=_os_kill, environ=dict(os.environ),
                                     getenv=os.getenv, getuid=getattr(os, "getuid", lambda: 0))

    def producer_fields(sandbox_key):
        sc = consume("producer_fields")
        record("producer_fields", {"sandbox_key": sandbox_key}, sc)
        maybe_raise(sc)
        return sc if isinstance(sc, dict) else {}

    def completion_output(session):
        sc = consume("completion_output")
        record("completion_output", {"session_id": session.id}, sc)
        maybe_raise(sc)
        return sc if isinstance(sc, dict) else {}

    def redact_result(notification):
        record("redact_result", {"notification": notification, "key_order": list(notification.keys())},
               consume("redact_result"))
        return notification

    def stop_systemd_unit(unit):
        sc = consume("stop_systemd_unit")
        record("stop_systemd_unit", {"unit": unit}, sc)
        maybe_raise(sc)
        return bool(sc)

    def hosted_scope():
        sc = consume("hosted_scope")
        record("hosted_scope", {}, sc)
        maybe_raise(sc)
        return sc

    ns["producer_fields"] = producer_fields
    ns["_completion_output"] = completion_output
    ns["_redact_process_result"] = redact_result
    ns["_stop_systemd_unit"] = stop_systemd_unit
    ns["hosted_scope"] = hosted_scope

    def copy_context():
        record("copy_context", {}, consume("copy_context"))
        return types.SimpleNamespace(run=lambda f, *a: f(*a))

    def set_home_override(value):
        sc = consume("set_home_override")
        record("set_home_override", drop_none({"value": value}), sc)
        maybe_raise(sc)
        return sc

    def reset_home_override(token):
        record("reset_home_override", drop_none({"token": token}), consume("reset_home_override"))

    def restore_undelivered(queue):
        sc = consume("restore_undelivered")
        record("restore_undelivered", {}, sc)
        maybe_raise(sc)
        return sc

    fake_module("contextvars", copy_context=copy_context)
    fake_module("hermes_constants", set_hermes_home_override=set_home_override,
                reset_hermes_home_override=reset_home_override)
    fake_module("tools.async_delegation", restore_undelivered_completions=restore_undelivered)
    fake_module("tools.environments.sandbox_scope", hosted_scope=hosted_scope,
                producer_fields=producer_fields)


def _os_kill(pid, sig):
    sc = consume("os_kill")
    record("os_kill", drop_none({"pid": pid, "sig": sig}), sc)
    maybe_raise(sc)


def sess_ret(s):
    return s.id if s is not None else None


def is_session_func(func):
    return func in ("append_locked", "append_output_if_running", "mark_exited")


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    ns["_IS_WINDOWS"] = inp.get("is_windows", False)
    if is_session_func(func):
        s = mk_session(inp.get("session", {}) or {})
        CUR["session"] = s
        text = inp.get("text", "")
        if func == "append_locked":
            s._append_locked(text)
            return {"ret": None, "session": sess_state(s)}
        if func == "append_output_if_running":
            r = s.append_output_if_running(text)
            return {"ret": r, "session": sess_state(s)}
        r = s.mark_exited(inp.get("exit_code"), inp.get("reason", "exited"), inp.get("source", ""))
        return {"ret": r, "session": sess_state(s)}

    reg = mk_registry(inp.get("state", {}) or {})
    CUR["reg"] = reg
    sid = inp.get("sid")
    now = inp.get("now", 0.0)
    if func == "detached_host_fate":
        r = reg._detached_host_fate(inp.get("pid"), inp.get("expected_start"))
        return {"ret": r, "state": reg_state(reg)}
    if func == "close_reused_detached":
        reg._close_reused_detached(reg.sessions[sid])
        return {"ret": None, "state": reg_state(reg)}
    if func == "prune_uncollected_detached":
        r = reg._prune_uncollected_detached(reg.sessions[sid])
        return {"ret": sess_ret(r), "state": reg_state(reg)}
    if func == "uncollected_gone":
        r = reg._uncollected_gone(reg.sessions.get(sid) if sid else None)
        return {"ret": r, "state": reg_state(reg)}
    if func == "ingest_output":
        reg._ingest_output(reg.sessions[sid], inp.get("text", ""), unless_exited=inp.get("unless_exited", False))
        return {"ret": None, "state": reg_state(reg)}
    if func == "finish_exited":
        r = reg._finish_exited(reg.sessions[sid], inp.get("exit_code"),
                               inp.get("reason", "exited"), inp.get("source", ""))
        return {"ret": r, "state": reg_state(reg)}
    if func == "finish_reader":
        decoder = types.SimpleNamespace(decode=_decoder_decode)
        reg._finish_reader(reg.sessions[sid], decoder, _append_cb, inp.get("label", "Process"),
                           _wait_cb, _exit_code_cb)
        return {"ret": None, "state": reg_state(reg)}
    if func == "release_finished_handles":
        reg._release_finished_handles(reg.sessions[sid])
        return {"ret": None, "state": reg_state(reg)}
    if func == "publish_finished":
        reg._publish_finished(reg.sessions[sid], inp.get("was_running", False))
        return {"ret": None, "state": reg_state(reg)}
    if func == "transfer_ownership":
        r = reg.transfer_ownership(inp.get("session_id", ""), from_owner=inp.get("from_owner", ""),
                                   to_owner=inp.get("to_owner", ""), to_task_id=inp.get("to_task_id", ""),
                                   to_session_key=inp.get("to_session_key", ""), note=inp.get("note", ""))
        return {"ret": sess_ret(r), "state": reg_state(reg)}
    if func == "unread_completions_owned_by":
        r = reg.unread_completions_owned_by(inp.get("owner_task_id", ""))
        return {"ret": [s.id for s in r], "state": reg_state(reg)}
    if func == "track_started":
        reg._track_started(reg.sessions[sid], reg._reader_loop, inp.get("reader_name", "proc-reader"))
        return {"ret": None, "state": reg_state(reg)}
    if func == "adopt_local":
        proc = ProcDouble("adopt", inp.get("proc", {}) or {})
        r = reg.adopt_local(proc, command=inp.get("command", ""), cwd=inp.get("cwd"),
                            task_id=inp.get("task_id", ""), session_key=inp.get("session_key", ""),
                            owner_task_id=inp.get("owner_task_id", ""), output_so_far=inp.get("output_so_far", ""),
                            notify_on_complete=inp.get("notify_on_complete", True))
        return {"ret": sess_ret(r), "state": reg_state(reg)}
    if func == "restore_completions":
        r = reg.restore_completions()
        return {"ret": r, "state": reg_state(reg)}
    if func == "signal_kill":
        r = reg._signal_kill(reg.sessions[sid], inp.get("consume_output", False))
        return {"ret": r, "state": reg_state(reg)}
    raise ValueError("unknown func: %s" % func)


# _finish_reader callback seams (decoder / append / wait / exit_code)
def _decoder_decode(b, final=False):
    sc = consume("decoder_flush")
    record("decoder_flush", {}, sc)
    maybe_raise(sc)
    return sc


def _append_cb(text):
    sc = consume("append")
    record("append", {"session_id": CUR["in"].get("sid"), "text": text}, sc)
    maybe_raise(sc)


def _wait_cb():
    sc = consume("wait")
    record("wait", {"session_id": CUR["in"].get("sid")}, sc)
    maybe_raise(sc)


def _exit_code_cb():
    sc = consume("exit_code")
    record("exit_code", {"session_id": CUR["in"].get("sid")}, sc)
    maybe_raise(sc)
    return sc


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["threads"] = {}
    CUR["reg"] = None
    CUR["session"] = None
    try:
        out = drive(ns, func, inp)
        out["exc"] = None
    except BaseException as e:
        out = {"ret": None, "exc": {"__raise__": type(e).__name__, "msg": str(e)}}
        if is_session_func(func):
            out["session"] = sess_state(CUR["session"]) if CUR["session"] is not None else None
        else:
            out["state"] = reg_state(CUR["reg"]) if CUR["reg"] is not None else None
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def compare(want, got):
    if canon(want.get("trace", [])) != canon(got.get("trace", [])):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        if canon(want.get(k)) != canon(got.get(k)):
            return False
    return True


def main():
    ns = build_ns()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record_in = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record_in.get("func")
        inp = record_in.get("in") or {}
        got = record_in.get("out") or {}
        cases += 1
        try:
            want = run_case(ns, func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE process_registry_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
