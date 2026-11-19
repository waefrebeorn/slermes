#!/usr/bin/env python3
"""Oracle for the gateway/session_controls.py control/interrupt FLOW cores
(src/gateway/session_controls_flow.c).

AST-extracts the live module helpers and the ENTIRE AuthorityConnection class
(future-annotations compile, so no heavy gateway import), seeds the module
constants (Assign + AnnAssign), the real persistence.core.errors taxonomy, the
gateway/session_contract.py dataclasses (SessionActor / SessionRef /
SessionHandle / Submission / AdmissionReceipt), the live Platform enum members
and authority_persistence, and replaces every unported collaborator (profile
helpers, sibling construction, authorize/detach/receipt/interrupt authority
verbs, stream replay, voice gates, submit field mergers, logging) with
recording stubs returning the SAME scripted results the C harness feeds its
universal seam.

State is injected as a JSON mirror; the oracle materializes per-id object
doubles so live `is` comparisons and in-place mutations agree with the C
mirror, then norms the final state back to JSON. Compares ordered seam trace +
return + final state. BaseException is caught per case (scripted
SystemExit/CancelledError propagate through live `except AuthorityRefusal`
exactly as in the port).

Input: stdin JSON lines from t_port_session_controls_flow.
Summary: ORACLE session_controls_flow: N cases, M mismatches. Fail-closed on 0
cases.
"""
import ast
import asyncio
import builtins
import contextlib
import json
import os
import re
import sqlite3
import sys
import types
import uuid
from collections import OrderedDict
from dataclasses import asdict, dataclass, field
from enum import Enum
from functools import partial
from pathlib import Path
from types import SimpleNamespace
from typing import Literal, Mapping

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_controls.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")
CONTRACT = os.path.join(QUARRY, "gateway", "session_contract.py")
CONFIG = os.path.join(QUARRY, "gateway", "config.py")
PERSIST = os.path.join(QUARRY, "gateway", "session_persistence.py")
COMPRESSOR = os.path.join(QUARRY, "agent", "context_compressor.py")

WANTED_MOD = {
    "_refusal_data", "_profile_label", "_seen_storage_refusal", "_log_refusal",
    "_log_storage_failure", "model_only", "display_projection",
}
ERROR_CLASSES = [
    "PersistenceError", "Conflict", "_SessionConflict", "CompressionBusy",
    "CompressionInProgress", "TurnLeaseLost", "AuthorityRefusal", "_one_line",
    "TransientStoreError", "PermanentStoreError", "StoreUnavailable",
    "StoreReplaced",
]
CONTRACT_CLASSES = [
    "SessionActor", "SessionRef", "SessionHandle", "Submission", "AdmissionReceipt",
]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "conn": None,
       "auths": {}}
_NOEI = object()


def rec(ev):
    CUR["trace"].append(ev)


class RecLogger:
    def _rec(self, level, fmt, args, exc_info=_NOEI):
        ev = {"ev": "log", "level": level, "fmt": fmt,
              "args": [str(a) if isinstance(a, BaseException) else a for a in args]}
        if exc_info is not _NOEI:
            ev["exc_info"] = exc_info
        rec(ev)

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a, k.get("exc_info", _NOEI))

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a, k.get("exc_info", _NOEI))

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a, k.get("exc_info", _NOEI))

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a, k.get("exc_info", _NOEI))


# ── script access (mirrors the harness stub semantics) ─────────────────────
def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "AuthorityRefusal":
        return CUR["ns"]["AuthorityRefusal"](msg, field=spec.get("field"),
                                             detail=spec.get("detail"))
    cls = getattr(builtins, kind, None)
    if cls is None and kind == "CancelledError":
        cls = asyncio.CancelledError
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def next_script(name):
    io = CUR["io"]
    if name not in io:
        return None
    v = io[name]
    if isinstance(v, list):
        if not v:
            return None
        i = CUR["pos"].get(name, 0)
        if i < len(v):
            CUR["pos"][name] = i + 1
        return v[min(i, len(v) - 1)]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam_rec(name, args):
    r = next_script(name)
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def raise_desc(e):
    d = {"__raise__": type(e).__name__, "message": str(e)}
    if isinstance(e, CUR["ns"]["AuthorityRefusal"]):
        d["field"] = e.field
        d["detail"] = e.detail
    return d


# ── extraction ──────────────────────────────────────────────────────────────
def _exec_nodes(nodes, src, ns):
    for node in nodes:
        exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)


