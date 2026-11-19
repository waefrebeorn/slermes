#!/usr/bin/env python3
"""Oracle for the api_server.py request-handling FLOW cores (api_server_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy gateway/aiohttp import), their module constants are
seeded, and every web.response/secret/profile/clock/loop/logging dependency is
replaced by a recording monkeypatch stub returning the same scripted results
the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered trace
of seam calls) plus the return value / mutated state, not just a final scalar.

Input: stdin JSON lines from t_port_api_server_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE api_server_flow: N cases, M mismatches. Exit 1 on any
mismatch; fail-closed FATAL exit 2 on extraction failure or zero cases.
"""
import ast
import asyncio as asyncio_real
import json
import os
import sys
import types
from collections import OrderedDict
from contextlib import nullcontext
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server.py")

WANTED_FUNCS = {
    "_cap_text", "_cap_list", "_coerce_request_bool", "_request_relay_metadata",
    "_normalize_image_part", "_chat_usage_payload", "_responses_usage_payload",
    "_make_request_fingerprint", "_names_launch_profile", "_derive_chat_session_id",
    "_request_turn_author", "_error_response", "_invalid_request",
    "_approval_request_event", "_require_auth",
}
# Live siblings the targets call (extracted real, never stubbed to constants).
HELPER_FUNCS = {"_openai_error", "_approval_event_choices"}
WANTED_METHODS = {
    "_expected_api_key", "_check_auth", "_cron_request_guard",
    "_validate_cron_prompt", "_parse_session_key_header", "_sse_headers",
    "_register_session_stream_approval",
}
WANTED_STATIC = {
    "_auth_failed_response", "_session_db_unavailable", "_cron_error_response",
    "_session_headers", "_result_runtime", "_model_lock_state",
}
WANTED_CLASSMETHOD = {"_requested_ids"}
WANTED_CLASSES = {"_SessionEventQueue", "_IdempotencyCache"}

CUR = {"func": None, "in": {}, "trace": [], "clock_idx": 0, "clean_idx": 0, "flush_idx": 0}


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


def fatal(msg):
    print("FATAL %s" % msg)
    sys.exit(2)


def extract():
    src = open(SRC, encoding="utf-8").read()
    from typing import Any, Dict, List, Optional, Tuple
    import hashlib
    import re
    import threading
    import itertools
    from contextlib import suppress, contextmanager
    from functools import wraps

    web_stub = types.SimpleNamespace(Response=object, Request=object, StreamResponse=object,
                                     json_response=lambda *a, **k: None)
    ns = {"__name__": "api_server_extracted", "Any": Any, "Dict": Dict, "List": List,
          "Optional": Optional, "Tuple": Tuple, "Path": Path, "os": os, "re": re,
          "hashlib": hashlib, "json": json, "threading": threading, "itertools": itertools,
          "suppress": suppress, "contextmanager": contextmanager, "wraps": wraps,
          "logger": RecLogger(), "web": web_stub,
          "asyncio": types.SimpleNamespace(Queue=object, Lock=object),
          "time": types.SimpleNamespace(time=lambda: 0.0)}
    tree = ast.parse(src)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    # Module constants (plain Name targets only; subscript assigns would
    # mutate the real process environment).
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
    # Queue/cache classes first: adapter method annotations reference them.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (WANTED_FUNCS | HELPER_FUNCS):
            exec_node(node)
    # APIServerAdapter methods: strip decorators, bind onto a double class.
    # Class-level constants (_JOB_ID_RE, _MAX_PROMPT_LENGTH, ...) seed into a
    # namespace carrying re/typing so the regex constant compiles.
    adapter_consts = {"re": re, "Any": Any, "Optional": Optional}
    const_seed = set(adapter_consts)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "APIServerAdapter":
            for sub in node.body:
                if isinstance(sub, (ast.Assign, ast.AnnAssign)):
                    try:
                        exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"),
                             adapter_consts)
                    except Exception:
                        pass
                if isinstance(sub, ast.FunctionDef) and \
                        sub.name in (WANTED_METHODS | WANTED_STATIC | WANTED_CLASSMETHOD):
                    sub.decorator_list = []
                    exec_node(sub)
    missing = (WANTED_FUNCS | HELPER_FUNCS | WANTED_METHODS | WANTED_STATIC
               | WANTED_CLASSMETHOD) - set(ns)
    if missing:
        fatal("extraction missed functions: %s" % sorted(missing))
    for cls in WANTED_CLASSES:
        if cls not in ns:
            fatal("extraction missed class %s" % cls)

    class AdapterDouble:
        pass
    for k, v in adapter_consts.items():
        if k not in const_seed and not k.startswith("__"):
            setattr(AdapterDouble, k, v)
    for n in WANTED_METHODS:
        setattr(AdapterDouble, n, ns[n])
    for n in WANTED_STATIC:
        setattr(AdapterDouble, n, staticmethod(ns[n]))
    for n in WANTED_CLASSMETHOD:
        setattr(AdapterDouble, n, classmethod(ns[n]))
    ns["AdapterDouble"] = AdapterDouble
    return ns


