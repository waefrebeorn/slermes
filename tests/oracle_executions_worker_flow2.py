#!/usr/bin/env python3
"""Differential oracle for executions_worker_flow2 (persistence/sqlite/workers/
executions.py -> src/persistence/executions_worker_flow2.c, ewf2_*).

The harness emits {func, in, out:{ret, trace}}. This oracle:
  * AST-extracts the LIVE unclaimed remainder defs from the READ-ONLY quarry
    under the SAME scripted io (in.io) and drives them with recording seams
    that emit the identical ordered event shape {"name","args","ret"};
  * diffs the live result against the harness's C result;
  * fail-closes on 0 cases or a namespace / extraction fault.

Seam set (must equal src/persistence/executions_worker_flow2.c):
    _persist, _value, _ledger

Contract: None->null; JSON objects keep insertion order; an exception is
{"__raise__": cls, "message": str(exc)}; scripted raises are canonicalized
through the FULL py3.14 builtin exception hierarchy (audited at startup).
BaseException members propagate as real raises (no except clause in these
verbs, so SystemExit/KeyboardInterrupt/GeneratorExit pass through).

Summary: 'ORACLE executions_worker_flow2: N cases, M mismatches'
"""
import __future__
import ast
import builtins
import json
import os
import sys
import types
from collections.abc import Mapping
from pathlib import Path

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(Path.home() / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

NAME = "executions_worker_flow2"
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "persistence" / "sqlite" / "workers" / "executions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "persistence" / "sqlite" / "workers" / "executions.py")

# ── FULL builtin exception hierarchy (mirror of the C ewf2_exc_parent table) ─
CANON = {"IOError": "OSError", "EnvironmentError": "OSError", "WindowsError": "OSError"}
EXC_PARENT = {
    "BaseException": None,
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "BaseExceptionGroup": "BaseException",
    "ExceptionGroup": "Exception", "Exception": "BaseException",
    "ArithmeticError": "Exception", "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception",
    "ImportError": "Exception", "ModuleNotFoundError": "ImportError",
    "LookupError": "Exception", "IndexError": "LookupError", "KeyError": "LookupError",
    "MemoryError": "Exception", "NameError": "Exception", "UnboundLocalError": "NameError",
    "OSError": "Exception", "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError", "TimeoutError": "OSError",
    "ReferenceError": "Exception", "RuntimeError": "Exception",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "PythonFinalizationError": "RuntimeError", "StopAsyncIteration": "Exception",
    "StopIteration": "Exception", "SyntaxError": "Exception",
    "IndentationError": "SyntaxError", "TabError": "IndentationError",
    "_IncompleteInputError": "SyntaxError", "SystemError": "Exception",
    "TypeError": "Exception", "ValueError": "Exception", "UnicodeError": "ValueError",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "Warning": "Exception", "UserWarning": "Warning",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "SyntaxWarning": "Warning", "RuntimeWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "ResourceWarning": "Warning", "EncodingWarning": "Warning",
}


def audit_exc_table():
    bad = []
    for n in dir(builtins):
        o = getattr(builtins, n)
        if not (isinstance(o, type) and issubclass(o, BaseException)):
            continue
        canon = CANON.get(n, n)
        parent = EXC_PARENT.get(canon)
        if parent is None:
            if canon != "BaseException":
                bad.append((n, "missing table entry"))
            continue
        mro = [c.__name__ for c in o.__mro__]
        if parent not in mro:
            bad.append((n, "parent %s not in mro %s" % (parent, mro)))
    return bad


# ── AST extraction of the LIVE unclaimed remainder defs ──────────────────
PORTED_METHODS = [
    "renew_turn_lease", "touch_session_activity", "clear_session_activity",
    "end_session", "set_session_system_prompt", "latest_conversation_role",
    "message_role", "acquire_compression_lock", "renew_compression_lock",
    "compression_lock_holder", "cleanup_compression_lock", "record_compression_cooldown",
    "restore_compression_cooldown", "clear_compression_cooldown",
    "set_compression_fallback_streak", "set_compression_ineffective_count",
    "set_compression_recovery_deadline", "reopen_orphaned_compression",
    "compression_session_context", "compression_lineage_view", "compression_history",
    "dispatch_delegation", "complete_delegation", "record_delegation_child",
    "read_delegation", "withdraw_delegation",
    "claim_delegation_delivery", "ack_delegation_delivery", "drop_delegation_delivery",
    "defer_delegation_delivery", "release_delegation_delivery",
]
# _delivery is an internal live helper (drives the five delivery verbs); it is
# extracted and bound on the instance but is NOT a fixture/case surface.
EXTRA_METHODS = ["_delivery"]
SEAM_NAMES = ("_persist", "_value", "_ledger")

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag
_CLS = next((n for n in _TREE.body
             if isinstance(n, ast.ClassDef)
             and n.name == "SQLiteWorkerExecutionRepository"), None)
if _CLS is None:
    print("ORACLE %s: FATAL no SQLiteWorkerExecutionRepository in %s" % (NAME, SRC),
          file=sys.stderr)
    sys.exit(1)
_NEED = set(PORTED_METHODS) | set(EXTRA_METHODS)
_METHOD_NODES = {n.name: n for n in _CLS.body
                 if isinstance(n, ast.FunctionDef) and n.name in _NEED}
_MISSING = [f for f in sorted(_NEED) if f not in _METHOD_NODES]
if _MISSING:
    print("ORACLE %s: FATAL could not extract %r from %s" % (NAME, _MISSING, SRC),
          file=sys.stderr)
    sys.exit(1)


def exec_fn(node, ns):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)


