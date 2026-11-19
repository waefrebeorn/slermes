#!/usr/bin/env python3
"""Oracle for the UNCLAIMED gateway/session_transcript.py FLOW cores + leaves
(src/gateway/session_transcript_flow.c).

AST-extracts the live module constants, TranscriptReadError, the module helper
functions and the ENTIRE SessionTranscriptMixin (future-annotations compile, so
no heavy gateway import), plus the pure leaves extract_api_content_sidecar
(agent/turn_context.py) and _lazy (gateway/session.py). Every unported
collaborator (persistence resolution, store verbs, claimed siblings, spool /
JSONL fallback imports, logging) becomes a recording stub returning the SAME
scripted results the C harness feeds its universal seam.

State is injected as a JSON mirror; the oracle materializes a StoreDouble
(a real SessionTranscriptMixin subclass) plus per-id owner-store doubles so
live attribute access, `is None` gates and in-place mutations agree with the C
mirror, then norms the final state back to JSON. Compares ordered seam trace +
return + final state. BaseException is caught per case (scripted SystemExit /
KeyboardInterrupt / CancelledError propagate through live `except Exception`
exactly as in the port).

Input: stdin JSON lines from t_port_session_transcript_flow.
Summary: ORACLE session_transcript_flow: N cases, M mismatches. Fail-closed on
0 cases.
"""
import ast
import asyncio
import builtins
import contextlib
import json
import os
import sys
import threading
import time
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Dict, List, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_transcript.py")
TURNCTX = os.path.join(QUARRY, "agent", "turn_context.py")
SESSION = os.path.join(QUARRY, "gateway", "session.py")

WANTED_MOD = {"_spool_dropped", "_bindable_text", "transcript_append_kwargs"}

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {},
       "double": None, "owners": {}, "rowproj": {}, "ns": None}
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
    if kind == "TranscriptReadError":
        return CUR["ns"]["TranscriptReadError"](spec.get("session_id", ""))
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
    if isinstance(e, CUR["ns"]["TranscriptReadError"]):
        d["session_id"] = e.session_id
    return d


# ── extraction ──────────────────────────────────────────────────────────────
def _exec_nodes(nodes, src, ns, flags=0):
    for node in nodes:
        exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                     flags=flags), ns)


