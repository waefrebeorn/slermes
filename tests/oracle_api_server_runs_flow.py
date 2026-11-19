#!/usr/bin/env python3
"""Oracle for the api_server_runs.py run-lifecycle FLOW cores (api_server_runs_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy gateway/aiohttp import), their module constants are
seeded (Assign + AnnAssign + the _RunStream/_RunLaunch dataclasses), and every
web.response/clock/redaction/room-grant/process-probe/store/logging dependency
is replaced by a recording monkeypatch stub returning the same scripted
results the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered
trace of seam calls) plus the return value / mutated state, not just a final
scalar.

Input: stdin JSON lines from t_port_api_server_runs_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE api_server_runs_flow: N cases, M mismatches. Exit 1 on any
mismatch; fail-closed FATAL exit 2 on extraction failure or zero cases.
"""
import ast
import copy
import json
import os
import sys
import types
from collections import deque
from contextlib import suppress
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Optional, Tuple

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server_runs.py")
SRC_GRANTS = os.path.join(QUARRY, "gateway", "platforms", "api_server_room_grants.py")

WANTED_FUNCS = {
    "terminal_run_status", "_run_user_message", "_tool_completed_preview",
    "_run_event", "_room_permission_for", "room_run_scope",
    "_remember_room_retention", "_run_idempotency_scope", "_check_run_auth",
    "_owner_alive", "_set_run_status", "_mark_shutdown_requested",
    "_forget_run", "_release_run_owner_if_forgotten", "_durable_run_status",
    "_resolve_conversation_history", "_accepted_response", "_replay_or_conflict",
    "_call_id",
}
WANTED_CLASSES = {"_RunStream", "_RunLaunch"}
GRANTS_FUNCS = {"_json_error", "_room_grant_error_response"}
GRANTS_CLASS = "RoomGrantReauthorizationRequired"

CUR = {"func": None, "in": {}, "trace": [], "clock_idx": 0}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _log(self, level, fmt, *args):
        rec({"ev": "log", "level": level, "msg": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw):
        self._log("debug", fmt, *args)

    def warning(self, fmt, *args, **kw):
        self._log("warning", fmt, *args)

    def info(self, fmt, *args, **kw):
        self._log("info", fmt, *args)

    def exception(self, fmt, *args, **kw):
        self._log("exception", fmt, *args)


def fatal(msg):
    print("FATAL %s" % msg)
    sys.exit(2)


def extract():
    src = open(SRC, encoding="utf-8").read()
    import hashlib
    import threading

    web_stub = types.SimpleNamespace(Response=object, Request=object, StreamResponse=object,
                                     json_response=lambda *a, **k: None)
    ns = {"__name__": "api_server_runs_extracted", "Any": Any, "Callable": Callable,
          "Dict": Dict, "List": List, "Optional": Optional, "Tuple": Tuple,
          "os": os, "json": json, "hashlib": hashlib, "threading": threading,
          "suppress": suppress, "deque": deque, "dataclass": dataclass,
          "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger()),
          "logger": RecLogger(), "web": web_stub,
          "asyncio": types.SimpleNamespace(Queue=object, AbstractEventLoop=object,
                                           CancelledError=type("CancelledError", (BaseException,), {})),
          "time": types.SimpleNamespace(time=lambda: 0.0, perf_counter=lambda: 0.0),
          "RequestKey": None, "_UnboundedThreadExecutor": object,
          "AuthorityRefusal": type("AuthorityRefusal", (Exception,), {}),
          "TERMINAL_STATUSES": frozenset({"completed", "failed", "cancelled", "interrupted"}),
          "functools": __import__("functools"), "contextvars": __import__("contextvars"),
          "uuid": types.SimpleNamespace(uuid4=lambda: None)}
    tree = ast.parse(src)

    def exec_node(node, namespace=None):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"),
             ns if namespace is None else namespace)

    # Module constants (plain Name targets only).
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec_node(node)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec_node(node)
            except Exception:
                pass
    # _RunStream first: the _RunLaunch dataclass annotates against it.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
            node.decorator_list = []
            exec_node(node)
    missing = (WANTED_FUNCS | WANTED_CLASSES) - set(ns)
    if missing:
        fatal("extraction missed: %s" % sorted(missing))

    # Live room-grants helpers (error envelope + 401/403 decision), extracted
    # from their own module so the auth fence runs the real error mapping.
    gsrc = open(SRC_GRANTS, encoding="utf-8").read()
    gtree = ast.parse(gsrc)
    ns2 = {"__name__": "room_grants_extracted", "web": web_stub,
           "Any": Any, "Optional": Optional}
    for node in gtree.body:
        if isinstance(node, ast.ClassDef) and node.name == GRANTS_CLASS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_GRANTS, "exec"), ns2)
    for node in gtree.body:
        if isinstance(node, ast.FunctionDef) and node.name in GRANTS_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_GRANTS, "exec"), ns2)
    gmissing = (GRANTS_FUNCS | {GRANTS_CLASS}) - set(ns2)
    if gmissing:
        fatal("grants extraction missed: %s" % sorted(gmissing))
    for name in GRANTS_FUNCS | {GRANTS_CLASS}:
        ns[name] = ns2[name]
    return ns