# ── seam plumbing ─────────────────────────────────────────────────────────
CUR_CTX = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.trace = []
        self.cursors = {}


def norm(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    return v


class AuthorityRefusal(ValueError):
    def __init__(self, reason, **kw):
        self.reason = reason
        super().__init__(reason)


class CompressionPublishRefused(RuntimeError):
    def __init__(self, session_id=None, message=None):
        super().__init__(message if message is not None else "")


CUSTOM_EXC = {"AuthorityRefusal": AuthorityRefusal,
              "CompressionPublishRefused": CompressionPublishRefused}


def make_exc(spec):
    cls = CANON.get(spec.get("__raise__", "Exception"), spec.get("__raise__", "Exception"))
    msg = str(spec.get("message", ""))
    E = CUSTOM_EXC.get(cls) or getattr(builtins, cls, Exception)
    try:
        return E(msg)
    except Exception:
        return Exception(msg)


def _persist_args(a, k):
    return {
        "epoch": k["epoch"], "execution_id": k["execution_id"],
        "session_id": k["session_id"], "generation": k["generation"],
        "sequence": k["sequence"], "operation": a[0], "payload": a[1],
    }


def _value_args(a, k):
    return {
        "epoch": a[2], "execution_id": a[3], "session_id": a[4],
        "generation": a[5], "sequence": a[6], "operation": a[0], "payload": a[1],
    }


def _ledger_args(a, k):
    return {
        "epoch": a[2], "execution_id": a[3], "session_id": a[4],
        "generation": a[5], "sequence": a[6], "operation": a[0], "payload": a[1],
        "worker_pid": a[7], "worker_birth": a[8],
        "owner_started_at": k.get("owner_started_at"),
        "probe_refused": k.get("probe_refused"),
    }


ARG_BUILDERS = {"_persist": _persist_args, "_value": _value_args, "_ledger": _ledger_args}


class SeamFn:
    """_persist / _value / _ledger: record args, emit one ordered trace event,
    then return the scripted value or raise a REAL Python exception (so a live
    except clause / BaseException passthrough sees the correct class)."""
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        ctx = CUR_CTX
        name = self.name
        args = ARG_BUILDERS[name](a, k)
        # Presence check: a scripted null is a valid result; a missing key fails closed.
        if name not in ctx.io:
            exc = RuntimeError("no script for %s" % name)
            ctx.trace.append({"name": name, "args": norm(args),
                              "ret": {"__raise__": "RuntimeError", "message": str(exc)}})
            raise exc
        script = ctx.io[name]
        chosen = script
        if isinstance(script, list):
            idx = int(ctx.cursors.get(name, 0))
            if idx >= len(script):
                exc = RuntimeError("script exhausted for %s" % name)
                ctx.trace.append({"name": name, "args": norm(args),
                                  "ret": {"__raise__": "RuntimeError", "message": str(exc)}})
                raise exc
            ctx.cursors[name] = idx + 1
            chosen = script[idx]
        if isinstance(chosen, dict) and "__raise__" in chosen:
            exc = make_exc(chosen)
            cls = type(exc).__name__
            ctx.trace.append({"name": name, "args": norm(args),
                              "ret": {"__raise__": cls, "message": str(exc)}})
            raise exc
        if isinstance(chosen, dict) and chosen.get("__script__") is True:
            rse = chosen.get("raise")
            if isinstance(rse, dict):
                exc = make_exc(rse)
                ctx.trace.append({"name": name, "args": norm(args),
                                  "ret": {"__raise__": type(exc).__name__, "message": str(exc)}})
                raise exc
            chosen = chosen.get("ret")
        ctx.trace.append({"name": name, "args": norm(args), "ret": norm(chosen)})
        return chosen


# ── namespace + instance construction ────────────────────────────────────
def identity(v):
    return v


def rec_ctor(**kw):
    return dict(kw)


def delegation_payload(v):
    """Fixture domain: every value is a plain JSON value, so the live helper's
    is_dataclass / WorkerDelegationPayload branches never fire."""
    return {"delegation_id": v}


def build():
    mod = types.ModuleType("ewf2_live_executions")
    sys.modules["ewf2_live_executions"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "ewf2_live_executions",
        "__builtins__": builtins,
        "Mapping": Mapping,
        "AuthorityRefusal": AuthorityRefusal,
        "CompressionPublishRefused": CompressionPublishRefused,
        # NewType / enum constructors: identity (value validation out of domain)
        "WorkerExecutionId": identity, "SessionId": identity, "WorkerKind": identity,
        "WorkerStatus": identity, "AuthorityEpoch": identity,
        "ExecutionGeneration": identity, "TitleSource": identity,
        "SessionLifecycleStatus": identity,
        # record constructors: field dicts in declaration order
        "WorkerLineageView": rec_ctor,
        # read_delegation builds WorkerDelegationView(**ledger_ret); in the
        # plain-JSON fixture domain that projection is the identity field dict
        # (same shape the C port returns verbatim from the seam).
        "WorkerDelegationView": rec_ctor,
        # delegated helper: scalar -> {"delegation_id": value}
        "delegation_payload": delegation_payload,
    })
    for name in PORTED_METHODS:
        exec_fn(_METHOD_NODES[name], ns)
    for name in EXTRA_METHODS:
        exec_fn(_METHOD_NODES[name], ns)

    class RepoDouble:
        pass

    inst = RepoDouble()
    for name in PORTED_METHODS + EXTRA_METHODS:
        setattr(inst, name, types.MethodType(ns[name], inst))
    for name in SEAM_NAMES:
        setattr(inst, name, SeamFn(name))
    return ns, inst


