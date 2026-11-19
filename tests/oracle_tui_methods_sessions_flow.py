#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_session.py FLOW port
(src/gateway/tui_methods_sessions_flow.c).

NOTE: the quarry module is tui_gateway/methods_session.py (SINGULAR — there is
no methods_sessions.py); the PoP annotations name the REAL defining module.
The deliverable keeps the task's "sessions" spelling.

Drives the LIVE methods_session bodies with SCRIPTED seams: every server
global (_ok / _err / _sessions / _sess / _sess_nowait / _session_db /
_profile_db / _persistence_for / _db_unavailable_error / _emit / logger /
uuid / git_probe / ...), claimed siblings (_str_param / _denied_source from
port_methods_session.c, _display_count_field from tui_methods_flow.c),
in-function import callables and callback args are recording stubs fed the
same scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
return ("ret") and the mutated caller-dict mirror ("state": session).

Live defs are AST-extracted from methods_session.py (module Assign constants
seeded, plain-Name targets only; UNDECORATED FunctionDefs exec'd with the
future-annotations flag into a registered sys.modules module — py3.14). The
heavy quarry import chain is never imported. DESKTOP_BACKEND_CONTRACT is
AST-extracted from tui_gateway/server.py, BOT_CHAT_TITLE from
tools/bot_mode_probe.py, INTERNAL_LISTING_SOURCES from
persistence/conversation/sessions.py. Ported leaf-flows (_row_of,
_unarchive_recoverable, _session_row_summary, _try_get_session,
_billing_invalid, _billing_call) compose LIVE so nested decision logic is
exercised, matching the C port's inline transcriptions. Decorator factories
(_session_arg / _session_method / _with_db / _pet_method / _billing_route)
are driven through their generated handlers; the registry decorators are
identity (no decoration-time events).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE tui_methods_sessions_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site. str(exc) is the
  message EXCEPT KeyError (single-arg str() == repr(message)).
- BillingError is a synthetic Exception subclass shared between make_exc and
  the hermes_cli.nous_billing stub; _billing_call catches exactly that kind.
- Doubles: scripted dicts become SimpleNamespaces (attribute-missing messages
  name 'types.SimpleNamespace'); a double is ALWAYS truthy (even {}). Plain
  dict rows keep container truthiness + dict subscript semantics.
- Persistence handles: dict scripts with optional sessions/lineage sub-objects
  (method seams fire only when the sub-object key is present), read_only /
  scope plain attributes. Writers always carry sessions.unarchive_recoverable
  + close. Handles project back to their script marker via __marker__.
- CM markers (session_db / profile_db / persistence_for scripts): a dict
  enters to itself (the handle marker); {"cm_enter_none": true} enters to
  None; null/non-dict raises TypeError at the live `with` site. Silent
  enter/exit (no events).
- Tuples project to lists; integral floats to ints; Paths to POSIX strings
  (fixture paths absolute POSIX, no trailing slash, no "." components).
  Fixture numbers are ints. str()/strip() fold the ASCII corpus only
  (strip whitespace includes \\x1c-\\x1f).
- Unpacking: lists/strings iterable; wrong length -> ValueError "(expected N,
  got M)"; non-iterables -> TypeError. Iteration domains (entries /
  sessions_items) are lists or null.
- _legacy_spawn_tree_entry json.loads: fixture text stays in the strict
  Python JSON grammar (no NaN/Infinity, no lone-surrogate escapes); the C
  port pre-validates that grammar before the lenient libjson parse.
