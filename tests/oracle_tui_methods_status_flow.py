#!/usr/bin/env python3
"""Oracle for the TUI session live-status / registry FLOW port
(src/gateway/tui_methods_status_flow.c).

MODULE RESOLUTION: the task named tui_gateway/methods_status.py, which does
NOT exist in the quarry. The TUI's live session STATUS methods
(_session_live_status / _session_pending_kind / _session_live_title) and the
registry claim/reuse + build-wait + RPC session-resolver flows that feed them
are defined in tui_gateway/session_registry.py (fully UNCLAIMED). That is the
REAL defining module the port credits.

Drives the LIVE session_registry bodies with SCRIPTED seams: every module
global, in-function import, sibling flow and external call is a recording stub
fed the same scripts the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the
final return ("ret") and the mutated module-global mirror ("state": _sessions).

Live defs are AST-extracted from tui_gateway/session_registry.py (module
Assign constants seeded, FunctionDefs exec'd into a registered sys.modules
module — py3.14). The heavy quarry import chain is never imported. Ported FLOW
siblings are SEAMED inside other flows (scripted) and run REAL in their own
direct cases; ported PURE-LEAF siblings (_session_lookup_key,
_live_profile_matches, _durable_ids, _legacy_display_anchor) run REAL inside
flows.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_status_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception / suppress site. The FULL
  builtin hierarchy rides make_exc; an unknown kind defaults to Exception.
- Doubles: session/current/record are DICTS (live .get); agent is a
  SimpleNamespace (getattr/setattr); lease/db/persistence/events are marker
  doubles ({"__lease__"/"__db__"/"__persistence__"/"__event__": role}). A
  callable api_key is {"__callable__": true}. _ANY_PROFILE / _detached_ws_transport
  sentinels are the markers "__ANY_PROFILE__" / "__DETACHED_WS__".
- Identity (`_sessions.get(sid) is current`): args.current_sid names the
  registry record that IS current; absent/null => standalone args.current.
- State: state.sessions mirrors _sessions; registry funcs materialize/echo it.
  _pop_session_by_id removes the sid on a non-null result.
- Tuples->lists; sets->sorted lists; integral floats->ints; ASCII/POSIX scope.
- Seam args drop None-valued TOP-LEVEL keys on both sides; nested nulls kept.
- Locks and CM enter/exit are SILENT; only the yielded value / wrapped call is
  scripted.
"""
import ast
import contextlib
import copy
import json
import os as _os_real
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePath

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os_env = _os_real.environ
os_env.setdefault("HOME", str(_REPO.parent))
os_env.setdefault("USERPROFILE", str(_REPO.parent))
os_env.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os_env.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = _os_real.path.expanduser("~/hermes-agent")
SRC = _os_real.path.join(Q, "tui_gateway", "session_registry.py")
_FLAGS = _fut.annotations.compiler_flag

PORTED = [
    "_wait_agent", "_agent_build_wait_cap", "_bind_build_profile_scopes",
    "_release_build_profile_scopes", "_deferred_build_agent_kwargs",
    "_await_resume_history", "_attach_built_agent", "_start_session_services",
    "_finish_agent_build", "_sess_nowait", "_sess_building", "_sess",
    "_new_session_key", "_is_server_minted_key", "_any_live_session_claims_key",
    "_live_profile_matches", "_session_lookup_key", "_find_live_session_by_key",
    "_claim_parked_runtimes", "_finalize_superseded_runtimes",
    "_claim_or_reuse_live", "_session_pending_kind", "_session_live_status",
    "_session_live_title", "_durable_ids", "_legacy_display_anchor",
    "_reconcile_display_with_live", "_main_runtime_from_agent",
]
# ported FLOW siblings seamed inside other flows (run REAL in own direct cases)
FLOW_SIBLINGS = [
    "_sess_nowait", "_sess_building", "_wait_agent", "_session_pending_kind",
    "_find_live_session_by_key", "_claim_parked_runtimes",
    "_finalize_superseded_runtimes", "_release_build_profile_scopes",
]
# functions that touch the _sessions module global (materialize + echo it)
REGISTRY_FUNCS = {
    "_any_live_session_claims_key", "_find_live_session_by_key",
    "_claim_parked_runtimes", "_claim_or_reuse_live", "_start_session_services",
    "_attach_built_agent", "_await_resume_history", "_finish_agent_build",
    "_sess_nowait",
}


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