class LoopDouble:
    """asyncio loop double: identity is the token; call_soon_threadsafe runs
    the callback synchronously (single-threaded scripted replay)."""

    def __init__(self, token, case_loops):
        self.token = token

    def call_soon_threadsafe(self, cb):
        raised = cin("cs_raises", False)
        rec({"ev": "call_soon_threadsafe", "raised": raised})
        if raised:
            raise RuntimeError("loop closed")
        cb()


class QueueDouble:
    def __init__(self, items):
        self.items = items

    def put_nowait(self, event):
        rec({"ev": "put_nowait", "event": event})
        self.items.append(event)


class ProfileVar:
    def get(self):
        r = cin("profile")
        rec({"ev": "request_profile", "ret": r})
        return r


def install_stubs(ns):
    AdapterDouble = ns["AdapterDouble"]

    def json_response_stub(body, status=200, headers=None):
        rec({"ev": "json_response", "body": body, "status": status, "headers": headers})
        return {"status": status, "body": body, "headers": headers}
    ns["web"].json_response = json_response_stub

    ns["logger"] = RecLogger()

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
    ns["time"] = types.SimpleNamespace(time=clock_stub)

    def secrets_match_stub(a, b):
        r = cin("secrets_match_ret", False)
        rec({"ev": "secrets_match", "a": a, "b": b, "ret": r})
        return r
    ns["secrets_match"] = secrets_match_stub

    def redact_stub(value, limit=None):
        text = str(value)
        r = cin("redact_ret", text)
        rec({"ev": "redact", "text": text, "ret": r})
        return r
    ns["_redact_api_error_text"] = redact_stub

    ns["_api_request_profile"] = ProfileVar()

    def audit_suffix_stub(self, request):
        r = cin("audit_suffix", "source='unknown'")
        rec({"ev": "audit_suffix", "ret": r})
        return r
    AdapterDouble._request_audit_log_suffix = audit_suffix_stub

    def draining_stub(self):
        r = cin("draining_ret")
        rec({"ev": "draining_response", "ret": r})
        return r
    AdapterDouble._draining_response = draining_stub

    def cors_stub(self, origin):
        r = cin("cors_ret")
        rec({"ev": "cors_headers", "origin": origin, "ret": r})
        return r
    AdapterDouble._cors_headers_for_origin = cors_stub

    def clean_stub(value, max_len=200):
        rets = cin("clean_rets", [])
        i = CUR["clean_idx"]
        CUR["clean_idx"] += 1
        r = rets[min(i, len(rets) - 1)] if rets else ""
        rec({"ev": "clean_runtime_id", "value": value, "max_len": max_len, "ret": r})
        return r
    AdapterDouble._clean_runtime_id = staticmethod(clean_stub)

    real_choices = ns["_approval_event_choices"]

    def choices_stub(*, smart_denied, allow_session, allow_permanent):
        r = real_choices(smart_denied=smart_denied, allow_session=allow_session,
                         allow_permanent=allow_permanent)
        rec({"ev": "approval_choices", "smart_denied": smart_denied,
             "allow_session": allow_session, "allow_permanent": allow_permanent, "ret": r})
        return r
    ns["_approval_event_choices"] = choices_stub

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

    pkg("gateway"); pkg("gateway.platforms"); pkg("hermes_cli"); pkg("agent")

    def get_secret_stub(name, default=""):
        if cin("secret_raises", False):
            t = cin("secret_exc_type", "RuntimeError")
            rec({"ev": "get_secret", "name": name, "default": default, "raised": t})
            # type(exc).__name__ must equal the scripted name (the live
            # fail-closed warning logs it).
            raise type(t, (Exception,), {})(t)
        r = cin("secret_ret")
        rec({"ev": "get_secret", "name": name, "default": default, "ret": r})
        return r
    mod("agent.secret_scope").get_secret = get_secret_stub

    def usable_stub(value, min_length=16):
        if cin("usable_raises", False):
            t = cin("usable_exc_type", "RuntimeError")
            rec({"ev": "has_usable_secret", "value": value, "min_length": min_length, "raised": t})
            raise type(t, (Exception,), {})(t)
        r = cin("usable_ret", False)
        rec({"ev": "has_usable_secret", "value": value, "min_length": min_length, "ret": r})
        return r
    auth_mod = mod("hermes_cli.auth")
    auth_mod.has_usable_secret = usable_stub

    def routing_home_stub():
        if cin("routing_home_raises", False):
            rec({"ev": "routing_home", "raised": True})
            raise RuntimeError("home unavailable")
        r = cin("routing_home", "")
        rec({"ev": "routing_home", "ret": r})
        return r
    mod("hermes_constants").get_routing_process_hermes_home = routing_home_stub

    def matches_stub(profile, home=None):
        if cin("matches_raises", False):
            rec({"ev": "profile_matches_home", "profile": profile, "home": home, "raised": True})
            raise RuntimeError("match failed")
        r = cin("matches_ret", False)
        rec({"ev": "profile_matches_home", "profile": profile, "home": home, "ret": r})
        return r
    mod("hermes_cli.profiles").profile_matches_home = matches_stub

    def turn_author_stub(raw):
        if cin("turn_author_raises", False):
            msg = cin("turn_author_exc", "author must be an object")
            rec({"ev": "parse_turn_author", "raw": raw, "raised": msg})
            raise ValueError(msg)
        r = cin("turn_author_ret")
        rec({"ev": "parse_turn_author", "raw": raw, "ret": r})
        return r
    mod("agent.turn_author").parse_turn_author = turn_author_stub

    def redact_approval_stub(text):
        mapping = cin("approval_redactions", {})
        r = mapping.get(text, text)
        rec({"ev": "redact_approval_command", "text": text, "ret": r})
        return r
    mod("gateway.run")._redact_approval_command = redact_approval_stub

    def run_event_stub(run_id, kind, **fields):
        r = cin("run_event_ret", {})
        rec({"ev": "run_event", "run_id": run_id, "kind": kind, "fields": fields, "ret": r})
        return dict(r)
    mod("gateway.platforms.api_server_runs")._run_event = run_event_stub