- _b64 data is ASCII text interpreted as bytes.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "tui_gateway", "methods_session.py")
SERVER_SRC = os.path.join(Q, "tui_gateway", "server.py")
BOTPROBE_SRC = os.path.join(Q, "tools", "bot_mode_probe.py")
PSESSIONS_SRC = os.path.join(Q, "persistence", "conversation", "sessions.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def extract_assign(path, name):
    src = open(path, encoding="utf-8").read()
    ns = {}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            exec_node(src, node, ns, path)
    if name not in ns:
        print(f"FATAL oracle_tui_methods_sessions_flow: {name} not found in {path}")
        sys.exit(2)
    return ns[name]


PORTED = [
    "_session_arg", "_session_method", "_with_db", "_new_runtime_ids",
    "_profile_build_scope", "_make_agent_in_context", "_cwd_info",
    "_session_row_summary", "_row_of", "_listing_rows", "_snapshot_sessions",
    "_pet_display_cfg", "_pet_emit", "_pet_gen_abort", "_pet_method",
    "_active_pet", "_b64", "_pet_config_followup", "_pet_pick_provider",
    "_billing_call", "_billing_invalid", "_billing_pending_change",
    "_billing_route", "_unarchive_recoverable", "_session_list_by_title",
    "_find_live_unpersisted", "_status_row", "_try_get_session", "_title_read",
    "_auto_reload", "_legacy_spawn_tree_entry",
]


# ── methods_session.py namespace (live defs + seeded constants) ──
# The registry stub CAPTURES decorated handlers: _billing_route registers its
# handler via @method(name) and returns None (the real HandlerRegistry keeps
# it); drive() fetches the captured handler by name.
REGISTERED = {}


def _make_registry():
    def method(name):
        def deco(fn):
            REGISTERED[name] = fn
            return fn
        return deco
    return types.SimpleNamespace(method=method, profile_scoped=lambda fn: fn)


def extract():
    mod = new_module("tms_oracle_methods_session")
    ns = mod.__dict__
    import contextlib
    import threading
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "re": re,
               "json": json, "threading": threading, "Path": Path,
               "types": types, "copy": copy})
    # method_ctx stubs (the registry decorators capture; no call-time events)
    ns["HandlerRegistry"] = _make_registry
    ns["bind_module"] = lambda *a, **k: None
    ns["INTERNAL_LISTING_SOURCES"] = extract_assign(
        PSESSIONS_SRC, "INTERNAL_LISTING_SOURCES")
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC)
    # re-seed: _with_session / _with_live_session need _session_arg defined
    seed_constants(src, ns, SRC)
    ns["DESKTOP_BACKEND_CONTRACT"] = extract_assign(
        SERVER_SRC, "DESKTOP_BACKEND_CONTRACT")
    required = PORTED + ["_with_session", "_with_live_session", "method",
                         "_profile_scoped", "_LISTING_DENY_SOURCES"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_sessions_flow: methods_session missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
BOT_CHAT_TITLE = extract_assign(BOTPROBE_SRC, "BOT_CHAT_TITLE")


# ── exception materialization / projection ──
BillingError = type("BillingError", (Exception,), {})

EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "PermissionError": PermissionError,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "InterruptedError": InterruptedError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "ProcessLookupError": ProcessLookupError, "IndexError": IndexError,
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError, "BillingError": BillingError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── canonical projection (compare parsed JSON, never raw strings) ──
class SilentCM:
    """Silent context-manager double carrying its script marker."""
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class CMEnter:
    """Silent CM whose __enter__ yields a scripted value (None included)."""
    def __init__(self, enter_value, marker):
        self.__enter_value__ = enter_value
        self.__cm_marker__ = marker

    def __enter__(self):
        return self.__enter_value__

    def __exit__(self, *a):
        return False


class CallableMarker:
    """fail_open / invalid callable double carrying its script marker."""
    def __init__(self, fn):
        self._fn = fn

    def __call__(self, *a, **k):
        return self._fn(*a, **k)


def canon(v):
    def conv(x):
        if isinstance(x, (SilentCM, CMEnter)):
            return conv(x.__cm_marker__)
        if isinstance(x, CallableMarker):
            return {"__callable__": True}
        if isinstance(x, types.SimpleNamespace):
            if "__marker__" in vars(x):
                return conv(x.__marker__)
            return conv(vars(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── Path filesystem methods are seams (stat / read_text) ──
_CUR_CTX = None


def _path_stat(self):
    raw = _CUR_CTX.call("stat", {"path": self.as_posix()})
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def _path_read_text(self, encoding=None, errors=None):
    return _CUR_CTX.call("read_text", {"path": self.as_posix(), "encoding": encoding})


Path.stat = _path_stat
Path.read_text = _path_read_text
# str(Path) on the Windows gate host is ntpath (backslashes); the port's
# representation contract projects Paths to POSIX strings. Normalize at the
# source so the live `str(p)` in _legacy_spawn_tree_entry matches the C port.
# (as_posix itself calls str(self) — wrap the ORIGINAL __str__, no recursion.)
_orig_path_str = Path.__str__


def _path_str_posix(self):
    return _orig_path_str(self).replace("\\", "/")


Path.__str__ = _path_str_posix


# ── per-case script backend (mirrors the C tms_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        # C returns a fresh copy per call; live flows mutate seam results in
        # place, so the script entry must stay pristine.
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── attribute doubles ──
def row_build(raw, ctx):
    if isinstance(raw, dict):
        return types.SimpleNamespace(
            as_session_row=rec(ctx, "as_session_row", lambda: {}))
    return raw


def entry_build(raw, ctx):
    if isinstance(raw, dict):
        return types.SimpleNamespace(
            as_list_row=rec(ctx, "as_list_row", lambda: {}))
    return raw


def entries_build(raw, ctx):
    if isinstance(raw, list):
        return [entry_build(e, ctx) for e in raw]
    return raw


def pet_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def selection_build(raw):
    if isinstance(raw, list) and len(raw) == 3:
        return [raw[0], pet_build(raw[1]), raw[2]]
    return raw


def marker_ns(raw):
    ns = types.SimpleNamespace()
    ns.__marker__ = raw
    return ns


def persistence_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    ns = types.SimpleNamespace()
    ns.__marker__ = raw
    for k, v in raw.items():
        if k == "sessions":
            ns.sessions = types.SimpleNamespace(
                get=rec(ctx, "sessions_get",
                        lambda session_id: {"session_id": session_id},
                        build=lambda r: row_build(r, ctx)),
                get_by_title=rec(ctx, "get_by_title",
                                 lambda title: {"title": title},
                                 build=lambda r: row_build(r, ctx)),
                get_title=rec(ctx, "get_title", lambda key: {"key": key}),
                set_title=rec(ctx, "set_title",
                              lambda key, title: {"key": key, "title": title}),
                unarchive_recoverable=rec(
                    ctx, "unarchive_recoverable",
                    lambda session_id: {"session_id": session_id}),
                list_sessions=rec(
                    ctx, "list_sessions",
                    lambda query, window, compact_rows=True:
                    {"order": query.order, "limit": window.limit,
                     "compact_rows": compact_rows, **query.kw},
                    build=lambda r: entries_build(r, ctx)))
        elif k == "lineage":
            ns.lineage = types.SimpleNamespace(
                tip=rec(ctx, "lineage_tip",
                        lambda session_id: {"session_id": session_id}))
        elif k == "scope":
            ns.scope = types.SimpleNamespace(**v) if isinstance(v, dict) else v
        else:
            setattr(ns, k, v)
    return ns


def writer_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    return types.SimpleNamespace(
        sessions=types.SimpleNamespace(
            unarchive_recoverable=rec(
                ctx, "writer_unarchive",
                lambda session_id: {"session_id": session_id})),
        close=rec(ctx, "writer_close", lambda: {}))


def cm_build_db(raw):
    if isinstance(raw, dict):
        if raw.get("cm_enter_none"):
            return CMEnter(None, raw)
        return CMEnter(marker_ns(raw), raw)
    return raw


def cm_build_persist(raw, ctx):
    if isinstance(raw, dict):
        if raw.get("cm_enter_none"):
            return CMEnter(None, raw)
        return CMEnter(persistence_build(raw, ctx), raw)
    return raw


class RecLogger:
    def __init__(self, ctx):
        self.ctx = ctx

    def debug(self, fmt, *args, **kw):
        return self.ctx.call("logger_debug", {"msg": fmt % args})

    def warning(self, fmt, *args, **kw):
        return self.ctx.call("logger_warning", {"msg": fmt % args})


# ── in-function import stub modules (registered once, rebound per case) ──
def stub_pkg(name):
    if name not in sys.modules:
        new_module(name)
    return sys.modules[name]


def stub_mod(full):
    parts = full.split(".")
    for i in range(1, len(parts)):
        stub_pkg(".".join(parts[:i]))
    mod = new_module(full) if full not in sys.modules else sys.modules[full]
    parent = sys.modules.get(".".join(parts[:-1]))
    if parent is not None:
        setattr(parent, parts[-1], mod)
    return mod


CONFIG_MOD = stub_mod("hermes_cli.config")
NOUS_MOD = stub_mod("hermes_cli.nous_billing")
SESSIONS_MOD = stub_mod("persistence.conversation.sessions")
ASSEMBLY_MOD = stub_mod("persistence_assembly")
BOTPROBE_MOD = stub_mod("tools.bot_mode_probe")
IMAGEGEN_MOD = stub_mod("agent.pet.generate.imagegen")
BILLINGVIEW_MOD = stub_mod("agent.billing_view")
NOUS_MOD.BillingError = BillingError
SESSIONS_MOD.INTERNAL_LISTING_SOURCES = NS["INTERNAL_LISTING_SOURCES"]
BOTPROBE_MOD.BOT_CHAT_TITLE = BOT_CHAT_TITLE


class SessionQuery:
    def __init__(self, order=None, **kw):
        self.order = order
        self.kw = kw


class SessionWindow:
    def __init__(self, limit=None):
        self.limit = limit


SESSIONS_MOD.SessionOrder = types.SimpleNamespace(LAST_ACTIVE="LAST_ACTIVE")
SESSIONS_MOD.SessionQuery = SessionQuery
SESSIONS_MOD.SessionWindow = SessionWindow


# ── real defs restored before every case (seam slots are per-case) ──
REAL = {nm: NS[nm] for nm in PORTED}


def install_stubs(ctx):
    NS["_ok"] = rec(ctx, "ok", lambda rid, payload: {"rid": rid, "payload": payload})
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_sess_nowait"] = rec(ctx, "sess_nowait",
                             lambda params, rid: {"params": params, "rid": rid})
    NS["_sess"] = rec(ctx, "sess", lambda params, rid: {"params": params, "rid": rid})
    NS["_session_db"] = rec(ctx, "session_db", lambda session: {"session": session},
                            build=cm_build_db)
    NS["_profile_db"] = rec(ctx, "profile_db", lambda params: {"params": params},
                            build=cm_build_db)
    NS["_persistence_for"] = rec(ctx, "persistence_for", lambda db: {"db": db},
                                 build=lambda raw: cm_build_persist(raw, ctx))
    NS["_db_unavailable_error"] = rec(
        ctx, "db_unavailable_error", lambda rid, code=None: {"rid": rid, "code": code})
    NS["_str_param"] = rec(ctx, "str_param",
                           lambda params, key, default="":
                           {"params": params, "key": key, "default": default})
    NS["_denied_source"] = rec(ctx, "denied_source", lambda row: {"row": row})
    NS["_display_count_field"] = rec(
        ctx, "display_count_field",
        lambda persistence, session_id: {"session_id": session_id})
    NS["_resolve_session_source"] = rec(
        ctx, "resolve_session_source", lambda source: {"source": source})
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "session_profile_runtime_scope",
        lambda d: {"profile_home": d.get("profile_home")})
    NS["_set_session_context"] = rec(
        ctx, "set_session_context", lambda key, cwd=None: {"key": key, "cwd": cwd})
    NS["_make_agent"] = rec(
        ctx, "make_agent",
        lambda sid, key, session_id=None, **kw:
        {"sid": sid, "key": key, "session_id": session_id, "kwargs": kw})
    NS["_clear_session_context"] = rec(
        ctx, "clear_session_context", lambda tokens: {"tokens": tokens})
    NS["_session_info"] = rec(ctx, "session_info",
                              lambda agent, session: {"agent": agent, "session": session})
    NS["git_probe"] = types.SimpleNamespace(
        branch=rec(ctx, "git_branch", lambda cwd: {"cwd": cwd}))
    NS["_project_info_for_cwd"] = rec(
        ctx, "project_info_for_cwd", lambda cwd: {"cwd": cwd})
    NS["uuid"] = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=ctx.call("uuid4_hex", {})))
    NS["_sessions"] = types.SimpleNamespace(
        items=lambda: ctx.call("sessions_items", {}))
    NS["_sessions_lock"] = SilentCM({})
    NS["_emit"] = rec(ctx, "emit",
                      lambda event, sid, payload:
                      {"event": event, "session_id": sid, "payload": payload})
    NS["logger"] = RecLogger(ctx)
    NS["_pet_cancel_release"] = rec(
        ctx, "pet_cancel_release", lambda token: {"token": token})
    NS["_pet_active_selection"] = rec(
        ctx, "pet_active_selection", lambda: {}, build=selection_build)
    NS["_serialize_billing_error"] = rec(
        ctx, "serialize_billing_error", lambda exc: {"exc": proj_exc(exc)})
    CONFIG_MOD.load_config = rec(ctx, "load_config", lambda: {})
    ASSEMBLY_MOD.open_persistence = rec(
        ctx, "open_persistence",
        lambda scope, eager=True: {"scope": scope, "eager": eager},
        build=lambda raw: writer_build(raw, ctx))
    IMAGEGEN_MOD.resolve_provider = rec(
        ctx, "resolve_provider",
        lambda require_references=False, prefer=None:
        {"require_references": require_references, "prefer": prefer})
    BILLINGVIEW_MOD.new_idempotency_key = rec(
        ctx, "new_idempotency_key", lambda: {})