def extract():
    import __future__
    flags = __future__.annotations.compiler_flag
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("session_transcript_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "session_transcript_flow_extracted",
        "contextlib": contextlib, "json": json, "threading": threading,
        "time": time,
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "TYPE_CHECKING": False, "Any": Any, "Dict": Dict, "List": List,
        "Optional": Optional,
    })
    # pure leaf: live extract_api_content_sidecar (no seam)
    ttree = ast.parse(open(TURNCTX, encoding="utf-8").read())
    for node in ttree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "extract_api_content_sidecar":
            _exec_nodes([node], TURNCTX, ns, flags)
    # live _lazy (SessionStore base helper the mixin reads through)
    stree = ast.parse(open(SESSION, encoding="utf-8").read())
    for node in ast.walk(stree):
        if isinstance(node, ast.FunctionDef) and node.name == "_lazy":
            _exec_nodes([node], SESSION, ns, flags)
            break
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                _exec_nodes([node], SRC, ns, flags)
    ns["logger"] = RecLogger()   # restore after the logger Assign re-ran
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_MOD:
            _exec_nodes([node], SRC, ns, flags)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "TranscriptReadError", "SessionTranscriptMixin"):
            _exec_nodes([node], SRC, ns, flags)
    for name in list(WANTED_MOD) + ["TranscriptReadError",
                                    "SessionTranscriptMixin",
                                    "_ASSISTANT_ONLY_KEYS", "_BOUND_TEXT_KEYS",
                                    "extract_api_content_sidecar", "_lazy"]:
        assert name in ns, "extraction missed %s" % name
    CUR["ns"] = ns
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

    reg("gateway.shutdown_flush", {
        "spool_dropped_transcript_message": lambda sid, msg: seam_rec(
            "spool_dropped_transcript_message",
            {"session_id": sid, "message": msg}),
        "drain_transcript_spool": lambda *a, **k: (0, 0),
    })

    def _home():
        r = seam_rec("get_hermes_home", {})
        return Path(r) if isinstance(r, str) else r

    reg("hermes_constants", {"get_hermes_home": _home})
    reg("session_rescue", {
        "divert_session_transcript_jsonl": lambda sid, messages, sessions_dir=None: seam_rec(
            "divert_session_transcript_jsonl",
            {"session_id": sid, "remaining": messages,
             "sessions_dir": str(sessions_dir)}),
    })

    class ConversationRequest:
        def __init__(self, view=None, include_lineage=False,
                     repair_alternation=False, include_row_ids=False):
            self.view = view
            self.include_lineage = include_lineage
            self.repair_alternation = repair_alternation
            self.include_row_ids = include_row_ids

    class NewMessage:
        def __init__(self, **fields):
            self.fields = fields

        @classmethod
        def from_live(cls, message):
            return cls(**message)

    class RewindTargetUnavailable(Exception):
        pass

    reg("persistence.conversation.messages", {
        "ConversationRequest": ConversationRequest,
        "NewMessage": NewMessage,
        "ReplaceMode": SimpleNamespace(PRESERVE_EXISTING_ARCHIVE="preserve",
                                       DESTRUCTIVE="destructive"),
        "RewindTargetUnavailable": RewindTargetUnavailable,
    })
    reg("persistence.core.errors", {
        "CompressionClosed": type("CompressionClosed", (RuntimeError,), {}),
        "StoreQuarantined": type("StoreQuarantined", (Exception,), {}),
        "StoreReplaced": type("StoreReplaced", (RuntimeError,), {}),
        "SearchIndexCorrupt": type("SearchIndexCorrupt", (Exception,), {}),
    })
    reg("agent.checkpoints.undo_hint", {
        "undo_span_start": lambda ts, prefix: seam_rec(
            "undo_span_start", {"timestamp": ts, "prefix": prefix}),
    })
    reg("agent.turn_context", {
        "extract_api_content_sidecar": ns["extract_api_content_sidecar"],
    })


# ── object doubles ──────────────────────────────────────────────────────────
class MsgDouble:
    def __init__(self, proj):
        self.proj = proj

    def as_openai_message(self):
        return seam_rec("as_openai_message", {"message": self.proj})


class LineageNS:
    def __init__(self, store_id):
        self.store_id = store_id

    def tip(self, sid):
        return seam_rec("lineage.tip",
                        {"store": self.store_id, "session_id": sid})

    def is_compression_child(self, row):
        return seam_rec("lineage.is_compression_child",
                        {"store": self.store_id,
                         "child": CUR["rowproj"].get(id(row))})


class SessionsNS:
    def __init__(self, store_id):
        self.store_id = store_id

    def get(self, sid):
        r = seam_rec("sessions.get",
                     {"store": self.store_id, "session_id": sid})
        if isinstance(r, dict):
            row = SimpleNamespace(**r)
            CUR["rowproj"][id(row)] = r
            return row
        return r


class MessagesNS:
    def __init__(self, store_id):
        self.store_id = store_id

    def has_platform_message_id(self, sid, pmid):
        return seam_rec("messages.has_platform_message_id",
                        {"store": self.store_id, "session_id": sid,
                         "platform_message_id": pmid})

    def latest_conversation_role(self, sid):
        return seam_rec("messages.latest_conversation_role",
                        {"store": self.store_id, "session_id": sid})

    def active_watermark(self, sid):
        r = seam_rec("messages.active_watermark",
                     {"store": self.store_id, "session_id": sid})
        return SimpleNamespace(**r) if isinstance(r, dict) else r

    def has_gateway_input_owner(self, sid, owner):
        return seam_rec("messages.has_gateway_input_owner",
                        {"store": self.store_id, "session_id": sid,
                         "owner": owner})

    def read_conversation(self, sid, req):
        r = seam_rec("messages.read_conversation",
                     {"store": self.store_id, "session_id": sid,
                      "repair_alternation": req.repair_alternation})
        if isinstance(r, list):
            return [MsgDouble(m) for m in r]
        return r

    def retain_refused_turn_for_display(self, sid, notice=None,
                                        boundary_row_id=None):
        return seam_rec("messages.retain_refused_turn_for_display",
                        {"store": self.store_id, "session_id": sid,
                         "notice": notice, "boundary_row_id": boundary_row_id})


