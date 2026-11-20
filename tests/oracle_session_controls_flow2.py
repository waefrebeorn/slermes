#!/usr/bin/env python3
"""Oracle for the session-controls FLOW remainder port
(src/gateway/session_controls_flow2.c).

Drives the LIVE gateway/session_controls.py decision chains with SCRIPTED
seams: every authority verb, store read, title validator, listing
collaborator, mutation engine, server-request registry, transport binding,
task scheduler and skin resolver is a recording stub fed the same scripts the
C harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered
seam events {"name","args","ret"} in "trace") plus the final return ("ret")
and the mutated connection/world state ("state").

The live defs are AST-extracted from gateway/session_controls.py (module
constants seeded; the AuthorityConnection ClassDef exec whole). Claimed
siblings ported in session_controls_flow.c (self.ping, _voice_stop_replay,
_typed_voice_stop, _record_voice_stop, submission) are SEAMED here, never
re-ported; unclaimed-but-unported callees (self._create, self.resume,
sibling.close) are seamed too. Support dataclasses/exceptions are the REAL
ones AST-extracted from session_contract / persistence.core.errors. Every exec
namespace is a registered sys.modules module (py3.14 dataclasses _is_type needs
cls.__module__ resolvable). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE session_controls_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}
  (AuthorityRefusal adds reason/field/detail; mutate's fence adds revision /
  execution_generation only when the live handler sets them).
- ref args project to {profile_id, session_id}; actor args to
  {subject, profile_id, capabilities(SORTED), transport_id}.
- __init__ returns the constructed connection's projection. host_scoped reads
  state.identity. close iterates handed_back sorted (fixtures keep <=1 id).
- the resolve seam scripts an "asks" list driving the entitled closure
  (may_settle for self then siblings, any() short-circuit) before the resolve
  event records.
- asyncio.to_thread runs synchronously; locks / owner_scope are no-ops; logger
  calls silent; create_task schedules without running (one seam event);
  task.add_done_callback silent.
- bare array script == per-call queue (last does NOT repeat, exhausted is
  fail-closed); list results need {"__script__": true, "ret": [...]}.
- ASCII only; integral floats == ints; `is True` gates JSON_BOOL-only.
"""
import ast
import enum
import json
import os
import re
import sys
import textwrap
import types
import asyncio as _asyncio
from dataclasses import asdict, is_dataclass
from functools import partial
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SC_SRC = os.path.join(Q, "gateway", "session_controls.py")
CONTRACT_SRC = os.path.join(Q, "gateway", "session_contract.py")
ERRORS_SRC = os.path.join(Q, "persistence", "core", "errors.py")
POLICY_SRC = os.path.join(Q, "gateway", "session_policy.py")
CONV_SESSIONS_SRC = os.path.join(Q, "persistence", "conversation", "sessions.py")

import __future__ as _fut
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


def class_nodes(src, names=None):
    return [n for n in ast.parse(src).body
            if isinstance(n, ast.ClassDef) and (names is None or n.name in names)]


def func_nodes(src):
    return [n for n in ast.parse(src).body
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]


