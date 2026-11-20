#!/usr/bin/env python3
"""Oracle for the Matrix-adapter FLOW remainder port
(src/gateway/platforms/matrix_adapter_flow2.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED decision chains
(authorization / invite-reconcile / DM-cache / reaction / read-receipt /
client-op / room-state / typing) with SCRIPTED seams: every client call,
state_store probe, scoped-secret reader, time.time and every seamed sibling
(claimed flow1 helpers + other ported flow2 targets invoked from a sibling
flow) is a recording stub fed the same scripts the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
mutated state ("state").

The live defs are AST-extracted from adapter.py's MatrixAdapter ClassDef
(decorators stripped, dedented) and bound onto SimpleNamespace self doubles.
The exec namespace is a registered sys.modules module (py3.14). The heavy
quarry import chain is never imported; mautrix types are the import-safe str
stubs. Async methods drive via asyncio.run with async seam recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec VERBATIM (C parks the spec; KeyError str() is repr(k)); unscripted
live raises project {__raise__, message:str(exc)}. Summary:
'ORACLE matrix_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; bools are bools; a type-sensitive float travels
  as {"__float__": true, "v": x}; integral floats normalize to int (canon).
- Attribute carriers read by a live body wrap {"__attr__": {attrs}} (oracle
  builds SimpleNamespace); client/state_store METHOD presence is a key holding
  JSON true (oracle binds an async recorder); hasattr == key presence.
- set-valued state (_joined_rooms) sorts on both sides; _dm_rooms is a dict.
- logger calls are silent (not traced); _client_op ok_msg/err_msg/level are
  carried but log-silent.
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
from contextlib import suppress
import typing

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "matrix", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

METHOD_FUNCS = [
    "_extra_truthy", "_parse_require_mention", "_parse_thread_require_mention",
    "_reset_clock_skew_detector", "_note_late_grace_drop", "_is_authorized_user",
    "_on_invite", "_schedule_pending_invite_joins", "_extract_invite_dm_signal",
    "_join_room_by_id", "_fetch_m_direct", "_refresh_dm_cache", "_record_dm_room",
    "_is_dm_room", "_send_reaction", "send_read_receipt", "_client_op",
    "redact_message", "invite_user", "set_presence", "_get_room_member_count",
    "_get_room_state_value", "_get_display_name", "_set_typing", "send_typing",
    "stop_typing",
]
STATIC_METHODS = {"_extra_truthy", "_parse_require_mention", "_parse_thread_require_mention"}
ASYNC_FUNCS = {
    "_on_invite", "_join_room_by_id", "_fetch_m_direct", "_refresh_dm_cache",
    "_record_dm_room", "_is_dm_room", "_send_reaction", "send_read_receipt",
    "_client_op", "redact_message", "invite_user", "set_presence",
    "_get_room_member_count", "_get_room_state_value", "_get_display_name",
    "_set_typing", "send_typing", "stop_typing",
}
# ported siblings composed REAL (not seamed) by these targets
COMPOSED = {
    "send_typing": ["_set_typing"], "stop_typing": ["_set_typing"],
    "redact_message": ["_client_op"], "invite_user": ["_client_op"],
    "set_presence": ["_client_op"],
}
# state keys each target may write (echoed even when absent from input)
WRITE_SET = {
    "_reset_clock_skew_detector": {"_late_grace_drops", "_late_grace_skew", "_clock_skew_warned"},
    "_note_late_grace_drop": {"_late_grace_drops", "_late_grace_skew", "_clock_skew_warned"},
    "_join_room_by_id": {"_joined_rooms"},
    "_refresh_dm_cache": {"_dm_rooms"},
    "_record_dm_room": {"_dm_rooms"},
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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ma2_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "re": re, "os": os, "json": json, "Path": Path, "suppress": suppress,
        "asyncio": asyncio, "typing": typing, "Any": typing.Any,
        "Optional": typing.Optional, "Dict": typing.Dict, "Set": typing.Set,
        # import-safe mautrix stubs (no mautrix in the oracle namespace)
        "RoomID": str, "EventID": str, "UserID": str, "ContentURI": str,
        "EventType": type("_EventTypeStub", (), {
            "ROOM_MESSAGE": "m.room.message", "REACTION": "m.reaction",
            "ROOM_ENCRYPTED": "m.room.encrypted", "ROOM_NAME": "m.room.name"}),
        "PresenceState": type("_PresenceStateStub", (), {
            "ONLINE": "online", "OFFLINE": "offline", "UNAVAILABLE": "unavailable"}),
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module-level plain-Name constants
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
    # class-level constants (e.g. _VALID_PRESENCE_STATES) read through self
    class_consts = {}
    for n in cls.body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
            if all(isinstance(t, ast.Name) for t in tgts):
                try:
                    exec_node(src, n, ns, SRC)
                    for t in tgts:
                        class_consts[t.id] = ns[t.id]
                except Exception:
                    pass
    for n in cls.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in METHOD_FUNCS:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec_node(src, n2, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = METHOD_FUNCS + ["_VALID_PRESENCE_STATES"]
    missing = [k for k in required if k not in ns and k not in class_consts]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    ns["__class_consts__"] = class_consts
    return ns


NS = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "Exception": Exception,
    "StopIteration": StopIteration, "IndexError": IndexError, "LookupError": LookupError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "TimeoutError": TimeoutError, "AssertionError": AssertionError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ma2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []  # (exc, verbatim spec) for identity round-trip

    def _fail(self, name, args, msg):
        exc = RuntimeError(msg)
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            self._fail(name, args, f"no script for {name}")
        used = entry
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._fail(name, args, f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                sp = used["raise"]
                exc = make_exc(sp)
                spec = {"__raise__": sp.get("type", "RuntimeError"),
                        "message": sp.get("message", "")}
                self.scripted.append((exc, spec))
                self.trace.append({"name": name, "args": args, "ret": spec})
                raise exc
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret

    def spec_for(self, exc):
        for e, spec in self.scripted:
            if e is exc:
                return spec
        return None


def rec(ctx, name, args_fn=None, build=None, is_async=False):
    if is_async:
        async def f(*a, **k):
            args = args_fn(*a, **k) if args_fn else {}
            raw = ctx.call(name, args)
            return build(raw) if build else raw
        return f

    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def pyval(v):
    if isinstance(v, dict) and v.get("__float__") is True:
        return float(v.get("v", 0))
    return v


def ad_build(raw):
    """{"__attr__": {attrs}} -> SimpleNamespace (shallow; nested dict stays dict)."""
    if isinstance(raw, dict) and "__attr__" in raw:
        inner = raw["__attr__"] or {}
        return types.SimpleNamespace(**{k: pyval(v) for k, v in inner.items()})
    return raw


def event_double(spec):
    """Recursive attribute double for an event (content nested -> SimpleNamespace)."""
    if spec is None:
        return None
    if isinstance(spec, dict):
        return types.SimpleNamespace(**{k: event_double(pyval(v)) for k, v in spec.items()})
    return spec


# ── seamed sibling specs (name -> args_fn, is_async, build) ──
SIBLING_SPECS = {
    "_is_authorized_user": (lambda user_id, room_id=None: {"user_id": user_id, "room_id": room_id}, False, None),
    "_schedule_invite_join": (lambda room_id, *, is_direct=False, inviter="": {"room_id": room_id, "is_direct": is_direct, "inviter": inviter}, False, None),
    "_extract_invite_dm_signal": (lambda invited_room: {"invited_room": invited_room}, False, lambda r: tuple(r) if isinstance(r, list) else r),
    "_invalidate_room_identities": (lambda room_id=None: {"room_id": room_id}, False, None),
    "_refresh_dm_cache": (lambda: {}, True, None),
    "_fetch_m_direct": (lambda **k: dict(k), True, None),
    "_resolve_room_identity": (lambda room_id, **k: {"room_id": room_id}, True, ad_build),
    "_state_event_value": (lambda event, key: {"event": event, "key": key}, False, None),
    "_is_sender_authorized": (lambda user_id, chat_id=None: {"user_id": user_id, "chat_id": chat_id}, False, None),
}
SEAMED_SIBLINGS = {
    "_on_invite": ["_is_authorized_user", "_schedule_invite_join"],
    "_schedule_pending_invite_joins": ["_extract_invite_dm_signal", "_is_authorized_user", "_schedule_invite_join"],
    "_join_room_by_id": ["_invalidate_room_identities", "_refresh_dm_cache"],
    "_refresh_dm_cache": ["_fetch_m_direct", "_invalidate_room_identities"],
    "_record_dm_room": ["_fetch_m_direct", "_invalidate_room_identities"],
    "_is_dm_room": ["_resolve_room_identity"],
    "_get_room_state_value": ["_state_event_value"],
    "_is_authorized_user": ["_is_sender_authorized"],
}

# ── client / state_store method specs ──
CLIENT_METHODS = {
    "join_room": (lambda room_id: {"room_id": room_id}, None),
    "leave_room": (lambda room_id: {"room_id": room_id}, None),
    "send_message_event": (lambda room_id, event_type, content: {"room_id": room_id, "event_type": event_type, "content": content}, None),
    "set_fully_read_marker": (lambda room_id, fully_read_event, read_receipt: {"room_id": room_id, "fully_read_event": fully_read_event, "read_receipt": read_receipt}, None),
    "send_receipt": (lambda room_id, event_id: {"room_id": room_id, "event_id": event_id}, None),
    "set_read_markers": (lambda room_id, *, fully_read_event=None, read_receipt=None: {"room_id": room_id, "fully_read_event": fully_read_event, "read_receipt": read_receipt}, None),
    "get_account_data": (lambda key: {"key": key}, ad_build),
    "set_account_data": (lambda key, content: {"key": key, "content": content}, None),
    "redact": (lambda room_id, event_id, reason=None: {"room_id": room_id, "event_id": event_id, "reason": reason}, None),
    "invite_user": (lambda room_id, user_id: {"room_id": room_id, "user_id": user_id}, None),
    "set_presence": (lambda *, presence=None, status=None: {"presence": presence, "status": status}, None),
    "set_typing": (lambda room_id, *, timeout=None: {"chat_id": room_id, "timeout": timeout}, None),
    "get_state_event": (lambda room_id, event_type: {"room_id": room_id, "event_type": event_type}, None),
    "joined_members": (lambda room_id: {"room_id": room_id}, ad_build),
}
SS_METHODS = {
    "get_members": (lambda room_id: {"room_id": room_id}, None),
    "get_member": (lambda room_id, user_id: {"room_id": room_id, "user_id": user_id}, ad_build),
}


def client_double(spec, ctx):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    ns = types.SimpleNamespace()
    for k, v in spec.items():
        if k == "state_store":
            setattr(ns, k, state_store_double(v, ctx))
        elif v is True and k in CLIENT_METHODS:
            afn, build = CLIENT_METHODS[k]
            setattr(ns, k, rec(ctx, "client." + k, afn, build, is_async=True))
        else:
            setattr(ns, k, pyval(v))
    return ns


def state_store_double(spec, ctx):
    if spec is None:
        return None
    if not isinstance(spec, dict):
        return spec
    ns = types.SimpleNamespace()
    for k, v in spec.items():
        if v is True and k in SS_METHODS:
            afn, build = SS_METHODS[k]
            setattr(ns, k, rec(ctx, "state_store." + k, afn, build, is_async=True))
        else:
            setattr(ns, k, pyval(v))
    return ns


def install_global_seams(ctx):
    NS["_extra_or_secret"] = rec(
        ctx, "extra_or_secret",
        lambda extra, key, env, default: {"extra": extra, "key": key, "env": env, "default": default})
    NS["_get_scoped_secret"] = rec(
        ctx, "get_scoped_secret", lambda name, default=None, **k: {"name": name, "default": default})
    NS["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", lambda: {}))


def mk_self(func, args, state_in, ctx):
    attrs = args.get("self") or {}
    slf = types.SimpleNamespace()
    for k, v in attrs.items():
        if k == "_client":
            setattr(slf, k, client_double(v, ctx))
        else:
            setattr(slf, k, pyval(v))
    # state stores (only keys present in the fixture state)
    for k, v in (state_in or {}).items():
        if k == "_joined_rooms":
            setattr(slf, k, set(v or []))
        elif k == "_dm_rooms":
            setattr(slf, k, dict(v or {}))
        else:
            setattr(slf, k, pyval(v))
    for k, v in NS["__class_consts__"].items():
        setattr(slf, k, v)
    # bind the target + composed ported siblings REAL
    bind = [func] + COMPOSED.get(func, [])
    for m in bind:
        if m in NS and m not in STATIC_METHODS:
            setattr(slf, m, types.MethodType(NS[m], slf))
    # install seamed siblings as instance-attr recorders (shadow the class)
    for nm in SEAMED_SIBLINGS.get(func, []):
        afn, is_async, build = SIBLING_SPECS[nm]
        setattr(slf, nm, rec(ctx, "self." + nm, afn, build, is_async))
    return slf


def echo_state(func, slf, state_in):
    keys = set(state_in or {}) | WRITE_SET.get(func, set())
    out = {}
    for k in keys:
        if not hasattr(slf, k):
            continue
        v = getattr(slf, k)
        if k == "_joined_rooms":
            out[k] = sorted(v)
        elif isinstance(v, (set, frozenset)):
            out[k] = sorted(v)
        else:
            out[k] = v
    return out


# _client_op needs the live ctx for its coro_factory recorder; set per drive
ctx_local = None


# ── per-case drive ──
def drive(func, inp):
    global ctx_local
    args = inp.get("args") or {}
    state_in = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    ctx_local = ctx
    install_global_seams(ctx)
    slf = None
    state = {}
    try:
        if func in STATIC_METHODS:
            cfg = types.SimpleNamespace(extra=args.get("extra"))
            if func == "_extra_truthy":
                ret = NS[func](cfg, args["key"], args["env_name"], args.get("default"))
            else:
                ret = NS[func](cfg)
        else:
            slf = mk_self(func, args, state_in, ctx)
            ret = _invoke(func, slf, args)
    except BaseException as exc:  # SystemExit / CancelledError included
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)
    # C emits the mutated state window EVEN ON A RAISE (partial history)
    if slf is not None:
        state = echo_state(func, slf, state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


def _invoke(func, slf, args):
    g = args.get
    if func == "_reset_clock_skew_detector":
        coro = slf._reset_clock_skew_detector()
    elif func == "_note_late_grace_drop":
        coro = slf._note_late_grace_drop(g("event_ts"))
    elif func == "_is_authorized_user":
        coro = slf._is_authorized_user(g("user_id"), g("room_id"))
    elif func == "_on_invite":
        coro = slf._on_invite(event_double(g("event")))
    elif func == "_schedule_pending_invite_joins":
        coro = slf._schedule_pending_invite_joins(g("sync_data"))
    elif func == "_extract_invite_dm_signal":
        coro = slf._extract_invite_dm_signal(g("invited_room"))
    elif func == "_join_room_by_id":
        coro = slf._join_room_by_id(g("room_id"))
    elif func == "_fetch_m_direct":
        coro = slf._fetch_m_direct(log_failure=g("log_failure") or False,
                                   require_dict=g("require_dict") or False)
    elif func == "_refresh_dm_cache":
        coro = slf._refresh_dm_cache()
    elif func == "_record_dm_room":
        coro = slf._record_dm_room(g("room_id"), g("inviter"))
    elif func == "_is_dm_room":
        coro = slf._is_dm_room(g("room_id"))
    elif func == "_send_reaction":
        coro = slf._send_reaction(g("room_id"), g("event_id"), g("emoji"))
    elif func == "send_read_receipt":
        coro = slf.send_read_receipt(g("room_id"), g("event_id"))
    elif func == "_client_op":
        cf = rec(ctx_local, "coro_factory", lambda: {}, None, is_async=True)
        coro = slf._client_op(cf, tuple(g("ok_msg") or []), g("err_msg") or "",
                              level=g("level") or "warning")
    elif func == "redact_message":
        coro = slf.redact_message(g("room_id"), g("event_id"), g("reason") or "")
    elif func == "invite_user":
        coro = slf.invite_user(g("room_id"), g("user_id"))
    elif func == "set_presence":
        coro = slf.set_presence(g("state") or "online", g("status_msg") or "")
    elif func == "_get_room_member_count":
        coro = slf._get_room_member_count(g("room_id"))
    elif func == "_get_room_state_value":
        coro = slf._get_room_state_value(g("room_id"), g("event_type"), g("key"))
    elif func == "_get_display_name":
        coro = slf._get_display_name(g("room_id"), g("user_id"))
    elif func == "_set_typing":
        coro = slf._set_typing(g("chat_id"), g("timeout"))
    elif func == "send_typing":
        coro = slf.send_typing(g("chat_id"), g("metadata"))
    elif func == "stop_typing":
        coro = slf.stop_typing(g("chat_id"))
    else:
        raise RuntimeError(f"unknown func {func}")
    if func in ASYNC_FUNCS:
        return asyncio.run(coro)
    return coro


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "extra_or_secret", "get_scoped_secret", "time.time",
    "self._is_sender_authorized", "self._is_authorized_user",
    "self._schedule_invite_join", "self._extract_invite_dm_signal",
    "self._invalidate_room_identities", "self._refresh_dm_cache",
    "self._fetch_m_direct", "self._resolve_room_identity", "self._state_event_value",
    "client.join_room", "client.leave_room", "client.send_message_event",
    "client.set_fully_read_marker", "client.send_receipt", "client.set_read_markers",
    "client.get_account_data", "client.set_account_data", "client.redact",
    "client.invite_user", "client.set_presence", "client.set_typing",
    "client.get_state_event", "client.joined_members",
    "state_store.get_members", "state_store.get_member", "coro_factory",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_matrix_adapter_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', open(__file__, encoding="utf-8").read()))
    used = {n for n in used if not n.endswith(".")}  # drop "client." + k concatenation prefixes
    used |= {"self." + k for k in SIBLING_SPECS}
    used |= {"client." + k for k in CLIENT_METHODS}
    used |= {"state_store." + k for k in SS_METHODS}
    used |= {"coro_factory"}
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow2: seam registry drift: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_matrix_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE matrix_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow2.jsonl"
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
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        # flag AttributeError naming a ported method/seam (binding bug signature)
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in METHOD_FUNCS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING {func} case {n}: {msg}")
    print(f"SELFTEST matrix_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    low = [f for f, c in counts.items() if c < 5]
    if low:
        print("WARNING funcs with <5 cases:", low)
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()