def scripted_check_auth(self, request):
    r = cin("check_auth_ret")
    rec({"ev": "check_auth", "ret": r})
    return r


def req_double(inp):
    rj = inp.get("request") or {}
    return types.SimpleNamespace(headers=dict(rj.get("headers") or {}),
                                 match_info=dict(rj.get("match_info") or {}))


def make_queue(ns, inp):
    q = object.__new__(ns["_SessionEventQueue"])
    st = inp.get("qstate") or {}
    loops = {}

    def loop_double(token):
        if token not in loops:
            loops[token] = LoopDouble(token, loops)
        return loops[token]

    q.session_id = st.get("session_id", "")
    q.run_id = st.get("run_id", "")
    q.seq = st.get("seq", 0)
    q._inflight = st.get("inflight", 0)
    q.loop = loop_double(st.get("loop", "loopA"))
    q.queue = QueueDouble(list(st.get("queue") or []))
    q._inflight_lock = nullcontext()

    def grl_stub():
        if cin("running_loop_raises", False):
            rec({"ev": "get_running_loop", "raised": True})
            raise RuntimeError("no running event loop")
        tok = cin("running_loop")
        rec({"ev": "get_running_loop", "ret": tok})
        return loop_double(tok) if tok is not None else None
    ns["asyncio"].get_running_loop = grl_stub
    return q, st


def queue_state(q, st):
    out = dict(st)
    out["seq"] = q.seq
    out["inflight"] = q._inflight
    out["queue"] = q.queue.items
    return out