def openai_error_stub(message, **kwargs):
    rec({"ev": "openai_error", "message": message, "kwargs": kwargs})
    return {"message": message, **kwargs}


def json_response_stub(body, status=200, headers=None):
    rec({"ev": "json_response", "body": body, "status": status, "headers": headers})
    return {"status": status, "body": body, "headers": headers}


def clock_stub():
    script = cin("clock", 0.0)
    if isinstance(script, list):
        i = CUR["clock_idx"]
        CUR["clock_idx"] += 1
        r = script[min(i, len(script) - 1)] if script else 0.0
    else:
        r = script
    rec({"ev": "clock", "ret": r})
    return r


def redact_stub(value, force=False):
    r = cin("redact_ret", value)
    rec({"ev": "redact", "text": value, "ret": r})
    return r


class ProfileVar:
    def get(self):
        r = cin("profile")
        rec({"ev": "request_profile", "ret": r})
        return r


class ApiServerDouble:
    _openai_error = staticmethod(openai_error_stub)

    def __init__(self):
        self._api_request_profile = ProfileVar()


class RequestDouble:
    """web.Request double: path/method for the permission fence, items/attr
    for the retention __setitem__ mechanics (scripted AttributeError/TypeError)."""

    def __init__(self, inp):
        rj = inp.get("request") or {}
        self.path = rj.get("path", "")
        self.method = rj.get("method", "GET")
        self.headers = dict(rj.get("headers") or {})
        self.match_info = dict(rj.get("match_info") or {})
        rs = inp.get("request_state") or {}
        self.items = dict(rs.get("items") or {})

    def __setitem__(self, key, value):
        t = cin("request_set_raises")
        if t:
            rec({"ev": "request_set", "key": key, "value": value, "raised": t})
            # The live except clause catches the REAL AttributeError/TypeError
            # classes (isinstance, not name) — a same-named dynamic subclass
            # would sail past the fallback branch.
            cls = {"AttributeError": AttributeError, "TypeError": TypeError}.get(t)
            if cls is None:
                cls = type(t, (Exception,), {})
            raise cls(t)
        rec({"ev": "request_set", "key": key, "value": value})
        self.items[key] = value


def req_state_out(req):
    return {"items": req.items,
            "attr": getattr(req, "_hermes_room_run_retention_until", None)}


class StoreDouble:
    def update_status(self, run_id, status):
        evd = {"ev": "store_update_status", "run_id": run_id, "status": copy.deepcopy(status)}
        if cin("store_update_raises", False):
            t = cin("store_update_exc_type", "RuntimeError")
            m = cin("store_update_exc_msg", "store failed")
            evd["raised"] = t
            evd["msg"] = m
            rec(evd)
            raise type(t, (Exception,), {})(m)
        rec(evd)

    def status_for_run(self, scope, run_id, retention_until=None):
        r = cin("store_record")
        rec({"ev": "store_status_for_run", "scope": scope, "run_id": run_id,
             "retention_until": retention_until, "ret": r})
        return copy.deepcopy(r) if r is not None else None

    def extend_retention(self, scope, run_id, retention_until):
        rec({"ev": "store_extend_retention", "scope": scope, "run_id": run_id,
             "retention_until": retention_until})


class ResponseStoreDouble:
    def get(self, response_id):
        r = cin("stored_ret")
        rec({"ev": "response_store_get", "id": response_id, "ret": r})
        return copy.deepcopy(r) if r is not None else None


class SelfDouble:
    def _room_grant_token(self, request):
        r = cin("room_token")
        rec({"ev": "room_grant_token", "ret": r})
        return r

    def _expected_api_key(self):
        r = cin("api_key_ret")
        rec({"ev": "expected_api_key", "ret": r})
        return r

    def _check_auth(self, request):
        r = cin("check_auth_ret")
        rec({"ev": "check_auth", "ret": r})
        return r

    def _current_response_store(self):
        return ResponseStoreDouble()


