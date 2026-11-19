#!/usr/bin/env python3
"""Oracle for the hermes_cli/gateway_session_store.py FLOW + leaf ports
(src/cli/port_gw_session_store_flow.c).

Drives the LIVE GatewaySessionStore methods and module leaves with SCRIPTED
seams: the class is AST-extracted whole from the quarry, module constants are
seeded (Assign + AnnAssign), the real GatewayClientError / RESUME_REFUSALS /
SessionState / SessionWindow / TranscriptView are extracted from their defining
quarry modules, and every live I/O (gateway RPCs, the reader probe, uuid minting,
the retry-state siblings, stderr warnings, debug logs) is replaced by recording
stubs returning the SAME scripts the C harness feeds its universal seam.
Compares the ordered seam-call trace + return (or raise spec) + final store
state mirror.

Input: stdin JSON lines from t_port_gw_session_store_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], "st": {...}}}
Summary: ORACLE gw_session_store_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.

Modeling notes (identical on the C side):
- store.call / _retry_state / _leave / SessionState.adopt_snapshot run REAL
  (no seam event); their state effects are compared in the final mirror.
- connection.state(sid) is a structural get-or-create seam recording one event
  whose ret is the 12-field state projection BEFORE adopt effects.
- `current is snapshot` identity: fixture st.current = {"ref": sid} binds the
  SAME object as snapshots[sid]; an inline current is a distinct object.
- State projection fields: session_id, info, title, todo_state, revision,
  subscription_id, execution_generation, messages, pending, pending_seq, gaps,
  attached_gaps. Other SessionState fields are not observable in the ported set.
- TranscriptView members project to .value; SessionWindow to {limit, offset}.
- __getattr__ resolves the callable then probe-calls it with in.probe
  {args, kwargs}; the outcome is {kind, probe_ret, probe_raise}.
- KeyError raise specs carry the RAW key; str(KeyError(k)) == repr(k).
- Integral floats normalize to int on both sides; dicts compare unordered,
  lists ordered.
"""
import ast
import copy
import json
import os
import sys
import types
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_session_store.py")
CLIENT = os.path.join(QUARRY, "hermes_cli", "gateway_client.py")
CONN = os.path.join(QUARRY, "hermes_cli", "gateway_connection.py")
SESSIONS = os.path.join(QUARRY, "persistence", "conversation", "sessions.py")
MESSAGES = os.path.join(QUARRY, "persistence", "conversation", "messages.py")

import __future__
ANN_FLAG = __future__.annotations.compiler_flag

STATE_FIELDS = ["session_id", "info", "title", "todo_state", "revision",
                "subscription_id", "execution_generation", "messages",
                "pending", "pending_seq", "gaps", "attached_gaps"]

WANTED_FUNCS = {"session_label", "_classic_only_policy", "_transcript_view",
                "_window", "_noop", "_zero"}

CUR = {"in": {}, "trace": [], "cursors": {}}
_MISSING = object()


def rec(ev):
    CUR["trace"].append(ev)