def drive(ns, func, inp):
    AdapterDouble = ns["AdapterDouble"]
    real_check_auth = AdapterDouble._check_auth

    if func == "cap_text":
        return {"ret": ns["_cap_text"](inp.get("text", ""))}
    if func == "cap_list":
        return {"ret": ns["_cap_list"](inp.get("items") or [])}
    if func == "coerce_request_bool":
        return {"ret": ns["_coerce_request_bool"](inp.get("value"), inp.get("default", False))}
    if func == "request_relay_metadata":
        return {"ret": ns["_request_relay_metadata"](inp.get("body"))}
    if func == "normalize_image_part":
        try:
            return {"ret": ns["_normalize_image_part"](inp.get("part") or {}), "err": None}
        except ValueError as e:
            return {"ret": None, "err": str(e)}
    if func == "chat_usage_payload":
        return {"ret": ns["_chat_usage_payload"](inp.get("usage") or {})}
    if func == "responses_usage_payload":
        return {"ret": ns["_responses_usage_payload"](inp.get("usage") or {})}
    if func == "make_request_fingerprint":
        return {"ret": ns["_make_request_fingerprint"](inp.get("body") or {}, inp.get("keys") or [])}
    if func == "session_headers":
        return {"ret": AdapterDouble._session_headers(inp.get("session_id", ""),
                                                      inp.get("gateway_session_key"))}
    if func == "result_runtime":
        return {"ret": AdapterDouble._result_runtime(inp.get("result"), inp.get("usage"))}
    if func == "model_lock_state":
        return {"ret": AdapterDouble._model_lock_state(inp.get("runtime_request") or {},
                                                       inp.get("runtime"))}
    if func == "requested_ids":
        return {"ret": list(AdapterDouble._requested_ids(inp.get("requested")))}
    if func == "names_launch_profile":
        return {"ret": ns["_names_launch_profile"](inp.get("profile_name", ""))}
    if func == "derive_chat_session_id":
        return {"ret": ns["_derive_chat_session_id"](inp.get("system_prompt"),
                                                     inp.get("first_user_message", ""),
                                                     inp.get("profile"))}
    if func == "request_turn_author":
        try:
            return {"ret": ns["_request_turn_author"](inp.get("body") or {}), "err": None}
        except ValueError as e:
            return {"ret": None, "err": str(e)}
    if func == "error_response":
        r = ns["_error_response"](inp.get("message", ""), inp.get("status", 400),
                                  err_type=inp.get("err_type", "invalid_request_error"),
                                  param=inp.get("param"), code=inp.get("code"),
                                  headers=inp.get("headers"))
        return {"ret": r}
    if func == "invalid_request":
        return {"ret": ns["_invalid_request"](inp.get("message", ""))}
    if func == "auth_failed_response":
        return {"ret": AdapterDouble._auth_failed_response()}
    if func == "session_db_unavailable":
        return {"ret": AdapterDouble._session_db_unavailable()}
    if func == "cron_error_response":
        return {"ret": AdapterDouble._cron_error_response(Exception(inp.get("exc_text", "")))}
    if func == "expected_api_key":
        AdapterDouble._check_auth = real_check_auth
        self_d = types.SimpleNamespace(_api_key=inp.get("api_key", ""))
        return {"ret": AdapterDouble._expected_api_key(self_d)}
    if func == "check_auth":
        AdapterDouble._check_auth = real_check_auth
        self_d = types.SimpleNamespace(_api_key=inp.get("api_key", ""))
        return {"ret": AdapterDouble._check_auth(self_d, req_double(inp))}
    if func == "require_auth":
        AdapterDouble._check_auth = scripted_check_auth

        async def handler(self, request):
            r = cin("handler_ret")
            rec({"ev": "handler", "ret": r})
            return r
        wrapped = ns["_require_auth"](handler)
        self_d = types.SimpleNamespace()
        ret = asyncio_real.run(wrapped(self_d, req_double(inp)))
        AdapterDouble._check_auth = real_check_auth
        return {"ret": ret}
    if func == "cron_request_guard":
        AdapterDouble._check_auth = scripted_check_auth
        ns["_CRON_AVAILABLE"] = inp.get("cron_available", True)
        self_d = types.SimpleNamespace()
        job_id, err = AdapterDouble._cron_request_guard(
            self_d, req_double(inp), need_job_id=inp.get("need_job_id", False),
            check_draining=inp.get("check_draining", False))
        AdapterDouble._check_auth = real_check_auth
        return {"ret": err, "job_id": job_id}
    if func == "validate_cron_prompt":
        if inp.get("scan_available", True):
            def scan_stub(prompt):
                r = cin("scan_ret")
                rec({"ev": "scan_cron_prompt", "prompt": prompt, "ret": r})
                return r
            ns["_scan_cron_prompt"] = scan_stub
        else:
            ns["_scan_cron_prompt"] = None
        self_d = types.SimpleNamespace()
        return {"ret": AdapterDouble._validate_cron_prompt(self_d, inp.get("prompt", ""))}
    if func == "parse_session_key_header":
        self_d = types.SimpleNamespace(_api_key=inp.get("api_key", ""))
        key, err = AdapterDouble._parse_session_key_header(self_d, req_double(inp))
        return {"ret": err, "key": key}
    if func == "sse_headers":
        self_d = types.SimpleNamespace()
        return {"ret": AdapterDouble._sse_headers(self_d, req_double(inp), inp.get("extra"))}
    if func == "approval_request_event":
        fields = dict(inp.get("fields") or {})
        return {"ret": ns["_approval_request_event"](inp.get("run_id", ""),
                                                     inp.get("approval_data"), **fields)}
    if func == "register_session_stream_approval":
        self_d = types.SimpleNamespace(_run_approval_sessions={})
        AdapterDouble._register_session_stream_approval(
            self_d, inp.get("run_id", ""), None, inp.get("message_id", ""))
        return {"ret": None, "state": {"run_approval_sessions": self_d._run_approval_sessions}}
    if func == "session_stream_approval_notify":
        def set_status(run_id, status, *, last_event=None, approval=None):
            rec({"ev": "set_run_status", "run_id": run_id, "status": status,
                 "last_event": last_event, "approval": approval})

        class EventsDouble:
            def enqueue(self, name, payload):
                rec({"ev": "enqueue", "name": name, "payload": payload})
        self_d = types.SimpleNamespace(_run_approval_sessions={}, _set_run_status=set_status)
        notify = AdapterDouble._register_session_stream_approval(
            self_d, inp.get("run_id", ""), EventsDouble(), inp.get("message_id", ""))
        notify(inp.get("approval_data") or {})
        return {"ret": None}
    if func in ("queue_payload", "queue_enqueue", "queue_settle_one", "queue_flush"):
        q, st = make_queue(ns, inp)
        if func == "queue_payload":
            payload = dict(inp.get("payload") or {})
            name, payload = q.payload(inp.get("name", ""), payload)
            return {"ret": [name, payload], "state": queue_state(q, st)}
        if func == "queue_enqueue":
            payload = dict(inp.get("payload") or {})
            q.enqueue(inp.get("name", ""), payload)
            return {"ret": None, "state": queue_state(q, st)}
        if func == "queue_settle_one":
            q._settle_one()
            return {"ret": None, "state": queue_state(q, st)}

        async def sleep_stub(delay):
            rec({"ev": "sleep_zero"})
            script = cin("flush_inflight", [])
            i = CUR["flush_idx"]
            CUR["flush_idx"] += 1
            q._inflight = script[min(i, len(script) - 1)] if script else 0
        ns["asyncio"].sleep = sleep_stub
        asyncio_real.run(q.flush())
        return {"ret": None, "state": queue_state(q, st)}
    if func == "idem_purge":
        c = object.__new__(ns["_IdempotencyCache"])
        c._store = OrderedDict(inp.get("store") or {})
        c._ttl = inp.get("ttl", 300)
        c._max = inp.get("max_items", 1000)
        c._purge()
        return {"ret": None, "state": dict(c._store)}
    raise ValueError("unknown func: %s" % func)


def main():
    try:
        ns = extract()
        install_stubs(ns)
    except SystemExit:
        raise
    except Exception as e:
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
        CUR["clean_idx"] = 0
        CUR["flush_idx"] = 0
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        if want != got:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE api_server_flow: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        fatal("zero cases compared")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