def seed_errors(ns):
    tree = ast.parse(open(ERRORS, encoding="utf-8").read())
    by_name = {}
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            by_name[node.name] = node
    _exec_nodes([by_name[n] for n in ERROR_CLASSES], ERRORS, ns)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in ("STORE_FAILURES", "MUTATION_GUARD_REFUSALS")
                for t in node.targets):
            with contextlib.suppress(Exception):
                _exec_nodes([node], ERRORS, ns)
    ns["busy_refusal"] = lambda exc: ns["AuthorityRefusal"](
        "session_busy",
        field="compression" if isinstance(exc, ns["CompressionInProgress"]) else None)


def seed_contract(ns):
    tree = ast.parse(open(CONTRACT, encoding="utf-8").read())
    ns["JsonObject"] = Mapping[str, object]
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "CANONICAL_GATEWAY_PROTOCOL"
                for t in node.targets):
            _exec_nodes([node], CONTRACT, ns)
    by_name = {n.name: n for n in tree.body if isinstance(n, ast.ClassDef)}
    _exec_nodes([by_name[n] for n in CONTRACT_CLASSES], CONTRACT, ns)


def seed_platform(ns):
    """Live Platform member VALUES without the heavy plugin-registry methods."""
    tree = ast.parse(open(CONFIG, encoding="utf-8").read())
    pcls = next(n for n in tree.body
                if isinstance(n, ast.ClassDef) and n.name == "Platform")
    members = {}
    for sub in pcls.body:
        if isinstance(sub, ast.Assign) and all(isinstance(t, ast.Name) for t in sub.targets):
            with contextlib.suppress(Exception):
                _exec_nodes([sub], CONFIG, members)
    ns["Platform"] = Enum("Platform", {k: v for k, v in members.items()
                                       if isinstance(v, str)})


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("session_controls_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "session_controls_flow_extracted",
        "asyncio": asyncio, "re": re, "json": json, "os": os, "uuid": uuid,
        "sqlite3": sqlite3, "Path": Path, "partial": partial,
        "asdict": asdict, "dataclass": dataclass, "field": field,
        "Literal": Literal, "Mapping": Mapping, "Enum": Enum,
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "DeferredReply": None,
        "profile_name_for_home": lambda path: seam_rec(
            "profile_name_for_home", {"path": path}),
    })
    seed_errors(ns)
    seed_contract(ns)
    seed_platform(ns)
    # authority_persistence is pure (returns authority.persistence): live def.
    ptree = ast.parse(open(PERSIST, encoding="utf-8").read())
    for node in ptree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "authority_persistence":
            _exec_nodes([node], PERSIST, ns)
    ctree = ast.parse(open(COMPRESSOR, encoding="utf-8").read())
    for node in ctree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "MODEL_ONLY_DISPLAY_METADATA_KEY"
                for t in node.targets):
            _exec_nodes([node], COMPRESSOR, ns)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            with contextlib.suppress(Exception):
                _exec_nodes([node], SRC, ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                _exec_nodes([node], SRC, ns)
    # The logger Assign re-ran logging.getLogger: restore the recorder AFTER.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_MOD:
            _exec_nodes([node], SRC, ns)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "AuthorityConnection":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
    for name in list(WANTED_MOD) + ["AuthorityConnection", "_STORAGE_REFUSALS",
                                    "_STORAGE_TRACEBACKS", "_END_REASON",
                                    "authority_persistence", "Platform",
                                    "MODEL_ONLY_DISPLAY_METADATA_KEY"]:
        assert name in ns, "extraction missed %s" % name
    CUR["ns"] = ns
    CUR["cls"] = ns["AuthorityConnection"]   # real class (norm isinstance)
    return ns


def install_stubs(ns):
    """Function-local imports resolve at call time: stub modules in sys.modules."""
    def reg(name, attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        parts = name.split(".")
        for i in range(1, len(parts)):
            pname = ".".join(parts[:i])
            if pname not in sys.modules:
                sys.modules[pname] = types.ModuleType(pname)
        for i in range(1, len(parts)):
            setattr(sys.modules[".".join(parts[:i])], parts[i],
                    sys.modules[".".join(parts[:i + 1])])
        return m

    reg("hermes_cli.profiles", {
        "profile_matches_home": lambda name, home=None: seam_rec(
            "profile_matches_home", {"profile": name, "home": str(home)}),
    })
    reg("gateway.session_config", {
        "local_voice_enabled": lambda sid: seam_rec(
            "local_voice_enabled", {"session_id": sid}),
    })
    reg("tui_gateway.server", {
        "_history_to_messages": lambda rows, profile_home=None, image_urls=True: seam_rec(
            "history_to_messages",
            {"rows": rows, "profile_home": profile_home, "image_urls": image_urls}),
        "_consume_typed_stop_phrase": lambda text, sid, emit=False: seam_rec(
            "consume_typed_stop_phrase",
            {"text": text, "session_id": sid, "emit": emit}),
    })
    reg("gateway.session_finite", {
        "admit_finite": lambda params, submitter=None: seam_rec(
            "admit_finite",
            {"params": params,
             "ref": {"profile_id": submitter[1].profile_id,
                     "session_id": submitter[1].session_id}}),
    })
    reg("gateway.session_surface", {
        "submit_surface_fields": lambda params: seam_rec(
            "submit_surface_fields", {"params": params}),
    })
    reg("gateway.session_display", {
        "submit_display_fields": lambda params: seam_rec(
            "submit_display_fields", {"params": params}),
    })
    reg("gateway.session_cli_turn", {
        "submit_cli_turn_fields": lambda params: seam_rec(
            "submit_cli_turn_fields", {"params": params}),
    })
    reg("gateway.session_managed_worker", {
        "interrupt_managed": lambda authority, actor, ref, generation: seam_rec(
            "interrupt_managed",
            {"session_id": ref.session_id, "execution_generation": generation}),
    })
    reg("gateway.session_server_requests", {
        "open_requests": lambda stream, sid: seam_rec(
            "open_requests", {"session_id": sid}),
    })
    reg("agent.context_compressor", {
        "MODEL_ONLY_DISPLAY_METADATA_KEY": ns["MODEL_ONLY_DISPLAY_METADATA_KEY"],
    })
    reg("persistence.core.errors", {
        n: ns[n] for n in ERROR_CLASSES if n in ns
    } | {"STORE_FAILURES": ns.get("STORE_FAILURES"),
         "MUTATION_GUARD_REFUSALS": ns.get("MUTATION_GUARD_REFUSALS"),
         "busy_refusal": ns.get("busy_refusal")})


# ── object doubles ──────────────────────────────────────────────────────────
class AuthDouble:
    """Authority stand-in: fixture fields + scripted verb methods."""
    def __init__(self, aid, fields, sessions):
        self.aid = aid
        self.profile_id = fields.get("profile_id", "")
        self.instance_id = fields.get("instance_id", "")
        reg = fields.get("registry")
        self.runner = (SimpleNamespace(session_authorities=RegistryDouble(reg))
                       if reg is not None else None)
        self.sessions = sessions

        def admissions_get(admission_id=None):
            r = seam_rec("admissions_get", {"admission_id": admission_id})
            # live reads .status on the stored row: a mapping double, not dict
            return SimpleNamespace(**r) if isinstance(r, dict) else r

        self.persistence = SimpleNamespace(admissions=SimpleNamespace(
            get=admissions_get))

    def authorize(self, actor, ref, capability):
        return seam_rec("authorize",
                        {"capability": capability, "session_id": ref.session_id})

    async def detach(self, actor, subscription_id, end_reason=None):
        sid = (CUR["in"].get("params") or {}).get("session_id")
        r = seam_rec("detach", {"session_id": sid,
                                "subscription_id": subscription_id,
                                "end_reason": end_reason})
        if CUR["io"].get("detach_evicts") and CUR["conn"] is not None:
            CUR["conn"].subscriptions.pop(sid, None)
        return r

    async def receipt(self, actor, ref, admission_id):
        r = seam_rec("receipt", {"session_id": ref.session_id,
                                 "admission_id": admission_id})
        return make_receipt(r)

    async def cancel_queued(self, actor, ref, admission_id):
        r = seam_rec("cancel_queued", {"session_id": ref.session_id,
                                       "admission_id": admission_id})
        return make_receipt(r)

    async def cancel_or_claimed(self, actor, ref, admission_id):
        return seam_rec("cancel_or_claimed", {"session_id": ref.session_id,
                                              "admission_id": admission_id})

    def _handle(self, ref):
        return make_handle(seam_rec("handle", {"session_id": ref.session_id}))

    async def interrupt(self, actor, ref, generation):
        r = seam_rec("authority_interrupt",
                     {"session_id": ref.session_id,
                      "execution_generation": generation})
        return make_handle(r)


class RegistryDouble:
    def __init__(self, reg):
        self._ids = list(reg.get("members", []))

    def __iter__(self):
        # resolve lazily: the auth registry is still filling at construction
        return iter([CUR["auths"][m] for m in self._ids])

    def starting(self, profile):
        return seam_rec("registry_starting", {"profile": profile})


class StreamDouble:
    def since(self, epoch, sequence):
        return seam_rec("stream_since", {"epoch": epoch, "sequence": sequence})


class SessionDouble:
    def __init__(self, fields, platform_cls):
        src = fields.get("source")
        self.source = (SimpleNamespace(platform=platform_cls(src["platform"]))
                       if src is not None else None)
        self.event_stream = StreamDouble()


def make_receipt(r):
    ns = CUR["ns"]
    return ns["AdmissionReceipt"](
        admission_id=r["admission_id"], ref=ns["SessionRef"](**r["ref"]),
        sequence=r["sequence"], status=r["status"], outcome=r["outcome"],
        authority_epoch=r["authority_epoch"],
        execution_generation=r["execution_generation"])


def make_handle(r):
    ns = CUR["ns"]
    return ns["SessionHandle"](
        ref=ns["SessionRef"](**r["ref"]), instance_id=r["instance_id"],
        authority_epoch=r["authority_epoch"], revision=r["revision"],
        execution_generation=r["execution_generation"],
        execution_state=r["execution_state"])


def make_ar(spec):
    exc = CUR["ns"]["AuthorityRefusal"](
        spec.get("reason", ""), field=spec.get("field"), detail=spec.get("detail"))
    for k in ("revision", "execution_generation", "admitted"):
        if k in spec:
            setattr(exc, k, spec[k])
    return exc


def make_storage_exc(spec):
    kind = spec.get("kind", "Exception")
    msg = spec.get("message", "")
    cls = getattr(sqlite3, kind, None)
    if cls is None:
        cls = CUR["ns"].get(kind) or getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def new_conn(cls, authority, identity, operator):
    c = object.__new__(cls)
    c.authority = authority
    c._identity = dict(identity or {})
    c._operator = operator
    c._siblings = {}
    c.subscriptions = {}
    c.handed_back = set()
    c.reconnects = set()
    c.replay_cursors = {}
    return c


def materialize(ns, inp):
    st = inp.get("state") or {}
    auths = {}
    sessions = {}
    for sid, f in (st.get("sessions") or {}).items():
        sessions[sid] = SessionDouble(f, ns["Platform"])
    for aid, f in (st.get("authorities") or {}).items():
        auths[aid] = AuthDouble(aid, f, sessions)
    CUR["auths"] = auths
    # Module dedupe sets: seed from the mirror (tuples).
    ns["_STORAGE_REFUSALS"].clear()
    for k in st.get("storage_refusals") or []:
        ns["_STORAGE_REFUSALS"].add(tuple(k))
    ns["_STORAGE_TRACEBACKS"].clear()
    for k in st.get("storage_tracebacks") or []:
        ns["_STORAGE_TRACEBACKS"].add(tuple(k))
    cls = CUR["cls"]   # real class; ns["AuthorityConnection"] is the ctor seam
    sj = inp.get("self") or {}
    aid = sj.get("authority", "")
    conn = new_conn(cls, auths.get(aid), sj.get("identity"), sj.get("operator", False))
    actor = sj.get("actor") or {}
    conn.actor = ns["SessionActor"](
        actor.get("subject", ""), actor.get("profile_id", ""),
        frozenset(actor.get("capabilities") or []), "transport-1")
    conn.transport = SimpleNamespace(desktop_local=False)
    conn.subscriptions = dict(st.get("subscriptions") or {})
    conn.handed_back = set(st.get("handed_back") or [])
    conn.reconnects = set(st.get("reconnects") or [])
    conn.replay_cursors = {k: tuple(v) for k, v in (st.get("replay_cursors") or {}).items()}
    for pid, c in (st.get("siblings") or {}).items():
        sc = new_conn(cls, auths.get(c.get("authority", "")), {}, False)
        conn._siblings[pid] = sc
    cvs = st.get("consumed_voice_stops")
    if cvs is not None and conn.authority is not None:
        conn.authority._consumed_voice_stops = OrderedDict(
            (tuple(k), True) for k in cvs)
    CUR["conn"] = conn
    # Sibling construction is a seam: replace the class global the live
    # _sibling_for reads at call time (conn keeps the REAL class + property).
    def ctor(authority, transport, identity, operator=False):
        r = seam_rec("new_connection", {"authority_id": authority.aid,
                                        "identity": identity,
                                        "operator": operator})
        target = auths.get(r.get("authority")) if isinstance(r, dict) else None
        return new_conn(cls, target if target is not None else authority,
                        identity, operator)
    ns["AuthorityConnection"] = ctor
    return conn


def norm(x):
    ns = CUR["ns"]
    if isinstance(x, CUR["cls"]):
        return {"authority": x.authority.aid if x.authority is not None else None}
    if isinstance(x, (ns["SessionActor"], ns["SessionRef"], ns["SessionHandle"],
                      ns["Submission"], ns["AdmissionReceipt"])):
        return asdict(x)
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    return x


def norm_state(ns):
    conn = CUR["conn"]
    auth = conn.authority if conn is not None else None
    cvs = None
    if auth is not None and "_consumed_voice_stops" in vars(auth):
        cvs = [list(k) for k in auth._consumed_voice_stops.keys()]
    return {
        "storage_refusals": [list(k) for k in sorted(ns["_STORAGE_REFUSALS"])],
        "storage_tracebacks": [list(k) for k in sorted(ns["_STORAGE_TRACEBACKS"])],
        "subscriptions": dict(conn.subscriptions) if conn is not None else {},
        "handed_back": sorted(conn.handed_back) if conn is not None else [],
        "reconnects": sorted(conn.reconnects) if conn is not None else [],
        "replay_cursors": {k: list(v) for k, v in
                           (conn.replay_cursors.items() if conn is not None else [])},
        "consumed_voice_stops": cvs,
        "siblings": {pid: {"authority": c.authority.aid if c.authority is not None else None}
                     for pid, c in (conn._siblings.items() if conn is not None else [])},
    }


# ── drive ───────────────────────────────────────────────────────────────────
def drive(ns, func, inp):
    conn = materialize(ns, inp)
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    params = inp.get("params")
    refj = inp.get("ref") or {}
    ref = ns["SessionRef"](refj.get("profile_id", ""), refj.get("session_id", ""))

    if func == "_refusal_data":
        return ns["_refusal_data"](make_ar(inp.get("exc") or {}))
    if func == "_profile_label":
        return ns["_profile_label"](inp.get("profile_id"))
    if func == "_seen_storage_refusal":
        return ns["_seen_storage_refusal"](tuple(inp.get("key") or []))
    if func == "_log_refusal":
        return ns["_log_refusal"](inp.get("profile"), inp.get("method", ""),
                                  make_ar(inp.get("exc") or {}))
    if func == "_log_storage_failure":
        return ns["_log_storage_failure"](
            inp.get("profile"), inp.get("method", ""), inp.get("session_id"),
            make_storage_exc(inp.get("exc") or {}))
    if func == "model_only":
        return ns["model_only"](inp.get("row") or {})
    if func == "display_projection":
        return ns["display_projection"](
            inp.get("history") or [], inp.get("profile_home"),
            inp.get("image_urls", True))
    if func == "_sibling_for":
        return conn._sibling_for(inp.get("profile"))
    if func == "awaits_profile_start":
        return conn.awaits_profile_start(inp.get("request"),
                                         tuple(inp.get("held") or []))
    if func == "ping":
        return asyncio.run(conn.ping(ref, params))
    if func == "detach":
        return asyncio.run(conn.detach(ref, params))
    if func == "events_since":
        return asyncio.run(conn.events_since(ref, params))
    if func == "_voice_stop_replay":
        return conn._voice_stop_replay(ref, params)
    if func == "_record_voice_stop":
        return conn._record_voice_stop(ref, params)
    if func == "_typed_voice_stop":
        return asyncio.run(conn._typed_voice_stop(ref, params))
    if func == "submission":
        return conn.submission(ref, params)
    if func == "interrupt":
        return asyncio.run(conn.interrupt(ref, params))
    if func == "cancel":
        return asyncio.run(conn.cancel(ref, params))
    raise ValueError("unknown func: %s" % func)


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


def main():
    ns = extract()
    install_stubs(ns)
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
        CUR["conn"] = None
        try:
            ret = drive(ns, func, inp)
            want_ret = norm(ret)
        except BaseException as e:
            want_ret = raise_desc(e)
        want = {"ret": want_ret, "trace": CUR["trace"], "state": norm_state(ns)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_controls_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