def extract_assign(path, name):
    src = open(path, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            ns = {}
            exec_node(src, node, ns, path)
            return ns[name]
    print(f"FATAL oracle_session_controls_flow2: constant {name} not found in {path}")
    sys.exit(2)


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── support namespace: real contract dataclasses + errors taxonomy ──
def extract_support():
    mod = new_module("sct2_oracle_support")
    ns = mod.__dict__
    csrc = open(CONTRACT_SRC, encoding="utf-8").read()
    cmod = new_module("sct2_oracle_contract")
    exec(compile(csrc, CONTRACT_SRC, "exec", flags=_FLAGS, dont_inherit=True), cmod.__dict__)
    for name in ("SessionActor", "SessionRef", "Submission", "AdmissionReceipt",
                 "SessionHandle", "CANONICAL_GATEWAY_PROTOCOL"):
        ns[name] = getattr(cmod, name)
    esrc = open(ERRORS_SRC, encoding="utf-8").read()
    ens = {"__name__": "sct2_oracle_errors"}
    new_module("sct2_oracle_errors").__dict__.update(ens)
    import typing
    ens.update({"Optional": typing.Optional, "Literal": typing.Literal,
                "Enum": enum.Enum, "object": object})
    for node in func_nodes(esrc):
        if node.name in ("_one_line", "busy_refusal"):
            exec_node(esrc, node, ens, ERRORS_SRC)
    for node in class_nodes(esrc):
        exec_node(esrc, node, ens, ERRORS_SRC)
    seed_constants(esrc, ens, ERRORS_SRC)
    for name in ("AuthorityRefusal", "PersistenceError", "STORE_FAILURES",
                 "MUTATION_GUARD_REFUSALS", "busy_refusal", "TurnLeaseLost",
                 "CompressionInProgress", "TransientStoreError", "PermanentStoreError",
                 "StoreUnavailable", "StoreReplaced", "CorruptStoreError"):
        ns[name] = ens[name]
    required = ["SessionActor", "SessionRef", "Submission", "AdmissionReceipt",
                "AuthorityRefusal", "MUTATION_GUARD_REFUSALS", "busy_refusal",
                "TurnLeaseLost", "CompressionInProgress", "CANONICAL_GATEWAY_PROTOCOL"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_controls_flow2: support missing: {missing}")
        sys.exit(2)
    return ns


SUP = extract_support()
CREATE_FIELDS = extract_assign(POLICY_SRC, "CREATE_FIELDS")
SURFACES = extract_assign(POLICY_SRC, "SURFACES")
INTERNAL_LISTING_SOURCES = extract_assign(CONV_SESSIONS_SRC, "INTERNAL_LISTING_SOURCES")


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (frozenset, set)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "ImportError": ImportError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "AuthorityRefusal": SUP["AuthorityRefusal"],
    "TurnLeaseLost": SUP["TurnLeaseLost"], "CompressionInProgress": SUP["CompressionInProgress"],
    "StoreUnavailable": SUP["StoreUnavailable"], "StoreReplaced": SUP["StoreReplaced"],
    "CorruptStoreError": SUP["CorruptStoreError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "AuthorityRefusal":
        return SUP["AuthorityRefusal"](spec.get("reason", ""),
                                       field=spec.get("field"), detail=spec.get("detail"))
    if kind in ("TurnLeaseLost", "CompressionInProgress"):
        return EXC_CLASSES[kind](spec.get("session_id", ""))
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, SUP["AuthorityRefusal"]):
        d["reason"] = exc.reason
        d["field"] = exc.field
        d["detail"] = exc.detail
        rev = getattr(exc, "revision", None)
        gen = getattr(exc, "execution_generation", None)
        if rev is not None:
            d["revision"] = rev
        if gen is not None:
            d["execution_generation"] = gen
    return d


# ── per-case script backend (mirrors the C sct2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def peek(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            return entry[idx]
        return entry

    def call(self, name, args):
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
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def arec(ctx, name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── doubles ──
class NoopCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class _NoopLock:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False


async def _to_thread(fn, *a, **k):
    return fn(*a, **k)


class TaskDouble:
    def __init__(self, id_):
        self.id = id_

    def add_done_callback(self, cb):
        return None


class CoroDouble:
    def __init__(self, args):
        self.args = args

    def close(self):
        return None


def make_actor(d):
    return SUP["SessionActor"](subject=d["subject"], profile_id=d["profile_id"],
                               capabilities=frozenset(d.get("capabilities") or []),
                               transport_id=d["transport_id"])


def build_receipt(d):
    if d is None:
        return None
    return SUP["AdmissionReceipt"](
        admission_id=d["admission_id"], ref=SUP["SessionRef"](**d["ref"]),
        sequence=d["sequence"], status=d["status"], outcome=d["outcome"],
        authority_epoch=d["authority_epoch"], execution_generation=d["execution_generation"])


def build_submission(d):
    if d is None:
        return None
    return SUP["Submission"](request_id=d["request_id"], ref=SUP["SessionRef"](**d["ref"]),
                             payload=d["payload"], intent=d["intent"])


def build_row(d):
    return None if d is None else types.SimpleNamespace(**d)


def build_ref(d):
    return None if d is None else SUP["SessionRef"](**d)


def build_entries(rows):
    out = []
    for e in rows or []:
        out.append(types.SimpleNamespace(
            session_id=e["session_id"], row=e["row"], closed=e["closed"],
            handle=types.SimpleNamespace(execution_state=e["handle"]["execution_state"])))
    return out


# ── session_controls namespace ──
def extract_sc():
    mod = new_module("sct2_oracle_sc")
    ns = mod.__dict__
    import sqlite3
    import logging as _logging
    import uuid as _uuid
    ns.update({"asyncio": None, "re": re, "logging": _logging, "sqlite3": sqlite3,
               "asdict": asdict, "partial": partial, "Path": Path, "uuid": _uuid,
               "logger": _SilentLogger(), "types": types})
    for name in ("SessionActor", "SessionRef", "Submission", "AdmissionReceipt",
                 "CANONICAL_GATEWAY_PROTOCOL", "AuthorityRefusal", "PersistenceError",
                 "STORE_FAILURES", "MUTATION_GUARD_REFUSALS", "busy_refusal"):
        ns[name] = SUP[name]
    ns["authority_persistence"] = lambda a: None
    src = open(SC_SRC, encoding="utf-8").read()
    seed_constants(src, ns, SC_SRC)
    for node in func_nodes(src):
        exec_node(src, node, ns, SC_SRC)
    for node in class_nodes(src, ["AuthorityConnection"]):
        exec_node(src, node, ns, SC_SRC)
    seed_constants(src, ns, SC_SRC)
    required = ["AuthorityConnection", "_resume_transcript", "_PROFILE_IMPLICIT", "_END_REASON"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_controls_flow2: session_controls missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_sc()

# In-function `import logging` resolves through sys.modules; keep it silent.
_logstub = types.ModuleType("logging")
_logstub.getLogger = lambda name=None: _SilentLogger()
sys.modules["logging"] = _logstub
NS["logging"] = _logstub


def stub_modules(mapping):
    for dotted, attrs in mapping.items():
        parts = dotted.split(".")
        for i in range(1, len(parts) + 1):
            name = ".".join(parts[:i])
            if name not in sys.modules:
                m = types.ModuleType(name)
                m.__path__ = []
                sys.modules[name] = m
            if i > 1:
                setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[name])
        mod = sys.modules[dotted]
        for k, v in attrs.items():
            setattr(mod, k, v)


# ── per-case setup ──
AUTH_AID = {}
AUTH_METHODS = {"resolve", "submit", "receipt", "respond", "resolve_unknown",
                "detach", "unless_deleted"}


def aid_of(a):
    return AUTH_AID.get(id(a))


def make_resolve(ctx):
    def resolve(frame, *, session_ok=None):
        entry = ctx.peek("resolve")
        asks = entry.get("asks") if isinstance(entry, dict) else None
        if isinstance(asks, list):
            for ask in asks:
                if session_ok is not None:
                    session_ok(ask.get("session_id"), ask.get("events"))
        return ctx.call("resolve", {"frame": canon(frame)})
    return resolve


class Setup:
    pass


def setup_case(args, state, ctx):
    AUTH_AID.clear()
    s = Setup()
    s.ctx = ctx
    auth_doubles = {}
    for aid, spec in (state.get("authorities") or {}).items():
        spec = dict(spec)
        lock = spec.pop("_titled_create_lock", None)
        auth = types.SimpleNamespace(**spec)
        if lock is not None:
            auth._titled_create_lock = _NoopLock()
        auth.events = dict(state.get("events") or {}) if aid == state.get("authority") else {}
        auth.sessions = ({sid: types.SimpleNamespace(**sp)
                          for sid, sp in (state.get("sessions") or {}).items()}
                         if aid == state.get("authority") else {})
        AUTH_AID[id(auth)] = aid
        auth_doubles[aid] = auth
    s.auth_doubles = auth_doubles
    s.auth = auth_doubles.get(state.get("authority"))

    # authority verb seams
    if s.auth is not None:
        s.auth.resolve = arec(ctx, "authority.resolve",
                              args_fn=lambda actor, ref: {"actor": canon(actor), "ref": canon(ref)})
        s.auth.submit = arec(ctx, "authority.submit",
                             args_fn=lambda actor, submission, queue_while_restarting=False: {
                                 "actor": canon(actor), "submission": canon(submission),
                                 "queue_while_restarting": queue_while_restarting},
                             build=build_receipt)
        s.auth.receipt = arec(ctx, "authority.receipt",
                              args_fn=lambda actor, ref, admission_id: {
                                  "actor": canon(actor), "ref": canon(ref),
                                  "admission_id": admission_id},
                              build=build_receipt)
        s.auth.respond = arec(ctx, "authority.respond",
                              args_fn=lambda actor, ref, gen, prompt_id, payload, kind=None: {
                                  "actor": canon(actor), "ref": canon(ref),
                                  "execution_generation": gen, "prompt_id": prompt_id,
                                  "payload": canon(payload), "kind": kind})
        s.auth.resolve_unknown = arec(ctx, "authority.resolve_unknown",
                                      args_fn=lambda actor, ref, aid_, gen: {
                                          "actor": canon(actor), "ref": canon(ref),
                                          "admission_id": aid_, "execution_generation": gen},
                                      build=build_receipt)
        s.auth.detach = arec(ctx, "authority.detach",
                             args_fn=lambda actor, subscription, orphaned=False: {
                                 "actor": canon(actor), "subscription_id": subscription,
                                 "orphaned": orphaned})
        s.auth.unless_deleted = rec(ctx, "authority.unless_deleted",
                                    args_fn=lambda ref, fn: {"ref": canon(ref),
                                                             "transcript_target": fn.args[1]},
                                    build=lambda r: tuple(r) if r is not None else None)

    store = types.SimpleNamespace(sessions=types.SimpleNamespace(
        get=rec(ctx, "store.sessions.get", args_fn=lambda session_id: {"session_id": session_id},
                build=build_row),
        resolve_by_title=rec(ctx, "store.sessions.resolve_by_title",
                             args_fn=lambda title: {"title": title})))
    s.store = store
    NS["authority_persistence"] = lambda a: store

    # asyncio double (per-case loop bound to ctx)
    loop = types.SimpleNamespace(
        create_task=lambda coro: TaskDouble(ctx.call("create_task", coro.args)))
    NS["asyncio"] = types.SimpleNamespace(to_thread=_to_thread, Lock=_NoopLock,
                                          get_running_loop=lambda: loop)
    NS["uuid"] = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex=ctx.call("uuid4_hex", {})))

    stub_modules({
        "gateway.owner_identity": {
            "principal_subject": rec(ctx, "principal_subject",
                                     args_fn=lambda identity: {"identity": canon(identity)})},
        "gateway.session_local_migration": {
            "bind_native_transport": rec(ctx, "bind_native_transport",
                                         args_fn=lambda a, actor, identity: {
                                             "authority": aid_of(a), "actor": canon(actor),
                                             "identity": canon(identity)}),
            "unbind_native_transport": rec(ctx, "unbind_native_transport",
                                           args_fn=lambda a, actor: {
                                               "authority": aid_of(a), "actor": canon(actor)})},
        "gateway.session_local_title": {
            "validate_title": rec(ctx, "validate_title", args_fn=lambda title: {"title": title}),
            "resolve_titled_session": rec(ctx, "resolve_titled_session",
                                          args_fn=lambda a, actor, title, missing_ok=False: {
                                              "authority": aid_of(a), "actor": canon(actor),
                                              "title": title, "missing_ok": missing_ok},
                                          build=build_ref)},
        "gateway.session_local_inventory": {
            "snapshot_info": arec(ctx, "snapshot_info",
                                  args_fn=lambda a, ref: {"authority": aid_of(a), "ref": canon(ref)})},
        "gateway.session_listing": {
            "check_page_params": rec(ctx, "check_page_params",
                                     args_fn=lambda params: {"params": canon(params)}),
            "stored_picker_sessions": rec(ctx, "stored_picker_sessions",
                                          args_fn=lambda a, actor, *, limit, include_hidden,
                                          workspace, picker, query, cursor: {
                                              "authority": aid_of(a), "actor": canon(actor),
                                              "limit": limit, "include_hidden": include_hidden,
                                              "workspace": workspace, "picker": picker,
                                              "query": query, "cursor": cursor})},
        "gateway.session_live_listing": {
            "live_entries": arec(ctx, "live_entries",
                                 args_fn=lambda a, actor: {"authority": aid_of(a), "actor": canon(actor)},
                                 build=build_entries)},
        "gateway.session_picker_fields": {
            "live_picker_page": rec(ctx, "live_picker_page",
                                    args_fn=lambda a, sessions, limit, workspace, picker: {
                                        "authority": aid_of(a), "sessions": canon(sessions),
                                        "limit": limit, "workspace": workspace, "picker": picker})},
        "persistence.conversation.sessions": {
            "INTERNAL_LISTING_SOURCES": INTERNAL_LISTING_SOURCES},
        "gateway.session_policy": {"CREATE_FIELDS": CREATE_FIELDS, "SURFACES": SURFACES},
        "gateway.session_skin": {
            "resolved_skin": rec(ctx, "resolved_skin", args_fn=lambda: {})},
        "gateway.session_mutations": {
            "mutate_session": arec(ctx, "mutate_session",
                                   args_fn=lambda a, actor, ref, params: {
                                       "authority": aid_of(a), "actor": canon(actor),
                                       "ref": canon(ref), "params": canon(params)})},
        "persistence.core.errors": {
            "MUTATION_GUARD_REFUSALS": SUP["MUTATION_GUARD_REFUSALS"],
            "busy_refusal": SUP["busy_refusal"]},
        "gateway.session_results": {
            "admission_result": rec(ctx, "admission_result",
                                    args_fn=lambda store_, aid_: {"admission_id": aid_})},
        "gateway.session_server_requests": {
            "may_settle": rec(ctx, "may_settle",
                              args_fn=lambda a, actor, sid, events: {
                                  "authority": aid_of(a), "actor": canon(actor),
                                  "session_id": sid, "events": canon(events)}),
            "resolve": make_resolve(ctx)},
        "gateway.session_llm_oneshot": {
            "forget": rec(ctx, "forget", args_fn=lambda conn: {})},
        "gateway.session_authorities": {"owner_scope": lambda a: NoopCM()},
        "gateway.session_viewer_close": {
            "arm_detached_orphan_close": arec(ctx, "arm_detached_orphan_close",
                                              args_fn=lambda a, sid: {
                                                  "authority": aid_of(a), "session_id": sid})},
        "gateway.run_housekeeping_disk": {
            "publish_local_auto_prune_notice": lambda a, ref: CoroDouble(
                {"authority": aid_of(a), "ref": canon(ref)})},
        "tui_gateway.tool_progress": {
            "_todo_state_from_history": rec(ctx, "_todo_state_from_history",
                                            args_fn=lambda history: {"history": canon(history)})},
        "gateway.session_local_recovery": {"omitted_history_state": lambda *a, **k: None},
    })

    # connection double is built by drive() (None for __init__).
    s.conn = None
    return s


def mk_conn(state, ctx, s):
    cls = NS["AuthorityConnection"]
    conn = cls.__new__(cls)
    conn.authority = s.auth
    conn.transport = state.get("transport")
    conn.actor = make_actor(state["actor"])
    conn.subscriptions = dict(state.get("subscriptions") or {})
    conn.replay_cursors = {sid: tuple(v) for sid, v in (state.get("replay_cursors") or {}).items()}
    conn.reconnects = set(state.get("reconnects") or [])
    conn.handed_back = set(state.get("handed_back") or [])
    conn._identity = state.get("identity") or {}
    conn._operator = state.get("operator", False)
    conn.native_owner = state.get("native_owner", False)
    if "notice_tasks" in state:
        conn._notice_tasks = {TaskDouble(t) for t in state["notice_tasks"]}
    conn._siblings = {}
    for pid, spec in (state.get("siblings") or {}).items():
        sib = cls.__new__(cls)
        sib.authority = s.auth_doubles.get(spec["authority"])
        sib.actor = make_actor(spec["actor"])
        sib.close = arec(ctx, "sibling.close", args_fn=lambda: {"authority": spec["authority"]})
        conn._siblings[pid] = sib
    # claimed siblings + unclaimed callees -> recording seams
    conn.ping = arec(ctx, "self.ping",
                     args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)})
    conn._voice_stop_replay = rec(ctx, "self._voice_stop_replay",
                                  args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)})
    conn._typed_voice_stop = arec(ctx, "self._typed_voice_stop",
                                  args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)})
    conn._record_voice_stop = rec(ctx, "self._record_voice_stop",
                                  args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)})
    conn.submission = rec(ctx, "self.submission",
                          args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)},
                          build=build_submission)
    conn._create = arec(ctx, "self._create",
                        args_fn=lambda params, title: {"params": canon(params), "title": title})
    conn.resume = arec(ctx, "self.resume",
                       args_fn=lambda ref, params: {"ref": canon(ref), "params": canon(params)})
    return conn