def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        NS[nm] = real
    install_stubs(ctx)
    try:
        if func == "_session_arg":
            resolve = rec(ctx, "resolve",
                          lambda params, rid: {"params": params, "rid": rid})
            fn = rec(ctx, "session_fn",
                     lambda rid, params, session:
                     {"rid": rid, "params": params, "session": session})
            handler = NS[func](resolve)(fn)
            ret = handler(args.get("rid"), args.get("params"))
        elif func == "_session_method":
            fn = rec(ctx, "session_fn",
                     lambda rid, params, session:
                     {"rid": rid, "params": params, "session": session})
            handler = NS[func](args.get("name", "m"),
                               live=args.get("live", False))(fn)
            ret = handler(args.get("rid"), args.get("params"))
        elif func == "_with_db":
            scoped = args.get("session_scoped", False)

            def fn(rid, params, *rest):
                a = {"rid": rid, "params": params}
                if scoped:
                    a["session"] = rest[0]
                a["persistence"] = rest[-1]
                return ctx.call("db_fn", a)
            handler = NS[func](args.get("code", 0), session_scoped=scoped)(fn)
            ret = handler(args.get("rid"), args.get("params"))
        elif func == "_new_runtime_ids":
            ret = NS[func](args.get("params"))
        elif func == "_profile_build_scope":
            ret = NS[func](args.get("profile_home"))
        elif func == "_make_agent_in_context":
            ret = NS[func](args.get("sid"), args.get("key"),
                           **(args.get("kwargs") or {}))
        elif func == "_cwd_info":
            ret = NS[func](args.get("session"), args.get("cwd"),
                           branch=args.get("branch"))
        elif func == "_session_row_summary":
            ret = NS[func](args.get("row"), tip_row=args.get("tip_row"),
                           resolved_id=args.get("resolved_id"),
                           persistence=persistence_build(args.get("persistence"), ctx))
        elif func == "_row_of":
            ret = NS[func](row_build(args.get("record"), ctx))
        elif func == "_listing_rows":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("limit", 200), **(args.get("kwargs") or {}))
        elif func == "_snapshot_sessions":
            ret = NS[func](args.get("rid"))
        elif func == "_pet_display_cfg":
            ret = NS[func]()
        elif func == "_pet_emit":
            ret = NS[func](args.get("event"), args.get("payload"), args.get("what"))
        elif func == "_pet_gen_abort":
            ret = NS[func](args.get("rid"), args.get("token"),
                           args.get("code", 0), args.get("message", ""))
        elif func == "_pet_method":
            slug = args.get("slug", False)

            def fn(rid, params, value=None):
                a = {"rid": rid, "params": params}
                if slug:
                    a["slug"] = value
                return ctx.call("pet_fn", a)
            fail_open = args.get("fail_open")
            if isinstance(fail_open, dict) and fail_open.get("__callable__"):
                fail_open = CallableMarker(
                    rec(ctx, "fail_open_fn", lambda params: {"params": params}))
            handler = NS[func](args.get("name", "m"), fail_open=fail_open,
                               slug=slug, scoped=args.get("scoped", True))(fn)
            ret = handler(args.get("rid"), args.get("params"))
        elif func == "_active_pet":
            ret = NS[func]()
        elif func == "_b64":
            data = args.get("data")
            if isinstance(data, str):
                data = data.encode("utf-8")
            ret = NS[func](data)
        elif func == "_pet_config_followup":
            fn = rec(ctx, "followup_fn", lambda *a: {"args": list(a)})
            ret = NS[func](args.get("what"), fn, *(args.get("args") or []))
        elif func == "_pet_pick_provider":
            ret = NS[func](args.get("params"),
                           require_references=args.get("require_references", False))
        elif func == "_billing_call":
            ret = NS[func](args.get("rid"), rec(ctx, "billing_fn", lambda: {}),
                           extra=args.get("extra"))
        elif func == "_billing_invalid":
            ret = NS[func](args.get("rid"), args.get("message", ""),
                           error=args.get("error", "invalid_request"))
        elif func == "_billing_pending_change":
            ret = NS[func](args.get("result"))
        elif func == "_billing_route":
            invalid = None
            if args.get("invalid") is not None:
                invalid = lambda p: ctx.call("invalid_fn", {"params": p})
            call = lambda nb, p, key: ctx.call("route_call", {"params": p, "key": key})
            name = args.get("name", "m")
            REGISTERED.clear()
            NS[func](name, call, invalid=invalid,
                     message=args.get("message", ""),
                     error=args.get("error", "invalid_request"),
                     idempotent=args.get("idempotent", False))
            handler = REGISTERED[name]
            ret = handler(args.get("rid"), args.get("params"))
        elif func == "_unarchive_recoverable":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("session_id"))
        elif func == "_session_list_by_title":
            ret = NS[func](args.get("rid"),
                           persistence_build(args.get("persistence"), ctx),
                           args.get("title_lookup"))
        elif func == "_find_live_unpersisted":
            ret = NS[func](args.get("needle"), args.get("home"))
        elif func == "_status_row":
            ret = NS[func](args.get("session"), args.get("params"), args.get("key"))
        elif func == "_try_get_session":
            ret = NS[func](args.get("db"), args.get("key"))
        elif func == "_title_read":
            session = state_in.setdefault("session", {})
            ret = NS[func](session, persistence_build(args.get("persistence"), ctx),
                           args.get("key"))
        elif func == "_auto_reload":
            nb = types.SimpleNamespace(
                patch_auto_top_up=rec(
                    ctx, "patch_auto_top_up",
                    lambda enabled=False, threshold=None, top_up_amount=None:
                    {"enabled": enabled, "threshold": threshold,
                     "top_up_amount": top_up_amount}))
            ret = NS[func](nb, args.get("p"), args.get("key"))
        elif func == "_legacy_spawn_tree_entry":
            ret = NS[func](Path(args["path"]), args.get("session_dir_name"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "resolve", "sess_nowait", "sess", "session_fn", "session_db", "profile_db",
    "persistence_for", "db_unavailable_error", "db_fn", "uuid4_hex", "str_param",
    "resolve_session_source", "session_profile_runtime_scope",
    "set_session_context", "make_agent", "clear_session_context", "session_info",
    "git_branch", "project_info_for_cwd", "display_count_field", "as_session_row",
    "list_sessions", "as_list_row", "denied_source", "sessions_items", "err",
    "ok", "load_config", "emit", "logger_debug", "logger_warning",
    "pet_cancel_release", "pet_fn", "fail_open_fn", "pet_active_selection",
    "followup_fn", "resolve_provider", "billing_fn", "serialize_billing_error",
    "invalid_fn", "new_idempotency_key", "route_call", "open_persistence",
    "unarchive_recoverable", "writer_unarchive", "writer_close", "get_by_title",
    "sessions_get", "lineage_tip", "get_title", "set_title", "patch_auto_top_up",
    "stat", "read_text",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_sessions_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_sessions_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tms_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_sessions_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_sessions_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_sessions_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_funcs.add(case.get("func"))
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_sessions_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_tui_methods_sessions_flow: fixture funcs outside PORTED: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE tui_methods_sessions_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_sessions_flow.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_b64", {"args": {"data": "hello"}})
    assert spot["ret"] == "aGVsbG8=", spot
    spot = drive("_billing_pending_change", {"args": {"result": {"message": "m", "x": 1}}})
    assert spot["ret"] == {"ok": True, "message": "m", "payload": {"message": "m", "x": 1}}, spot
    spot = drive("_billing_invalid",
                 {"args": {"rid": "r1", "message": "bad"},
                  "io": {"ok": {"jsonrpc": "2.0", "id": "r1", "result": {"ok": False}}}})
    assert spot["trace"][0]["args"]["payload"] == {
        "ok": False, "error": "invalid_request", "message": "bad"}, spot
    spot = drive("_row_of", {"args": {"record": None}})
    assert spot["ret"] is None and spot["trace"] == [], spot
    spot = drive("_session_row_summary",
                 {"args": {"row": {"id": "s1", "title": "T", "source": "web"},
                           "resolved_id": "tip1"},
                  "io": {}})
    assert spot["ret"] == {"id": "s1", "resolved_id": "tip1", "title": "T",
                           "preview": "", "started_at": 0, "message_count": 0,
                           "source": "web", "_lineage_root_id": None}, spot
    spot = drive("_snapshot_sessions",
                 {"args": {"rid": "r1"},
                  "io": {"sessions_items": {"__script__": True,
                         "raise": {"type": "RuntimeError", "message": "boom"}},
                         "err": {"jsonrpc": "2.0", "id": "r1",
                                 "error": {"code": 5036, "message": "e"}}},
                  "fail_closed": False})
    assert spot["ret"][0] is None, spot
    assert spot["trace"][1]["args"]["message"] == \
        "could not enumerate active sessions: boom", spot
    spot = drive("_find_live_unpersisted",
                 {"args": {"needle": "k1", "home": None},
                  "io": {"sessions_items": [[["live1", {"session_key": "k1"}]]]}})
    assert spot["ret"] == "live1", spot
    spot = drive("_pet_display_cfg", {"io": {"load_config": {"display": {"pet": {"enabled": True}}}}})
    assert spot["ret"] == {"enabled": True}, spot
    spot = drive("_legacy_spawn_tree_entry",
                 {"args": {"path": "/h/p/snap.json", "session_dir_name": "d1"},
                  "io": {"stat": {"st_mtime": 77},
                         "read_text": "{\"session_id\": \"s9\", \"subagents\": [1, 2]}"}})
    assert spot["ret"] == {"path": "/h/p/snap.json", "session_id": "s9",
                           "finished_at": 77, "started_at": None, "label": "",
                           "count": 2}, spot
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_sessions_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()