def make_self(ns, inp):
    st = inp.get("state") or {}
    s = SelfDouble()
    for name in ("run_statuses", "run_streams", "run_streams_created", "run_owners",
                 "run_approval_sessions", "active_run_agents", "active_run_tasks"):
        setattr(s, "_" + name, copy.deepcopy(st.get(name) or {}))
    for name in ("run_idempotency_ids", "stopping_run_ids", "shutdown_interrupted_run_ids"):
        setattr(s, "_" + name, set(st.get(name) or []))
    s._run_shutdown_requested_at = st.get("run_shutdown_requested_at")
    s._run_idempotency_store = StoreDouble()
    s.gateway_runner = types.SimpleNamespace(session_authority=None)
    # The adapter binds these module functions with _api_server / self wired
    # in; the double mirrors that binding so intra-flow calls stay live.
    api = ApiServerDouble()
    s._run_idempotency_scope = lambda request: ns["_run_idempotency_scope"](
        s, request, _api_server=api)
    s._durable_run_status = lambda request, run_id: ns["_durable_run_status"](
        s, request, run_id)
    s._set_run_status = lambda run_id, status, **fields: ns["_set_run_status"](
        s, run_id, status, **fields)
    s._release_run_owner_if_forgotten = lambda run_id: ns["_release_run_owner_if_forgotten"](
        s, run_id)
    return s


def state_out(s):
    return {
        "run_statuses": s._run_statuses,
        "run_streams": s._run_streams,
        "run_streams_created": s._run_streams_created,
        "run_owners": s._run_owners,
        "run_approval_sessions": s._run_approval_sessions,
        "active_run_agents": s._active_run_agents,
        "active_run_tasks": s._active_run_tasks,
        "run_idempotency_ids": sorted(s._run_idempotency_ids),
        "stopping_run_ids": sorted(s._stopping_run_ids),
        "shutdown_interrupted_run_ids": sorted(s._shutdown_interrupted_run_ids),
        "run_shutdown_requested_at": s._run_shutdown_requested_at,
    }


def install_stubs(ns):
    ns["web"].json_response = json_response_stub
    ns["logger"] = RecLogger()
    ns["time"] = types.SimpleNamespace(time=clock_stub)

    def room_run_claims_stub(self, request, *, permission):
        if cin("claims_raises", False):
            t = cin("claims_exc_type", "RuntimeError")
            m = cin("claims_exc_msg", "grant invalid")
            rec({"ev": "room_run_claims", "permission": permission, "raised": t, "msg": m})
            cls = ns[GRANTS_CLASS] if t == GRANTS_CLASS else type(t, (Exception,), {})
            raise cls(m)
        r = cin("claims_ret")
        rec({"ev": "room_run_claims", "permission": permission, "ret": r})
        return copy.deepcopy(r) if r is not None else None
    ns["room_run_claims"] = room_run_claims_stub

    def retention_stub(request):
        r = cin("retention_until", 0.0)
        rec({"ev": "room_retention_until", "ret": r})
        return r
    ns["_room_retention_until"] = retention_stub

    # ── in-function import stubs (sys.modules, both package levels) ──
    def pkg(name):
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules.setdefault(name, m)
        return m

    def mod(name):
        m = types.ModuleType(name)
        sys.modules[name] = m
        return m

    pkg("gateway")
    pkg("gateway.platforms")

    def pid_exists_stub(pid):
        raised = cin("pid_exists_raises", False)
        r = cin("pid_exists_ret", False)
        evd = {"ev": "pid_exists", "pid": pid}
        if raised:
            evd["raised"] = True
            rec(evd)
            raise RuntimeError("pid probe failed")
        evd["ret"] = r
        rec(evd)
        return r

    def start_time_stub(pid):
        if cin("start_time_raises", False):
            rec({"ev": "process_start_time", "pid": pid, "raised": True})
            raise RuntimeError("start time unavailable")
        r = cin("start_time_ret")
        rec({"ev": "process_start_time", "pid": pid, "ret": r})
        return r

    def fingerprints_stub(recorded, current):
        raised = cin("fp_match_raises", False)
        r = cin("fp_match_ret", False)
        evd = {"ev": "fingerprints_match", "recorded": recorded, "current": current}
        if raised:
            evd["raised"] = True
            rec(evd)
            raise RuntimeError("fingerprint probe failed")
        evd["ret"] = r
        rec(evd)
        return r

    status_mod = mod("gateway.status")
    status_mod._pid_exists = pid_exists_stub
    status_mod.get_process_start_time = start_time_stub
    status_mod.start_time_fingerprints_match = fingerprints_stub

    def projection_stub(self, run_id):
        r = cin("projection_ret")
        rec({"ev": "run_projection", "run_id": run_id, "ret": r})
        return copy.deepcopy(r) if r is not None else None
    mod("gateway.platforms.api_server_authority_runs").run_projection = projection_stub