def project_connection(conn):
    return {
        "authority": aid_of(conn.authority), "transport": conn.transport,
        "actor": canon(conn.actor), "native_owner": conn.native_owner,
        "identity": conn._identity, "operator": conn._operator,
        "subscriptions": {}, "replay_cursors": {}, "reconnects": [],
        "handed_back": [], "siblings": {},
    }


def project_authorities(auth_doubles):
    out = {}
    for aid, auth in auth_doubles.items():
        d = {}
        for k, v in vars(auth).items():
            if k in AUTH_METHODS or k in ("events", "sessions"):
                continue
            d[k] = True if isinstance(v, _NoopLock) else canon(v)
        out[aid] = d
    return out


def project_state(state, s):
    final = canon(state)
    conn = s.conn
    if conn is not None:
        final["subscriptions"] = dict(conn.subscriptions)
        final["replay_cursors"] = {sid: list(v) for sid, v in conn.replay_cursors.items()}
        final["reconnects"] = sorted(conn.reconnects)
        final["handed_back"] = sorted(conn.handed_back)
        final["siblings"] = {pid: {"authority": aid_of(sib.authority), "actor": canon(sib.actor)}
                             for pid, sib in conn._siblings.items()}
        if hasattr(conn, "_notice_tasks"):
            final["notice_tasks"] = sorted(t.id for t in conn._notice_tasks)
    final["authorities"] = project_authorities(s.auth_doubles)
    if s.auth is not None:
        final["sessions"] = {sid: canon(vars(l)) for sid, l in s.auth.sessions.items()}
        final["events"] = dict(s.auth.events)
    return final