# ── LIVE driver ───────────────────────────────────────────────────────────
# Params the LIVE methods read as ATTRIBUTES (obj.field), not dict keys. The
# harness emits them as plain JSON objects; an ast-extracted live def reads
# `.ts` / `.holder` / `.cooldown_until` / `.session_exists` off them, so each
# must be materialized as an attribute double or the oracle fails closed with
# AttributeError. Wording: the C port reads each known field with `ewf2_get`
# and maps an ABSENT key to JSON null; the fixture domain is plain JSON, so a
# real frozen dataclass cannot be built from it. Model each carrier as a
# SimpleNamespace carrying ALL of its fields, filling absent keys with None —
# the exact shape `norm()` renders as the C port's null defaults.
CARRIER_FIELDS = {
    "lease": ("holder", "ttl_seconds"),
    "activity": ("ts", "description", "provenance"),
    "cooldown": ("cooldown_until", "error"),
    "snapshot": ("session_exists", "cooldown_until", "error"),
}
# Which carrier each verb expects (fixtures may omit the whole carrier key for
# a lease verb — the C port then sees all-null fields; model the same double).
METHOD_CARRIER = {
    "renew_turn_lease": "lease",
    "acquire_compression_lock": "lease",
    "renew_compression_lock": "lease",
    "touch_session_activity": "activity",
    "record_compression_cooldown": "cooldown",
    "restore_compression_cooldown": "snapshot",
}