def build_exc(spec):
    """A scripted raise needs a REAL exception instance (BaseException trio
    included, so except-Exception sites propagate them correctly)."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    builtins = {"ValueError": ValueError, "TypeError": TypeError,
                "KeyError": KeyError, "AttributeError": AttributeError,
                "RuntimeError": RuntimeError, "Exception": Exception,
                "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
                "GeneratorExit": GeneratorExit}
    if kind in builtins:
        cls = builtins[kind]
    elif kind == "GatewayClientError":
        cls = NS["GatewayClientError"]
    else:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        return cls(msg)  # str(KeyError(k)) == repr(k)
    return cls(msg)


def rec_seam(name, args):
    """Universal scripted seam: queue/constant/__script__ semantics, records
    {ev, args, ret} then raises a scripted spec or returns a deep copy."""
    io = CUR["in"].get("io") or {}
    script = io.get(name, _MISSING)
    if script is _MISSING:
        result = {"__raise__": "RuntimeError", "message": "no script for " + name}
    elif isinstance(script, list):
        cur = CUR["cursors"].get(name, 0)
        if cur >= len(script):
            result = {"__raise__": "RuntimeError",
                      "message": "script exhausted for " + name}
        else:
            result = script[cur]
            CUR["cursors"][name] = cur + 1
    elif isinstance(script, dict) and script.get("__script__"):
        result = script.get("ret")
    else:
        result = script
    rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(result)})
    if isinstance(result, dict) and "__raise__" in result:
        raise build_exc(result)
    return copy.deepcopy(result)


class RecLogger:
    def debug(self, fmt, *a, **k):
        rec({"ev": "logger.debug",
             "args": {"fmt": fmt,
                      "args": [str(x) if isinstance(x, BaseException) else x for x in a]},
             "ret": None})

    def warning(self, fmt, *a, **k):
        rec({"ev": "logger.warning",
             "args": {"fmt": fmt,
                      "args": [str(x) if isinstance(x, BaseException) else x for x in a]},
             "ret": None})


def state_proj(s):
    return {k: copy.deepcopy(getattr(s, k)) for k in STATE_FIELDS}


class ReaderDouble:
    """The read-only SurfaceLabelReader stand-in: any read method records."""
    def __init__(self, marker):
        self.marker = marker

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)

        def method(*a, **k):
            return rec_seam("reader." + name, {"args": list(a), "kwargs": k})
        return method


class HandlesDouble:
    def __init__(self, marker):
        self.marker = marker

    def close(self):
        return rec_seam("handles.close", {})


class SessionsDouble:
    def is_empty_candidate(self, session_id):
        return rec_seam("persistence.sessions.is_empty_candidate",
                        {"session_id": session_id})


class PersistenceDouble:
    """The storage-interface reader stand-in; only the empty-session probe is
    reached directly (the title/revision readers are seamed siblings)."""
    def __init__(self, marker):
        self.marker = marker
        self.sessions = SessionsDouble()


class ConnDouble:
    """GatewayConnection stand-in: scripted RPC seams + real SessionState
    registry. call keeps the live kwonly signature so duplicate method/abandon
    kwargs raise the real TypeError at binding (before any trace event)."""
    def __init__(self, spec, states):
        self.final = spec.get("final", False)
        self.launch_api_key = spec.get("launch_api_key")
        self.launch_api_key_typed = spec.get("launch_api_key_typed", False)
        self.states = states

    def call(self, method, *, timeout=_MISSING, abandon=_MISSING, **params):
        args = {"method": method}
        if timeout is not _MISSING:
            args["timeout"] = timeout
        if abandon is not _MISSING:
            args["abandon"] = abandon
        args.update(params)
        return rec_seam("connection.call", args)

    def resume(self, call, session_id, *, typed=False):
        return rec_seam("connection.resume",
                        {"call": "store.call", "session_id": session_id, "typed": typed})

    def start(self, timeout=90):
        return rec_seam("connection.start", {})

    def reattach(self, session_id, abandon=None):
        return rec_seam("connection.reattach", {"session_id": session_id})

    def state(self, session_id):
        s = self.states.get(session_id)
        if s is None:
            s = self.states[session_id] = NS["SessionState"](session_id)
        rec({"ev": "connection.state", "args": {"session_id": session_id},
             "ret": state_proj(s)})
        return s


def _reg_mod(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def _extract_class(path, classname, ns):
    src = open(path, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == classname:
            if node.decorator_list:
                lines = src.splitlines()
                seg = "\n".join(lines[node.decorator_list[0].lineno - 1:node.end_lineno])
            else:
                seg = ast.get_source_segment(src, node)
            exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
            return
    raise RuntimeError("class %s not found in %s" % (classname, path))


def _extract_assign(path, name, ns):
    src = open(path, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            return
    raise RuntimeError("assign %s not found in %s" % (name, path))


NS = {}


def extract():
    global NS
    modname = "gw_session_store_extracted"
    mod = _reg_mod(modname)
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["__builtins__"] = __builtins__
    import re as _re
    import argparse as _argparse
    ns["re"] = _re
    ns["argparse"] = _argparse
    ns["json"] = json
    ns["os"] = types.SimpleNamespace(getenv=lambda *a: None, getcwd=lambda: "")
    ns["sys"] = types.SimpleNamespace(stderr="stderr", modules={})
    ns["logging"] = types.SimpleNamespace(getLogger=lambda name: RecLogger())
    ns["logger"] = RecLogger()
    ns["uuid"] = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=rec_seam("uuid4", {})))
    ns["print"] = lambda *a, **k: rec(
        {"ev": "print", "args": {"text": a[0] if a else "", "file": "stderr"},
         "ret": None})
    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["Optional"] = Optional
    ns["Enum"] = Enum

    # Real exceptions / constants from their defining quarry modules.
    _extract_class(CLIENT, "GatewayClientError", ns)
    _extract_assign(CONN, "RESUME_REFUSALS", ns)
    _extract_class(CONN, "SessionState", ns)

    class GatewayConnection:
        @classmethod
        def shared(cls):
            raise RuntimeError("GatewayConnection.shared() called; pass a connection double")
    ns["GatewayConnection"] = GatewayConnection

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Module constants (Assign + AnnAssign, plain-Name targets) in source order.
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # Module leaves + the whole store class (methods stay real).
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=ANN_FLAG, dont_inherit=True), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "GatewaySessionStore":
            seg = ast.get_source_segment(src, node)
            exec(compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)

    # Real SessionWindow / TranscriptView for the in-function imports.
    sess_ns = {"__name__": "gwss_sessions_extracted", "__builtins__": __builtins__,
               "dataclass": dataclass, "field": field, "Optional": Optional}
    _reg_mod("gwss_sessions_extracted").__dict__.update(sess_ns)
    sess_mod = sys.modules["gwss_sessions_extracted"]
    _extract_class(SESSIONS, "SessionWindow", sess_mod.__dict__)
    msg_ns = {"__name__": "gwss_messages_extracted", "__builtins__": __builtins__,
              "Enum": Enum}
    _reg_mod("gwss_messages_extracted").__dict__.update(msg_ns)
    msg_mod = sys.modules["gwss_messages_extracted"]
    _extract_class(MESSAGES, "TranscriptView", msg_mod.__dict__)

    # sys.modules stubs for the in-function imports (packages + leaves).
    for pkg in ("persistence", "persistence.conversation"):
        if pkg not in sys.modules:
            _reg_mod(pkg)
    sessions_stub = types.ModuleType("persistence.conversation.sessions")
    sessions_stub.SessionWindow = sess_mod.__dict__["SessionWindow"]
    sessions_stub.sanitize_title = lambda *a, **k: rec_seam(
        "sanitize_title", {"args": list(a), "kwargs": k})
    sessions_stub.session_yolo_enabled = lambda *a, **k: rec_seam(
        "session_yolo_enabled", {"args": list(a), "kwargs": k})
    sys.modules["persistence.conversation.sessions"] = sessions_stub
    sys.modules["persistence.conversation"].sessions = sessions_stub
    messages_stub = types.ModuleType("persistence.conversation.messages")
    messages_stub.TranscriptView = msg_mod.__dict__["TranscriptView"]
    sys.modules["persistence.conversation.messages"] = messages_stub
    sys.modules["persistence.conversation"].messages = messages_stub

    for nm in ("_READS", "_GATEWAY_OWNED", "_GENERATION_FENCED", "_COUNTED_REPAIRS",
               "_CANONICAL_LOCAL_ID", "GatewayClientError", "RESUME_REFUSALS",
               "SessionState", "GatewaySessionStore", "session_label",
               "_classic_only_policy", "_transcript_view", "_window", "_noop", "_zero"):
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    NS = ns
    return ns


def setup_case(inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["cursors"] = {}
    stspec = inp.get("st") or {}
    cspec = stspec.get("connection") or {}
    states = {}
    for sid, sspec in (cspec.get("states") or {}).items():
        s = NS["SessionState"](sid)
        for k in STATE_FIELDS:
            if isinstance(sspec, dict) and k in sspec:
                setattr(s, k, copy.deepcopy(sspec[k]))
        states[sid] = s
    conn = ConnDouble(cspec, states)
    store = NS["GatewaySessionStore"](connection=conn)
    store.snapshots = copy.deepcopy(stspec.get("snapshots") or {})
    cur = stspec.get("current")
    if isinstance(cur, dict) and set(cur) == {"ref"}:
        store.current = store.snapshots[cur["ref"]]  # SAME object (identity)
    else:
        store.current = copy.deepcopy(cur)
    retry = stspec.get("retry")
    store.retry = (copy.deepcopy(retry[0]), copy.deepcopy(retry[1])) if retry is not None else None
    store._reader = stspec.get("reader")
    p = stspec.get("persistence")
    store.persistence = PersistenceDouble(p) if p is not None else None
    h = stspec.get("handles")
    store._handles = HandlesDouble(h) if h is not None else None
    store._create_parameters = copy.deepcopy(stspec.get("create_parameters"))

    # Seamed siblings (instance overrides; live calls self.<name>(...)).
    def _db():
        r = rec_seam("_db", {})
        return None if r is None else ReaderDouble(r)
    store._db = _db
    store._create_params = lambda launch, title, record_cwd: rec_seam(
        "_create_params", {"launch": launch, "title": title, "record_cwd": record_cwd})
    store._row_revision = lambda session_id, default: rec_seam(
        "_row_revision", {"session_id": session_id, "default": default})
    store._title_holder = lambda session_id, title: rec_seam(
        "_title_holder", {"session_id": session_id, "title": title})
    # _check_resume_launch is a MODULE global in open_session.
    NS["_check_resume_launch"] = lambda launch, snapshot: rec_seam(
        "_check_resume_launch", {"launch": launch, "snapshot": snapshot})
    return store


def dump_st(store):
    conn = store.connection
    return {
        "snapshots": store.snapshots,
        "current": store.current,
        "retry": list(store.retry) if store.retry is not None else None,
        "connection": {
            "final": conn.final,
            "launch_api_key": conn.launch_api_key,
            "launch_api_key_typed": conn.launch_api_key_typed,
            "states": {sid: state_proj(s) for sid, s in conn.states.items()},
        },
        "reader": store._reader,
        "handles": store._handles.marker if store._handles is not None else None,
        "persistence": store.persistence.marker if store.persistence is not None else None,
        "create_parameters": store._create_parameters,
    }


def drive_getattr(store, inp):
    name = inp.get("name")
    probe = inp.get("probe") or {}
    args = probe.get("args") or []
    kwargs = probe.get("kwargs") or {}
    try:
        fn = store.__getattr__(name)
    except AttributeError as e:
        return {"kind": "attr_error", "probe_ret": None,
                "probe_raise": {"__raise__": "AttributeError", "message": str(e)}}
    if fn is NS["_noop"]:
        kind = "noop"
    elif fn is NS["_zero"]:
        kind = "zero"
    elif name == "sanitize_title":
        kind = "sanitize_title"
    elif name == "session_yolo_enabled":
        kind = "yolo"
    elif name in NS["_READS"]:
        kind = "read"
    else:
        kind = "refused"
    try:
        pr = fn(*args, **kwargs)
        return {"kind": kind, "probe_ret": pr, "probe_raise": None}
    except BaseException as e:
        return {"kind": kind, "probe_ret": None,
                "probe_raise": {"__raise__": type(e).__name__, "message": str(e)}}


def drive(inp):
    func = CUR["func"]
    store = setup_case(inp)
    try:
        if func == "session_label":
            ret = NS["session_label"](inp.get("session_id"))
        elif func == "_classic_only_policy":
            ret = NS["_classic_only_policy"](inp.get("launch"))
        elif func == "_transcript_view":
            ret = NS["_transcript_view"](inp.get("include_inactive"),
                                         inp.get("include_compacted")).value
        elif func == "_window":
            w = NS["_window"](inp.get("limit"), inp.get("offset"))
            ret = {"limit": w.limit, "offset": w.offset}
        elif func == "set_session_yolo":
            ret = store.set_session_yolo(inp.get("session_id"), inp.get("enabled"))
        elif func == "close_reader":
            ret = store.close_reader()
        elif func == "__getattr__":
            ret = drive_getattr(store, inp)
        elif func == "_call":
            ret = store._call(inp.get("method"), inp.get("abandon"),
                              **(inp.get("params", {})))
        elif func == "_adopt":
            ret = store._adopt(inp.get("snapshot"))
        elif func == "attach":
            ret = store.attach(inp.get("session_id"), typed=inp.get("typed", False))
        elif func == "open_session":
            cli = types.SimpleNamespace(**(inp.get("cli") or {}))
            ret = store.open_session(cli, inp.get("launch"))
        elif func == "create":
            ret = store.create(inp.get("launch"), inp.get("title"),
                               record_cwd=inp.get("record_cwd", True))
        elif func == "branch":
            ret = store.branch(inp.get("session_id"), inp.get("title"))
        elif func == "_mutate":
            ret = store._mutate(inp.get("session_id"), inp.get("operation"),
                                inp.get("payload"), inp.get("snapshot"))
        elif func == "set_session_title":
            ret = store.set_session_title(inp.get("session_id"), inp.get("title"))
        elif func == "delete_session":
            ret = store.delete_session(inp.get("session_id"))
        elif func == "end_session":
            ret = store.end_session(inp.get("session_id"), inp.get("reason"))
        elif func == "delete_session_if_empty":
            ret = store.delete_session_if_empty(inp.get("session_id"))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": ret, "trace": CUR["trace"], "st": dump_st(store)}


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        f = float(x)
        if f == int(f) and abs(f) < 1e15:
            return ("num", float(int(f)))
        return ("num", f)
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        try:
            want = drive(inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gw_session_store_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())