PORTED = ["__init__", "host_scoped", "_voice_stop_key", "_resume_transcript", "create",
          "describe", "info", "list_sessions", "submit", "mutate", "receipt",
          "resolve_unknown", "respond", "respond_clarify", "_resolve_response",
          "close", "_spawn_notice"]


def invoke(func, args, s, ctx):
    ref = lambda: SUP["SessionRef"](**args["ref"])
    params = lambda: dict(args.get("params") or {})
    if func == "__init__":
        auth = s.auth_doubles[args["authority"]]
        conn = NS["AuthorityConnection"](auth, args.get("transport"), args["identity"],
                                         operator=args.get("operator", False))
        return project_connection(conn)
    conn = s.conn
    if func == "host_scoped":
        return conn.host_scoped
    if func == "_voice_stop_key":
        return conn._voice_stop_key(ref(), params())
    if func == "_resume_transcript":
        snapshot = types.SimpleNamespace(**(args.get("snapshot") or {}))
        return _asyncio.run(NS["_resume_transcript"](s.auth, ref(), snapshot,
                                                     args.get("history"), args.get("omit")))
    if func == "create":
        return _asyncio.run(conn.create(ref(), params()))
    if func == "describe":
        return _asyncio.run(conn.describe(ref(), params()))
    if func == "info":
        return _asyncio.run(conn.info(ref(), params()))
    if func == "list_sessions":
        return _asyncio.run(conn.list_sessions(ref(), params()))
    if func == "submit":
        return _asyncio.run(conn.submit(ref(), params()))
    if func == "mutate":
        return _asyncio.run(conn.mutate(ref(), params()))
    if func == "receipt":
        return _asyncio.run(conn.receipt(ref(), params()))
    if func == "resolve_unknown":
        return _asyncio.run(conn.resolve_unknown(ref(), params()))
    if func == "respond":
        return _asyncio.run(conn.respond(ref(), params()))
    if func == "respond_clarify":
        return _asyncio.run(conn.respond_clarify(ref(), params()))
    if func == "_resolve_response":
        return conn._resolve_response(args["frame"])
    if func == "close":
        return _asyncio.run(conn.close())
    if func == "_spawn_notice":
        return conn._spawn_notice(ref())
    raise RuntimeError(f"unknown session_controls func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    s = setup_case(args, state, ctx)
    s.state = state
    if func != "__init__":
        s.conn = mk_conn(state, ctx, s)
    try:
        ret = invoke(func, args, s, ctx)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    final = project_state(state, s)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(final)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "principal_subject", "uuid4_hex", "bind_native_transport", "self.ping", "resolved_skin",
    "authority.resolve", "snapshot_info", "check_page_params", "stored_picker_sessions",
    "resolve_titled_session", "store.sessions.get", "store.sessions.resolve_by_title",
    "live_entries", "live_picker_page", "self._voice_stop_replay", "self._typed_voice_stop",
    "self._record_voice_stop", "self.submission", "authority.submit", "mutate_session",
    "self.resume", "authority.receipt", "admission_result", "authority.resolve_unknown",
    "authority.respond", "may_settle", "resolve", "sibling.close", "forget",
    "authority.detach", "arm_detached_orphan_close", "unbind_native_transport",
    "create_task", "_todo_state_from_history", "authority.unless_deleted",
    "validate_title", "self._create",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_controls_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_used = set(re.findall(r'sct2_call\(\s*c,\s*"([^"]+)"', csrc))
    missing_c = sorted(c_used - registry)
    if missing_c:
        print(f"FATAL oracle_session_controls_flow2: C seams outside registry: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|arec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_session_controls_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_controls_flow2.jsonl"
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
            print(f"FATAL oracle_session_controls_flow2: fixture io keys without a seam: {sorted(bad)}")
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
            rec_ = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_.get("func", "")
        inp = rec_.get("in") or {}
        got = rec_.get("out")
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
    print(f"ORACLE session_controls_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()