def _drive(ns, func, inp):
    if func == "terminal_run_status":
        status, fields = ns["terminal_run_status"](inp.get("result") or {})
        return {"ret": [status, fields]}
    if func == "run_user_message":
        msg, err = ns["_run_user_message"](inp.get("raw_input"), openai_error_stub)
        return {"ret": [msg, err]}
    if func == "call_id":
        return {"ret": ns["_call_id"](inp.get("kw") or {})}
    if func == "tool_completed_preview":
        return {"ret": ns["_tool_completed_preview"](inp.get("result"), redact_stub)}
    if func == "run_event":
        fields = dict(inp.get("fields") or {})
        return {"ret": ns["_run_event"](inp.get("run_id", ""), inp.get("name", ""), **fields)}
    if func == "room_permission_for":
        return {"ret": ns["_room_permission_for"](RequestDouble(inp))}
    if func == "room_run_scope":
        return {"ret": ns["room_run_scope"](inp.get("claims") or {})}
    if func == "remember_room_retention":
        req = RequestDouble(inp)
        ns["_remember_room_retention"](req, inp.get("claims") or {})
        return {"ret": None, "request_state": req_state_out(req)}
    if func == "run_idempotency_scope":
        s = make_self(ns, inp)
        req = RequestDouble(inp)
        r = ns["_run_idempotency_scope"](s, req, _api_server=ApiServerDouble())
        return {"ret": r, "request_state": req_state_out(req)}
    if func == "check_run_auth":
        s = make_self(ns, inp)
        req = RequestDouble(inp)
        r = ns["_check_run_auth"](s, req, permission=inp.get("permission", "status"),
                                  _api_server=ApiServerDouble())
        return {"ret": r}
    if func == "owner_alive":
        return {"ret": ns["_owner_alive"](inp.get("owner_pid", 0), inp.get("owner_started", 0))}
    if func == "set_run_status":
        s = make_self(ns, inp)
        fields = dict(inp.get("fields") or {})
        r = ns["_set_run_status"](s, inp.get("run_id", ""), inp.get("status", "running"), **fields)
        return {"ret": r, "state": state_out(s)}
    if func == "mark_shutdown_requested":
        s = make_self(ns, inp)
        r = ns["_mark_shutdown_requested"](s)
        return {"ret": r, "state": state_out(s)}
    if func == "forget_run":
        s = make_self(ns, inp)
        tables = [getattr(s, "_" + n) for n in (inp.get("tables") or [])]
        ns["_forget_run"](s, inp.get("run_id", ""), *tables)
        return {"ret": None, "state": state_out(s)}
    if func == "release_run_owner_if_forgotten":
        s = make_self(ns, inp)
        ns["_release_run_owner_if_forgotten"](s, inp.get("run_id", ""))
        return {"ret": None, "state": state_out(s)}
    if func == "durable_run_status":
        s = make_self(ns, inp)
        req = RequestDouble(inp)
        r = ns["_durable_run_status"](s, req, inp.get("run_id", ""))
        return {"ret": r, "state": state_out(s), "request_state": req_state_out(req)}
    if func == "resolve_conversation_history":
        s = make_self(ns, inp)
        hist, instructions, stored, err = ns["_resolve_conversation_history"](
            s, inp.get("body") or {}, inp.get("raw_input"), _openai_error=openai_error_stub)
        return {"ret": [hist, instructions, stored, err]}
    if func == "accepted_response":
        r = ns["_accepted_response"](inp.get("run_id", ""), inp.get("status", "queued"),
                                     inp.get("gateway_session_key"),
                                     replayed=inp.get("replayed", False))
        return {"ret": r}
    if func == "replay_or_conflict":
        s = make_self(ns, inp)
        req = RequestDouble(inp)
        r = ns["_replay_or_conflict"](s, req, inp.get("outcome", "reused"),
                                      inp.get("record") or {}, inp.get("gateway_session_key"),
                                      openai_error_stub)
        return {"ret": r, "state": state_out(s), "request_state": req_state_out(req)}
    raise ValueError("unknown func: %s" % func)


def drive(ns, func, inp):
    try:
        return _drive(ns, func, inp)
    except Exception as e:
        # Same convention as the C err_out paths: "<ExcType>: <str(exc)>".
        return {"ret": None, "err": "%s: %s" % (type(e).__name__, e)}


def canon(o):
    """JSON-round-trip normalization: live tuples compare as lists, integral
    floats as ints — representation, not logic."""
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    if isinstance(o, float) and o.is_integer():
        return int(o)
    return o


def main():
    try:
        ns = extract()
        install_stubs(ns)
    except SystemExit:
        raise
    except BaseException as e:
        fatal("extraction/stub install failed: %r" % e)
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["clock_idx"] = 0
        try:
            want = drive(ns, func, inp)
        except BaseException as e:
            # Fail closed on ANY live escape (SystemExit included): a case the
            # live code cannot answer is a mismatch, never a silent pass.
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        want = canon(want)
        if want != got:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE api_server_runs_flow: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        fatal("zero cases compared")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())