def carrier_double(key, value):
    fields = CARRIER_FIELDS[key]
    src = value if isinstance(value, dict) else {}
    return types.SimpleNamespace(**{f: src.get(f) for f in fields})


def mat_args(args, f):
    """Materialize every attribute-carrier param as an attribute double, with
    all known fields present (absent -> None), so the live def sees the SAME
    attribute shape the C port models from JSON fields."""
    out = dict(args)
    for key in CARRIER_FIELDS:
        if key in out:
            out[key] = carrier_double(key, out[key])
    ck = METHOD_CARRIER.get(f)
    if ck and ck not in out:
        out[ck] = carrier_double(ck, None)
    return out


def drive(rec, ns, inst):
    global CUR_CTX
    p = rec.get("in") or {}
    ctx = Ctx(p.get("io") or {})
    CUR_CTX = ctx
    f = rec.get("func")
    try:
        ret = getattr(inst, f)(**mat_args(p.get("args") or {}, f))
        return {"ret": norm(ret), "trace": ctx.trace}
    except BaseException as e:
        return {"ret": {"__raise__": type(e).__name__, "message": str(e)},
                "trace": ctx.trace}


def cross_check(records):
    seen = {r.get("func") for r in records}
    absent = [f for f in PORTED_METHODS if f not in seen]
    extra = [f for f in seen if f not in PORTED_METHODS]
    if absent or extra:
        print("ORACLE %s: FATAL func set mismatch absent=%r extra=%r"
              % (NAME, absent, extra), file=sys.stderr)
        sys.exit(1)
    # Seam-set equality is structural: C and oracle both use SEAM_NAMES.
    if tuple(sorted(SEAM_NAMES)) != ("_ledger", "_persist", "_value"):
        print("ORACLE %s: FATAL unexpected seam set %r" % (NAME, SEAM_NAMES), file=sys.stderr)
        sys.exit(1)


def selftest(records, ns, inst):
    """Drive every fixture through the LIVE defs; flag namespace faults that a
    fail-closed seam would otherwise hide as confident raise specs."""
    faults = []
    for rec in records:
        out = drive(rec, ns, inst)
        r = out.get("ret")
        if isinstance(r, dict) and r.get("__raise__") in ("NameError", "AttributeError"):
            faults.append((rec.get("func"), r))
    if faults:
        print("ORACLE %s: SELFTEST faults %r" % (NAME, faults[:6]), file=sys.stderr)
        sys.exit(1)


def diff_line(tag, obj):
    return "    %s=%s" % (tag, json.dumps(obj, ensure_ascii=False)[:1600])


def main():
    ns, inst = build()
    bad = audit_exc_table()
    if bad:
        print("ORACLE %s: FATAL exception table audit: %r" % (NAME, bad), file=sys.stderr)
        sys.exit(1)
    records = []
    for line in sys.stdin:
        line = line.strip()
        if line:
            records.append(json.loads(line))
    cross_check(records)
    selftest(records, ns, inst)
    n = mism = shown = 0
    for rec in records:
        n += 1
        got = rec.get("out")
        want = drive(rec, ns, inst)
        if got != want:
            mism += 1
            if shown < 12:
                shown += 1
                print("MISMATCH %s case %d:" % (rec.get("func"), n))
                print(diff_line("C  ", got))
                print(diff_line("PY ", want))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
