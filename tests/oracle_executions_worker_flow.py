#!/usr/bin/env python3
"""Differential oracle for executions_worker_flow (persistence/sqlite/workers/
executions.py -> src/persistence/executions_worker_flow.c, ewf_*).

The harness emits {func, in, out:{ret, trace}}. This oracle:
  * AST-extracts the LIVE defs from the READ-ONLY quarry under the SAME
    scripted io (in.io) and drives them with recording seams that emit the
    identical ordered event shape {"name","args","ret"};
  * diffs the live result against the harness's C result;
  * fail-closes on 0 cases or a namespace / extraction fault.

Contract: None->null; JSON objects keep insertion order; an exception is
{"__raise__": cls, "message": str(exc)}; scripted raises are canonicalized
through the FULL py3.14 builtin exception hierarchy (audited at startup).
BaseException members propagate as real raises (an "except RuntimeError" in
persist_operation never swallows SystemExit/KeyboardInterrupt/GeneratorExit).

Summary: 'ORACLE executions_worker_flow: N cases, M mismatches'
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

NAME = "executions_worker_flow"
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "persistence" / "sqlite" / "workers" / "executions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "persistence" / "sqlite" / "workers" / "executions.py")

# ── FULL builtin exception hierarchy (mirror of the C ewf_exc_parent table) ─
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


# ── AST extraction of the LIVE defs ───────────────────────────────────────
PORTED_HELPERS = ["_key_set", "_record", "_messages", "_annotations"]
PORTED_METHODS = [
    "finish", "record_usage", "record_auxiliary_usage", "acquire_turn_lease",
    "cleanup_turn_lease", "set_session_title", "set_session_title_source",
    "next_session_title", "set_session_billing_route", "set_latest_user_api_content",
    "set_message_api_content", "set_user_message_content", "create_session",
    "session_lifecycle", "patch_session_sidecars", "set_session_tool_pin",
    "append_transcript", "compression_watermark", "compression_boundary",
    "persist_operation",
]
PORTED = PORTED_HELPERS + PORTED_METHODS

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
_METHOD_NODES = {n.name: n for n in _CLS.body
                 if isinstance(n, ast.FunctionDef) and n.name in PORTED_METHODS}
_HELPER_NODES = {n.name: n for n in _TREE.body
                 if isinstance(n, ast.FunctionDef) and n.name in PORTED_HELPERS}
_MISSING = [f for f in PORTED if f not in _METHOD_NODES and f not in _HELPER_NODES]
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
    if isinstance(v, (dict,)):
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


CUSTOM_EXC = {"AuthorityRefusal": AuthorityRefusal}


def make_exc(spec):
    cls = CANON.get(spec.get("__raise__", "Exception"), spec.get("__raise__", "Exception"))
    msg = str(spec.get("message", ""))
    E = CUSTOM_EXC.get(cls) or getattr(builtins, cls, Exception)
    try:
        return E(msg)
    except Exception:
        return Exception(msg)


class SeamFn:
    """_persist / _value: record args, emit one ordered trace event, then return
    the scripted value or raise a REAL Python exception (so a live except
    clause sees the correct class)."""
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        ctx = CUR_CTX
        name = self.name
        if name == "_persist":
            args = {
                "epoch": k["epoch"], "execution_id": k["execution_id"],
                "session_id": k["session_id"], "generation": k["generation"],
                "sequence": k["sequence"], "operation": a[0], "payload": a[1],
            }
        else:  # _value(operation, payload, epoch, execution_id, session_id, generation, sequence)
            args = {
                "epoch": a[2], "execution_id": a[3], "session_id": a[4],
                "generation": a[5], "sequence": a[6], "operation": a[0],
                "payload": a[1],
            }
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
                                  "ret": {"__raise__": type(exc).__name__,
                                          "message": str(exc)}})
                raise exc
            chosen = chosen.get("ret")
        ctx.trace.append({"name": name, "args": norm(args), "ret": norm(chosen)})
        return chosen


# ── namespace + instance construction ────────────────────────────────────
ROW_KEYS = ("_row_id", "timestamp", "_db_row_snapshot", "_canonical_row",
            "message_uid", "_tool_call_uids", "_tool_call_uid")
MAIN_KEYS = frozenset({
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "api_call_count", "estimated_cost_usd", "actual_cost_usd",
    "cost_status", "cost_source", "pricing_version", "model", "billing_provider",
    "billing_base_url", "billing_mode", "absolute", "task", "source", "prompt_cache_clock",
})
AUX_KEYS = frozenset({
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "api_call_count", "estimated_cost_usd", "model",
    "billing_provider", "billing_base_url",
})
CREATE_KEYS = frozenset({
    "source", "model", "model_config", "system_prompt", "user_id", "session_key",
    "chat_id", "chat_type", "thread_id", "parent_session_id", "cwd", "profile_name",
    "git_repo_root", "origin_json", "display_name",
})
CREATE_REQ = frozenset({"source"})
APPEND_KEYS = frozenset({"turn_lease_holder"})


class RowAnnotation:
    @staticmethod
    def from_wire(a):
        return {k: a.get(k) for k in ROW_KEYS}


def identity(v):
    return v


def rec_ctor(**kw):
    return dict(kw)


def build():
    mod = types.ModuleType("ewf_live_executions")
    sys.modules["ewf_live_executions"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "ewf_live_executions",
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
        "WorkerExecutionRecord": rec_ctor, "WorkerTitleResult": rec_ctor,
        "WorkerAppendResult": rec_ctor, "RowAnnotation": RowAnnotation,
        # module constants
        "_MAIN_USAGE_KEYS": MAIN_KEYS, "_AUXILIARY_USAGE_KEYS": AUX_KEYS,
        "_CREATE_KEYS": CREATE_KEYS, "_APPEND_OPTION_KEYS": APPEND_KEYS,
    })
    for name in PORTED_HELPERS:
        exec_fn(_HELPER_NODES[name], ns)
    for name in PORTED_METHODS:
        exec_fn(_METHOD_NODES[name], ns)

    class RepoDouble:
        pass

    inst = RepoDouble()
    for name in PORTED_METHODS:
        setattr(inst, name, types.MethodType(ns[name], inst))
    inst._persist = SeamFn("_persist")
    inst._value = SeamFn("_value")
    return ns, inst


# ── LIVE driver ───────────────────────────────────────────────────────────
def mat_args(args, f):
    """Match the fixture schema to the live signature:
      * '**kwargs' verbs store their keyword payload under a nested key; the
        live signature collects those keys into **kwargs, so spread them.
      * dataclass parameters (lease/route/peer) are attribute carriers live;
        materialize them as attribute doubles (the C reads JSON fields).
      * an enum source double carries a .value attribute."""
    out = dict(args)
    spread = {
        "record_usage": "usage", "record_auxiliary_usage": "usage",
        "create_session": "fields", "append_transcript": "options",
    }
    if f in spread:
        sub = out.pop(spread[f], {})
        if isinstance(sub, dict):
            out.update(sub)
    for k in ("lease", "route", "peer"):
        if k in out and isinstance(out[k], dict):
            out[k] = types.SimpleNamespace(**out[k])
    if isinstance(out.get("source"), dict) and "__enum__" in out["source"]:
        out["source"] = types.SimpleNamespace(value=out["source"]["__enum__"])
    return out


def drive(rec, ns, inst):
    global CUR_CTX
    p = rec.get("in") or {}
    ctx = Ctx(p.get("io") or {})
    CUR_CTX = ctx
    f = rec.get("func")
    try:
        if f in PORTED_HELPERS:
            a = dict(p.get("args") or {})
            if f == "_key_set":
                for kk in ("allowed", "required"):
                    if kk in a and isinstance(a[kk], list):
                        a[kk] = frozenset(a[kk])
            ret = ns[f](**a)
        else:
            ret = getattr(inst, f)(**mat_args(p.get("args") or {}, f))
        return {"ret": norm(ret), "trace": ctx.trace}
    except BaseException as e:
        return {"ret": {"__raise__": type(e).__name__, "message": str(e)},
                "trace": ctx.trace}


def cross_check(records):
    seen = {r.get("func") for r in records}
    absent = [f for f in PORTED if f not in seen]
    extra = [f for f in seen if f not in PORTED]
    if absent or extra:
        print("ORACLE %s: FATAL func set mismatch absent=%r extra=%r"
              % (NAME, absent, extra), file=sys.stderr)
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