# ── exception classes (real builtins; unknown kinds -> Exception subclass) ──
EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "TypeError": TypeError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning,
}
import asyncio as _asyncio_real
EXC_CLASSES["CancelledError"] = _asyncio_real.CancelledError


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, spec.get("message", ""))
        return e
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── sentinels ──
_ANY_PROFILE = object()
_DETACHED_WS_TRANSPORT = object()


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if x is _DETACHED_WS_TRANSPORT:
            return "__DETACHED_WS__"
        if x is _ANY_PROFILE:
            return "__ANY_PROFILE__"
        if isinstance(x, _Event):
            return {"__event__": x.role}
        if isinstance(x, _Lease):
            return {"__lease__": True}
        if isinstance(x, _Db):
            return {"__db__": True}
        if isinstance(x, _Persistence):
            return {"__persistence__": True}
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, PurePath):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if callable(x) and not isinstance(x, type):
            return {"__callable__": True}
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── per-case script backend (mirrors the C tmst_call exactly) ──
_MISSING = object()
_CUR_CTX = None


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
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── doubles ──
class _Event:
    def __init__(self, ctx, role):
        self.ctx = ctx
        self.role = role

    def wait(self, timeout=None):
        return self.ctx.call(self.role + ".wait", {"timeout": timeout})

    def is_set(self):
        return self.ctx.call(self.role + ".is_set", {})

    def set(self):
        return self.ctx.call(self.role + ".set", {})


class _Lease:
    def __init__(self, ctx):
        self.ctx = ctx

    def release(self):
        return self.ctx.call("lease.release", {})


class _Db:
    def __init__(self, ctx):
        self.ctx = ctx

    def close(self):
        return self.ctx.call("db.close", {})


class _Persistence:
    def __init__(self, ctx):
        self.sessions = types.SimpleNamespace(
            get_title=rec(ctx, "get_title", lambda key: {"key": key}))


class _YieldCM:
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


class _SilentCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class _Log:
    def __init__(self, ctx):
        self.ctx = ctx

    def warning(self, fmt, *args):
        return self.ctx.call("log.warning", {"fmt": fmt, "args": list(args)})

    def exception(self, fmt, *args):
        return self.ctx.call("log.exception", {"fmt": fmt, "args": list(args)})

    def info(self, fmt, *args):
        return self.ctx.call("log.info", {"fmt": fmt, "args": list(args)})

    def debug(self, fmt, *args):
        return self.ctx.call("log.debug", {"fmt": fmt, "args": list(args)})


def ns_double(raw):
    if not isinstance(raw, dict):
        return raw
    fields = {}
    for k, v in raw.items():
        if isinstance(v, dict) and v.get("__callable__"):
            fields[k] = lambda *a, **k: None
        else:
            fields[k] = v
    return types.SimpleNamespace(**fields)


def materialize_record(rec_, ctx):
    """In-place: agent dict->ns, transport marker->sentinel, event markers->doubles."""
    if not isinstance(rec_, dict):
        return rec_
    if isinstance(rec_.get("agent"), dict):
        rec_["agent"] = ns_double(rec_["agent"])
    if rec_.get("transport") == "__DETACHED_WS__":
        rec_["transport"] = _DETACHED_WS_TRANSPORT
    for k in ("agent_ready", "resume_history_ready", "_notif_stop"):
        v = rec_.get(k)
        if isinstance(v, dict) and v.get("__event__"):
            rec_[k] = _Event(ctx, v["__event__"])
    return rec_