class OwnerStore:
    """Persistence handle double: attributes resolve inertly, verbs seam."""
    def __init__(self, store_id):
        self.store_id = store_id
        self.lineage = LineageNS(store_id)
        self.sessions = SessionsNS(store_id)
        self.messages = MessagesNS(store_id)


def make_double_class(ns):
    class StoreDouble(ns["SessionTranscriptMixin"]):
        pass
    StoreDouble._lazy = ns["_lazy"]
    return StoreDouble


def materialize(ns, inp):
    st = inp.get("state") or {}
    cls = CUR["cls"]
    d = object.__new__(cls)
    d._dirty_transcripts = {k: [dict(m) for m in v]
                            for k, v in (st.get("dirty_transcripts") or {}).items()}
    d._transcript_append_failures = dict(st.get("append_failures") or {})
    if st.get("reroutes") is not None:
        d._transcript_reroutes = dict(st["reroutes"])
    if st.get("spooled_drop_sessions") is not None:
        d._spooled_drop_sessions = set(st["spooled_drop_sessions"])
    if st.get("session_owner_hints") is not None:
        d._session_owner_hints = dict(st["session_owner_hints"])
    d._entries = {k: SimpleNamespace(**v) if isinstance(v, dict) else v
                  for k, v in (st.get("entries") or {}).items()}
    d._lock = contextlib.nullcontext()
    d._transcript_retry_lock = contextlib.nullcontext()
    CUR["owners"] = {sid: OwnerStore(sid) for sid in (st.get("stores") or {})}
    CUR["rowproj"] = {}

    def persistence(sid):
        r = seam_rec("persistence_for_session_id", {"session_id": sid})
        if isinstance(r, str) and r in CUR["owners"]:
            return CUR["owners"][r]
        return r

    d._persistence_for_session_id = persistence
    d._save = lambda: seam_rec("save", {})
    # claimed siblings: seam calls (scripted results)
    d._compression_tip_for_session_id = lambda sid: seam_rec(
        "compression_tip_for_session_id", {"session_id": sid})

    def drain_lock():
        seam_rec("get_transcript_drain_lock", {})
        return contextlib.nullcontext()

    d._get_transcript_drain_lock = drain_lock

    def clear_dirty(sid):
        seam_rec("clear_dirty_transcript", {"session_id": sid})
        # known state effect of the claimed sibling (applied on both sides)
        d._dirty_transcripts.pop(sid, None)
        d._transcript_append_failures.pop(sid, None)

    d._clear_dirty_transcript = clear_dirty
    d.rewrite_transcript = lambda sid, messages, active_only=False, \
        reject_active_turn_lease=False: seam_rec(
            "rewrite_transcript",
            {"session_id": sid, "messages": messages,
             "active_only": active_only,
             "reject_active_turn_lease": reject_active_turn_lease})
    CUR["double"] = d
    return d


def norm(x):
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    return x


def norm_state():
    d = CUR["double"]
    return {
        "dirty_transcripts": norm(d._dirty_transcripts),
        "append_failures": norm(d._transcript_append_failures),
        "reroutes": norm(d._transcript_reroutes)
        if hasattr(d, "_transcript_reroutes") else None,
        "spooled_drop_sessions": sorted(d._spooled_drop_sessions)
        if hasattr(d, "_spooled_drop_sessions") else None,
        "session_owner_hints": norm(d._session_owner_hints)
        if hasattr(d, "_session_owner_hints") else None,
        "entries": {k: dict(vars(v)) if isinstance(v, SimpleNamespace) else v
                    for k, v in d._entries.items()},
    }


