#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/workers/executions.py (gate w19_executions_57e1de).

Extracts the live module helpers (`_key_set`, `_messages`, `_record`) and the repository class
from the Python AST, then drives each ported verb on a stubbed store: `mutate_worker_execution`
captures the exact `operation`/`payload` the receipt transaction would receive. The keyword
sets are derived from the live TypedDict bodies, so the fixture's `allowed` lists are checked
against the real WorkerMainUsage / WorkerAuxiliaryUsage / WorkerSessionCreate /
TranscriptAppendOptions declarations.
"""
import ast
import contextlib
import json
import sys
import types
import __future__
from dataclasses import dataclass, fields
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "workers" / "executions.py"
WORKERS = QUARRY / "persistence" / "workers" / "executions.py"
SESSIONS = QUARRY / "persistence" / "workers" / "sessions.py"
GATE = "w19_executions_57e1de"
_ANN = __future__.annotations.compiler_flag


class AuthorityRefusal(Exception):
    pass


class WorkerKind(str):
    _vals = ("cron", "child", "compute", "kanban")

    def __new__(cls, v):
        if v not in cls._vals:
            raise ValueError("%r is not a valid WorkerKind" % (v,))
        return str.__new__(cls, v)


class WorkerStatus(str):
    _vals = ("registered", "running", "unknown", "terminal")

    def __new__(cls, v):
        if v not in cls._vals:
            raise ValueError("%r is not a valid WorkerStatus" % (v,))
        return str.__new__(cls, v)


def _ident(v):
    return v


@dataclass(frozen=True)
class WorkerExecutionRecord:
    execution_id: object
    session_id: object
    kind: object
    owner_epoch: object
    generation: object
    status: object
    last_sequence: object

    def __post_init__(self):
        if self.last_sequence < 0:
            raise ValueError("WorkerExecutionRecord.last_sequence must be >= 0")


class RowAnnotation:
    @classmethod
    def from_wire(cls, annotation):
        return cls()


class WorkerAppendResult:
    def __init__(self, count=None, annotations=()):
        self.count = count
        self.annotations = annotations


class WorkerCooldownSnapshot:
    def __init__(self, session_exists=None, cooldown_until=None, error=None):
        self.session_exists = session_exists
        self.cooldown_until = cooldown_until
        self.error = error


class WorkerTurnLease:
    def __init__(self, holder=None, ttl_seconds=300.0):
        self.holder = holder
        self.ttl_seconds = ttl_seconds


class ConversationPeer:
    def __init__(self, session_key=None, source=None):
        self.session_key = session_key
        self.source = source


class WorkerActivity:
    def __init__(self, ts=None, description=None, provenance=None):
        self.ts = ts
        self.description = description
        self.provenance = provenance


class _Gateway:
    pass


class _Store:
    def holder(self):
        return "holder"


def typed_dict_keys(path, name):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == name:
            req, opt = [], []
            for b in node.body:
                if isinstance(b, ast.AnnAssign) and isinstance(b.target, ast.Name):
                    ann = ast.unparse(b.annotation)
                    (req if ann.startswith("Required[") else opt).append(b.target.id)
            return req, opt
    return [], []


CREATE_REQ, CREATE_OPT = typed_dict_keys(SESSIONS, "WorkerSessionCreate")
APPEND_REQ, APPEND_OPT = typed_dict_keys(SESSIONS, "TranscriptAppendOptions")
MAIN_REQ, MAIN_OPT = typed_dict_keys(WORKERS, "WorkerMainUsage")
AUX_REQ, AUX_OPT = typed_dict_keys(WORKERS, "WorkerAuxiliaryUsage")


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


G = {"__name__": "exec_live", "AuthorityRefusal": AuthorityRefusal,
     "WorkerExecutionRecord": WorkerExecutionRecord, "WorkerKind": WorkerKind,
     "WorkerStatus": WorkerStatus, "WorkerExecutionId": _ident, "SessionId": _ident,
     "AuthorityEpoch": _ident, "ExecutionGeneration": _ident, "MessageId": _ident,
     "RowAnnotation": RowAnnotation, "WorkerAppendResult": WorkerAppendResult,
     "isinstance": isinstance, "Mapping": dict, "dict": dict, "set": set,
     "frozenset": frozenset, "tuple": tuple, "len": len, "getattr": getattr,
     "refused": contextlib.nullcontext, "_gateway": _Gateway(),
     "_MAIN_USAGE_KEYS": frozenset(MAIN_REQ + MAIN_OPT),
     "_AUXILIARY_USAGE_KEYS": frozenset(AUX_REQ + AUX_OPT),
     "_CREATE_KEYS": frozenset(CREATE_REQ + CREATE_OPT),
     "_APPEND_OPTION_KEYS": frozenset(APPEND_REQ + APPEND_OPT),
     "_worker_public": lambda row: row,
     "worker_claim_origin": lambda *a, **k: None,
     "worker_retry_target": lambda *a, **k: None,
     "register_repository": lambda *a, **k: None}


def _exec_module(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec_module(SRC, {"_key_set", "_messages", "_annotations", "_record"}, G)

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for node in _tree.body:
    if isinstance(node, ast.ClassDef) and node.name == "SQLiteWorkerExecutionRepository":
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                     flags=__future__.annotations.compiler_flag, dont_inherit=True), G)

CAPTURE = {}


def mutate_worker_execution(_holder, *, epoch=None, execution_id=None, session_id=None,
                            generation=None, sequence=None, operation=None, payload=None):
    CAPTURE["operation"] = operation
    CAPTURE["payload"] = json.loads(json.dumps(payload, default=str))
    return {"value": 0, "count": 0, "annotations": []}


G["mutate_worker_execution"] = mutate_worker_execution


def _repo():
    return G["SQLiteWorkerExecutionRepository"](_Store())


def _call(method, **kw):
    CAPTURE.clear()
    getattr(_repo(), method)(epoch=1, execution_id="e1", session_id="s1", generation=1,
                             sequence=1, **kw)
    return {"ok": True, "operation": CAPTURE["operation"], "payload": CAPTURE["payload"]}


def run_case(r):
    f = r.get("func")
    if f == "key_set":
        try:
            G["_key_set"](r.get("keys"), set(r.get("allowed") or []), set(r.get("required") or []))
            return "ok"
        except AuthorityRefusal:
            return "invalid_params"
    if f == "messages":
        return G["_messages"](r.get("messages") or [])
    if f == "record":
        try:
            rec = G["_record"](r.get("public") or {})
        except (ValueError, KeyError) as exc:
            return {"ok": False, "error": type(exc).__name__}
        return {"ok": True, "value": {k: getattr(rec, k) for k in (
            "execution_id", "session_id", "kind", "owner_epoch", "generation", "status",
            "last_sequence")}}
    if f == "payload_usage_main":
        try:
            return _call("record_usage", **(r.get("usage") or {}))
        except AuthorityRefusal:
            return {"ok": False, "error": "invalid_params"}
    if f == "payload_usage_auxiliary":
        try:
            return _call("record_auxiliary_usage", task=r.get("task"), **(r.get("usage") or {}))
        except AuthorityRefusal:
            return {"ok": False, "error": "invalid_params"}
    if f == "payload_create":
        try:
            return _call("create_session", **(r.get("fields") or {}))
        except AuthorityRefusal:
            return {"ok": False, "error": "invalid_params"}
    if f == "payload_append":
        try:
            return _call("append_transcript", messages=r.get("messages") or [],
                         **(r.get("options") or {}))
        except AuthorityRefusal:
            return {"ok": False, "error": "invalid_params"}
    if f == "payload_user_content":
        kw = {"row_id": r.get("row_id"), "content": r.get("content")}
        if r.get("has_display_metadata"):
            kw["display_metadata"] = r.get("display_metadata")
        return _call("set_user_message_content", **kw)
    if f == "payload_restore_cooldown":
        snap = r.get("snapshot") or {}
        return _call("restore_compression_cooldown",
                     snapshot=WorkerCooldownSnapshot(session_exists=snap.get("session_exists"),
                                                     cooldown_until=snap.get("cooldown_until"),
                                                     error=snap.get("error")))
    if f == "payload_lease":
        return _call("acquire_turn_lease",
                     lease=WorkerTurnLease(holder=r.get("holder"), ttl_seconds=r.get("ttl_seconds")))
    if f == "payload_boundary":
        peer = r.get("peer") or {}
        return _call("compression_boundary",
                     peer=ConversationPeer(session_key=peer.get("session_key"),
                                           source=peer.get("source")))
    if f == "payload_activity":
        act = r.get("activity") or {}
        return _call("touch_session_activity",
                     activity=WorkerActivity(ts=act.get("ts"), description=act.get("description"),
                                             provenance=act.get("provenance")))
    return None


def main():
    cases = mism = 0
    wanted = {"key_set", "messages", "record", "payload_usage_main", "payload_usage_auxiliary",
              "payload_create", "payload_append", "payload_user_content",
              "payload_restore_cooldown", "payload_lease", "payload_boundary", "payload_activity"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in wanted:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s args=%r py=raised %r c=%r" % (f, r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