# ── extraction ──
def extract():
    src_text = Path(SRC).read_text(encoding="utf-8")
    mod = new_module("tmst_oracle_session_registry")
    ns = mod.__dict__
    ns.update({
        "contextlib": contextlib, "re": re, "Path": Path,
        "_detached_ws_transport": _DETACHED_WS_TRANSPORT,
    })
    seed_constants(src_text, ns, SRC)
    for node in ast.parse(src_text).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src_text, node, ns, SRC)
    # _ANY_PROFILE is defined in-module; ensure the sentinel identity is the
    # one canon/drive use (rebind to our sentinel so projections agree).
    ns["_ANY_PROFILE"] = _ANY_PROFILE
    required = PORTED + ["_ANY_PROFILE", "_MINTED_SESSION_KEY_RE"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_status_flow: session_registry missing: {missing}")
        sys.exit(2)
    return ns, src_text


NS, SRC_TEXT = extract()
REAL = {nm: NS[nm] for nm in PORTED}

# ── fake import modules (in-function imports defeat ns seeds) ──
AGENT_MOD = new_module("agent")
RUNTIME_SCOPE_MOD = new_module("agent.runtime_scope")
AGENT_MOD.runtime_scope = RUNTIME_SCOPE_MOD
MESSAGE_META_MOD = new_module("agent.message_metadata")
MESSAGE_META_MOD.MESSAGE_UID = "message_uid"
MESSAGE_META_MOD.ABSORBED_MESSAGE_UIDS = "_absorbed_message_uids"
AGENT_MOD.message_metadata = MESSAGE_META_MOD
TOOLS_MOD = new_module("tools")
APPROVAL_MOD = new_module("tools.approval")
TOOLS_MOD.approval = APPROVAL_MOD
TUI_GW_MOD = new_module("tui_gateway")
SERVER_REQUESTS_MOD = new_module("tui_gateway.server_requests")
TUI_GW_MOD.server_requests = SERVER_REQUESTS_MOD
LOGGING_MOD = new_module("logging")


def proj_ph(ph):
    if ph is _ANY_PROFILE:
        return "__ANY_PROFILE__"
    return ph


def sibling_args(live):
    if live in ("_sess_nowait", "_sess_building"):
        return lambda params, rid: {"params": params, "rid": rid}
    if live == "_wait_agent":
        return lambda session, rid, timeout=30.0: {"session": session, "rid": rid, "timeout": timeout}
    if live == "_session_pending_kind":
        return lambda sid: {"sid": sid}
    if live == "_find_live_session_by_key":
        return lambda session_key, profile_home=_ANY_PROFILE: {
            "session_key": session_key, "profile_home": proj_ph(profile_home)}
    if live == "_claim_parked_runtimes":
        return lambda session_key, keep_sid, profile_home=_ANY_PROFILE: {
            "session_key": session_key, "keep_sid": keep_sid, "profile_home": proj_ph(profile_home)}
    if live == "_finalize_superseded_runtimes":
        return lambda stale: {"stale": stale}
    if live == "_release_build_profile_scopes":
        return lambda scopes: {"scopes": scopes}
    raise RuntimeError(f"no sibling_args for {live}")


def install_stubs(ctx, registry):
    # restore every ported def to REAL, then seam the FLOW siblings
    for nm, real in REAL.items():
        NS[nm] = real
    for live in FLOW_SIBLINGS:
        NS[live] = rec(ctx, live, sibling_args(live))
    # module globals
    NS["_sessions"] = registry
    NS["_sessions_lock"] = _SilentCM()
    NS["_session_resume_lock"] = _SilentCM()
    NS["logger"] = _Log(ctx)
    NS["_current_rpc_method"] = types.SimpleNamespace(
        get=rec(ctx, "_current_rpc_method.get", lambda: {}))
    NS["AGENT_STILL_STARTING"] = (
        "Hermes is still starting this session (loading tools), so this command could not run yet. "
        "Wait for the status bar to show ready and try again.")
    # external seams (bare globals)
    NS["_err"] = rec(ctx, "_err", lambda rid, code, msg, data=None: {
        "rid": rid, "code": code, "message": msg})
    NS["_load_cfg"] = rec(ctx, "_load_cfg", lambda: {})
    NS["_profile_runtime_scope_tokens"] = rec(
        ctx, "_profile_runtime_scope_tokens",
        lambda profile_home, hydrate_secrets=True: {"profile_home": profile_home})
    NS["_release_profile_runtime_scope_tokens"] = rec(
        ctx, "_release_profile_runtime_scope_tokens", lambda token: {"scopes": token})
    NS["_context_cwd_is_launch_artifact"] = rec(
        ctx, "_context_cwd_is_launch_artifact", lambda current: {"current": current})
    NS["_session_source"] = rec(ctx, "_session_source", lambda session: {"session": session})
    NS["_session_cwd"] = rec(ctx, "_session_cwd", lambda session: {"session": session})
    NS["_overrides_have_routable_provider"] = rec(
        ctx, "_overrides_have_routable_provider", lambda overrides: {"overrides": overrides})
    NS["_register_session_cwd"] = rec(ctx, "_register_session_cwd", lambda record: {"record": record})
    NS["_session_todo_state"] = rec(ctx, "_session_todo_state", lambda record: {"record": record})
    NS["_config_model_target"] = rec(ctx, "_config_model_target", lambda: {})
    NS["_start_notification_poller"] = rec(
        ctx, "_start_notification_poller", lambda sid, rec_: {"sid": sid, "rec": rec_})
    NS["_notify_session_boundary"] = rec(
        ctx, "_notify_session_boundary", lambda hook, key, source: {
            "hook": hook, "key": key, "source": source})
    NS["_start_agent_build"] = rec(
        ctx, "_start_agent_build", lambda sid, session: {"sid": sid, "session": session})
    NS["_transfer_db_to_agent"] = rec(
        ctx, "_transfer_db_to_agent", lambda agent, db: {
            "agent": "agent" if agent is not None else None, "db": "db"})
    NS["_release_persistence"] = rec(
        ctx, "_release_persistence", lambda persistence: {
            "persistence": "persistence" if persistence is not None else None})
    NS["new_session_id"] = rec(ctx, "new_session_id", lambda: {})
    NS["_coerce_message_text"] = rec(
        ctx, "_coerce_message_text", lambda content, image_urls=True: {"content": content})
    NS["_cancel_ws_orphan_reap"] = rec(ctx, "_cancel_ws_orphan_reap", lambda sid: {"sid": sid})
    NS["_teardown_popped_session"] = rec(
        ctx, "_teardown_popped_session", lambda popped, end_reason: {
            "popped": popped, "end_reason": end_reason})
    NS["_session_db"] = rec(ctx, "session_db", lambda session: {"session": session},
                            build=lambda raw: _YieldCM(raw))
    NS["_persistence_for"] = rec(
        ctx, "persistence_for", lambda db: {"db": "db" if db is not None else None},
        build=lambda raw: _YieldCM(_Persistence(ctx) if raw is not None else None))

    # _pop_session_by_id: record + pop side effect on the registry
    def _pop(sid):
        raw = ctx.call("_pop_session_by_id", {"sid": sid})
        if raw is not None:
            registry.pop(sid, None)
        return raw
    NS["_pop_session_by_id"] = _pop

    # in-function import targets
    RUNTIME_SCOPE_MOD.bind_state = rec(
        ctx, "bind_state", lambda scope, secrets: {"scope": "scope", "secrets": secrets})
    RUNTIME_SCOPE_MOD.scope_for_profile = rec(
        ctx, "scope_for_profile", lambda profile_home: {"profile_home": profile_home})
    APPROVAL_MOD.unregister_gateway_notify = rec(
        ctx, "unregister_gateway_notify", lambda key: {"key": key})
    SERVER_REQUESTS_MOD.pending_kind = rec(ctx, "pending_kind", lambda sid: {"sid": sid})
    LOGGING_MOD.getLogger = lambda name: _Log(ctx)


def resolve_ph(args):
    if "profile_home" not in args:
        return _ANY_PROFILE
    ph = args["profile_home"]
    if ph == "__ANY_PROFILE__":
        return _ANY_PROFILE
    return ph


def resolve_current(args, registry, ctx):
    csid = args.get("current_sid")
    if csid and csid in registry:
        return registry[csid]
    cur = copy.deepcopy(args.get("current"))
    return materialize_record(cur, ctx)


def drive(func, inp):
    global _CUR_CTX
    inp = copy.deepcopy(inp)
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = inp.get("state") or {}
    registry = state_in.get("sessions")
    if func in REGISTRY_FUNCS:
        if registry is None:
            registry = {}
            state_in["sessions"] = registry
        for sid in list(registry):
            materialize_record(registry[sid], ctx)
    elif registry is None:
        registry = {}
    saved_logging = sys.modules.get("logging")
    install_stubs(ctx, registry)
    sys.modules["logging"] = LOGGING_MOD
    if func in REAL:
        NS[func] = REAL[func]
    ret = None
    try:
        if func == "_wait_agent":
            session = materialize_record(copy.deepcopy(args.get("session")), ctx)
            ret = NS[func](session, args.get("rid"), timeout=args.get("timeout", 30.0))
        elif func == "_agent_build_wait_cap":
            ret = NS[func]()
        elif func == "_bind_build_profile_scopes":
            ret = NS[func](args.get("profile_home"))
        elif func == "_release_build_profile_scopes":
            ret = NS[func](args.get("scopes"))
        elif func == "_deferred_build_agent_kwargs":
            ret = NS[func](args.get("current"), args.get("session_db"))
        elif func == "_await_resume_history":
            cur = resolve_current(args, registry, ctx)
            ret = NS[func](args.get("sid"), cur)
        elif func == "_attach_built_agent":
            cur = resolve_current(args, registry, ctx)
            agent = ns_double(args.get("agent"))
            ret = NS[func](args.get("sid"), cur, agent)
        elif func == "_start_session_services":
            cur = args.get("current")
            ret = NS[func](args.get("sid"), args.get("key"), cur)
        elif func == "_finish_agent_build":
            cur = resolve_current(args, registry, ctx)
            lease = args.get("lease")
            db = _Db(ctx) if args.get("session_db") is not None else None
            ret = NS[func](args.get("sid"), args.get("key"), cur,
                           notify_registered=args.get("notify_registered", False),
                           scopes=args.get("scopes"), session_db=db,
                           persistence=args.get("persistence"))
        elif func == "_sess_nowait":
            ret = NS[func](args.get("params"), args.get("rid"))
        elif func == "_sess_building":
            ret = NS[func](args.get("params"), args.get("rid"))
        elif func == "_sess":
            ret = NS[func](args.get("params"), args.get("rid"))
        elif func == "_new_session_key":
            ret = NS[func]()
        elif func == "_is_server_minted_key":
            ret = NS[func](args.get("value"))
        elif func == "_any_live_session_claims_key":
            ret = NS[func](args.get("target"))
        elif func == "_live_profile_matches":
            session = materialize_record(copy.deepcopy(args.get("session")), ctx)
            ret = NS[func](session, resolve_ph(args))
        elif func == "_session_lookup_key":
            session = materialize_record(copy.deepcopy(args.get("session")), ctx)
            ret = NS[func](session, fallback=args.get("fallback", ""))
        elif func == "_find_live_session_by_key":
            ret = NS[func](args.get("session_key"), resolve_ph(args))
        elif func == "_claim_parked_runtimes":
            ret = NS[func](args.get("session_key"), keep_sid=args.get("keep_sid"),
                           profile_home=resolve_ph(args))
        elif func == "_finalize_superseded_runtimes":
            ret = NS[func](args.get("stale"))
        elif func == "_claim_or_reuse_live":
            lease = _Lease(ctx) if args.get("lease") is not None else None
            ret = NS[func](args.get("sid"), args.get("session_key"),
                           args.get("record"), lease)
        elif func == "_session_pending_kind":
            ret = NS[func](args.get("sid"))
        elif func == "_session_live_status":
            session = materialize_record(copy.deepcopy(args.get("session")), ctx)
            ret = NS[func](args.get("sid"), session)
        elif func == "_session_live_title":
            session = copy.deepcopy(args.get("session"))
            ret = NS[func](session, args.get("key"))
        elif func == "_durable_ids":
            ret = NS[func](args.get("msg"))
        elif func == "_legacy_display_anchor":
            ret = NS[func](args.get("display"), args.get("memory"))
        elif func == "_reconcile_display_with_live":
            ret = NS[func](args.get("db_display"), args.get("in_memory"))
        elif func == "_main_runtime_from_agent":
            ret = NS[func](ns_double(args.get("agent")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    finally:
        if saved_logging is not None:
            sys.modules["logging"] = saved_logging
        elif "logging" in sys.modules:
            del sys.modules["logging"]
        _CUR_CTX = None
    state_out = dict(state_in)
    if func in REGISTRY_FUNCS:
        state_out["sessions"] = registry
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "_err", "_load_cfg", "agent_ready.wait", "agent_ready.is_set",
    "resume_history.wait", "old_stop.set", "lease.release", "db.close",
    "_profile_runtime_scope_tokens", "scope_for_profile", "bind_state",
    "_release_profile_runtime_scope_tokens", "_context_cwd_is_launch_artifact",
    "_session_source", "_session_cwd", "_overrides_have_routable_provider",
    "_register_session_cwd", "_session_todo_state", "_config_model_target",
    "_start_notification_poller", "_notify_session_boundary", "_start_agent_build",
    "_transfer_db_to_agent", "_release_persistence", "unregister_gateway_notify",
    "new_session_id", "pending_kind", "_coerce_message_text", "session_db",
    "persistence_for", "get_title", "_cancel_ws_orphan_reap", "_pop_session_by_id",
    "_teardown_popped_session", "_current_rpc_method.get", "log.warning",
    "log.exception",
] + FLOW_SIBLINGS


def cross_check():
    c_src = (_REPO / "src" / "gateway" / "tui_methods_status_flow.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'tmst_call\(\s*ctx\s*,\s*"([^"]+)"', c_src))
    # sibling calls run via direct C function (pure leaves) are not tmst_call sites
    oracle_seams = set(SEAM_NAMES)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_status_flow.jsonl"
    fix_seams = set()
    if fix.exists():
        for line in fix.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_seams |= set((case.get("in") or {}).get("io", {}).keys())
    problems = []
    for nm in sorted(c_seams - oracle_seams):
        problems.append(f"C seam '{nm}' has no oracle SEAM_NAMES entry")
    for nm in sorted(fix_seams - c_seams):
        problems.append(f"fixture io key '{nm}' has no C tmst_call site")
    if problems:
        print("FATAL oracle_tui_methods_status_flow: seam cross-check:")
        for p in problems:
            print("  " + p)
        sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace", []), got.get("trace", [])
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
        print(f"  ret want={json.dumps(want.get('ret'))[:300]}")
        print(f"  ret got ={json.dumps(got.get('ret'))[:300]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w)[:300]}")
        print(f"  trace[{i}] got ={json.dumps(g)[:300]}")
    if want.get("state") != got.get("state"):
        print(f"  state want={json.dumps(want.get('state'))[:300]}")
        print(f"  state got ={json.dumps(got.get('state'))[:300]}")


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
    print(f"ORACLE tui_methods_status_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(180, exit=True)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_status_flow.jsonl"
    n = bad = 0
    counts = {}
    known_names = set(PORTED) | set(SEAM_NAMES)
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in known_names:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_is_server_minted_key", {"args": {"value": "20261118_120000_abc123"}})
    assert spot["ret"] is True, spot
    spot = drive("_is_server_minted_key", {"args": {"value": "20261118_120000_ABC123"}})
    assert spot["ret"] is False, spot
    spot = drive("_live_profile_matches",
                 {"args": {"session": {"profile_home": "/h/p1"}, "profile_home": "__ANY_PROFILE__"}})
    assert spot["ret"] is True, spot
    spot = drive("_session_lookup_key",
                 {"args": {"session": {"agent": {"session_id": "A"}, "session_key": "K"},
                           "fallback": "F"}})
    assert spot["ret"] == "A", spot
    spot = drive("_durable_ids", {"args": {"msg": {"message_uid": "u1", "_row_id": 5}}})
    assert spot["ret"] == [["row", 5], ["uid", "u1"]], spot
    spot = drive("_main_runtime_from_agent",
                 {"args": {"agent": {"model": " m1 ", "api_key": {"__callable__": True}}}})
    assert spot["ret"] == {"model": "m1", "api_key": {"__callable__": True}}, spot
    spot = drive("_agent_build_wait_cap", {"args": {}, "io": {"_load_cfg": {"agent": {"build_wait_timeout": 90}}}})
    assert spot["ret"] == 90, spot
    spot = drive("_session_live_status",
                 {"args": {"sid": "s1", "session": {"running": True}},
                  "io": {"_session_pending_kind": ""}})
    assert spot["ret"] == "working", spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_status_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()