# ── drive ───────────────────────────────────────────────────────────────────
def drive(ns, func, inp):
    d = materialize(ns, inp)
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    sid = inp.get("session_id")

    if func == "TranscriptReadError":
        return raise_desc(ns["TranscriptReadError"](sid))
    if func == "_spool_dropped":
        return ns["_spool_dropped"](sid, inp.get("message"))
    if func == "_bindable_text":
        return ns["_bindable_text"](inp.get("value"))
    if func == "transcript_append_kwargs":
        return ns["transcript_append_kwargs"](
            sid, inp.get("message"), fallback_ts=inp.get("fallback_ts"))
    if func == "_follow_reroutes":
        return d._follow_reroutes(sid)
    if func == "_enqueue_transcript_message":
        cls = ns["SessionTranscriptMixin"]
        old = cls._MAX_PENDING_PER_SESSION
        cls._MAX_PENDING_PER_SESSION = int(inp.get("cap", 200))
        try:
            return d._enqueue_transcript_message(sid, inp.get("message"))
        finally:
            cls._MAX_PENDING_PER_SESSION = old
    if func == "_divert_transcript_after_db_replaced":
        return d._divert_transcript_after_db_replaced(
            sid, inp.get("queue_session_id"), make_exc(inp.get("exc") or {}))
    if func == "_live_compression_child":
        return d._live_compression_child(sid)
    if func == "_migrate_transcript_queue_to_child":
        qsid = inp.get("queue_session_id")
        pending = [dict(m) for m in (inp.get("pending") or [])]
        d._dirty_transcripts[qsid] = pending   # the caller's list IS state's
        if inp.get("msg_is_head") and pending:
            msg = pending[0]
        else:
            msg = dict(inp.get("msg") or {})
        return d._migrate_transcript_queue_to_child(
            sid, qsid, inp.get("child_id"), pending, msg)
    if func == "_publish_transcript_reroute":
        return d._publish_transcript_reroute(sid, inp.get("child_id"))
    if func == "_spool_stalled_backlog":
        return d._spool_stalled_backlog(sid, inp.get("queue_session_id"))
    if func == "mark_spooled_drop_sessions":
        return d.mark_spooled_drop_sessions(inp.get("session_ids"))
    if func == "has_platform_message_id":
        return d.has_platform_message_id(sid, inp.get("platform_message_id"))
    if func == "transcript_tail_role":
        return d.transcript_tail_role(sid)
    if func == "is_published_compression_child":
        return d.is_published_compression_child(
            inp.get("parent_session_id"), inp.get("child_session_id"))
    if func == "persist_rotated_compression_child":
        return d.persist_rotated_compression_child(
            inp.get("parent_session_id"), inp.get("child_session_id"),
            inp.get("messages"))
    if func == "has_input_owner":
        return d.has_input_owner(sid, inp.get("owner"))
    if func == "load_transcript":
        return d.load_transcript(sid)
    if func == "retain_refused_turn_for_display":
        return d.retain_refused_turn_for_display(
            sid, notice=inp.get("notice"),
            boundary_row_id=inp.get("boundary_row_id"))
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()),
                              key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def run_case(rec_):
    ns = CUR["ns"]
    func = rec_.get("func")
    inp = rec_.get("in") or {}
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["double"] = None
    try:
        ret = drive(ns, func, inp)
        want_ret = norm(ret)
    except BaseException as e:
        want_ret = raise_desc(e)
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state()}


def main():
    ns = extract()
    install_stubs(ns)
    CUR["cls"] = make_double_class(ns)
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
        cases += 1
        got = record.get("out") or {}
        want = run_case(record)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % record.get("func"))
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_transcript_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())