#!/usr/bin/env python3
"""Oracle for the kanban_db_dispatch dead-worker / crash-accounting remainder
FLOW port (src/cli/kanban_db_dispatch_flow2.c).

MODULE RESOLUTION: hermes_cli/kanban_db_dispatch.py is partly claimed by
port_kanban_db_dispatch.c, kanban_workers.c, kanban_db*.c and
session_kanban_flow3.c (which ported _worker_terminal_timeout_env,
_process_fingerprint, _pid_recycled, _worker_alive). This port covers the NEXT
unclaimed cluster — the dead-worker booking + crash breaker flow:
  describe_suppression, _worker_log_exit_code, _exit_summary_marker,
  _log_noise_prefixes, _worker_final_output, _DeadWorker.run_outcome,
  _classify_dead_worker, _classify_dead_worker_exit, _reclaim_dead_workers,
  _account_crashes, _is_handoff_event, adopt_worker_pid.

Drives the LIVE bodies with SCRIPTED seams: every _kb collaborator, every
in-function import (agent.i18n / hermes_cli.kanban_owner_recovery), every
claimed sibling (_classify_worker_exit, _exit_code_kind, _error_fingerprint,
_protocol_violation_streak, _record_task_failure, _worker_alive,
_process_fingerprint) and every ported sibling is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace"), the final return ("ret")
and the module-global mirror ("state", echoed verbatim — nothing here mutates
state).

Live defs are AST-extracted from hermes_cli/kanban_db_dispatch.py (targeted
module Assign constants seeded, the _DeadWorker / _CrashSweep dataclasses and
FunctionDefs exec'd with the __future__ annotations flag into a registered
sys.modules module — py3.14). The heavy quarry import chain is never imported.
Ported siblings are SEAMED inside flows (each is exercised by its own direct
cases, where the real body runs).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE kanban_db_dispatch_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds pass through every except-Exception site. The FULL
  builtin hierarchy plus the quarry customs ride make_exc.
- A _DeadWorker projects to its eight-field vars() dict; flags default false;
  the five leading fields are required (absent -> the dataclass __init__
  TypeError); an unknown key -> the unexpected-keyword TypeError.
- A _CrashSweep projects to vars(); crash_details tuples project to lists.
- describe_suppression results are SimpleNamespace doubles (absent attribute ->
  "'types.SimpleNamespace' object has no attribute ...").
- conn rows are fixture dicts (key access; absent key -> KeyError). conn.execute
  scripts are cursor specs {"rows": [...], "rowcount": N}.
- The write_txn CM is modelled by __enter__/__exit__ seams (enter raise skips
  the body; exit raise replaces the in-flight exception; truthy exit suppresses).
- Tuples project to lists; sets to sorted lists; integral floats to ints.
- Seam args drop None-valued TOP-LEVEL keys on both sides; nested nulls
  (conn.execute params, event payloads) are preserved.
- Source literals carry their exact bytes; fixture DATA is ASCII.
"""
import ast
import copy
import dataclasses
import json
import os as _os_real
import re
import sys
import textwrap
import types
import __future__ as _fut
from dataclasses import dataclass, field
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os_env = _os_real.environ
os_env.setdefault("HOME", str(_REPO.parent))
os_env.setdefault("USERPROFILE", str(_REPO.parent))

Q = _os_real.path.expanduser("~/hermes-agent")
DISP_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "kanban_db_dispatch.py")
QSQ_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "quiet_single_query.py")

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


def seed_named(src, ns, path, names):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            if not any(t.id in names for t in tgts):
                continue
            exec_node(src, node, ns, path)


# ── exception classes (shared identity between make_exc and except sites) ──
SqliteError = type("sqlite3.Error", (Exception,), {})
JSONDecodeError = type("JSONDecodeError", (ValueError,), {})

EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": __import__("asyncio").CancelledError,
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
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
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
    "UnicodeTranslateError": UnicodeTranslateError, "JSONDecodeError": JSONDecodeError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning,
    "sqlite3.Error": SqliteError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    # Unicode*Error builtins need the __new__ bypass (their __init__ wants 5 args)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, spec.get("message", ""))
        return e
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(vars(x))
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


# ── per-case script backend (mirrors the C kdd2_call exactly) ──
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
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    f.__name__ = name
    return f


_CUR_CTX = None


def late_rec(name, args_fn=None, build=None):
    def f(*a, **k):
        ctx = _CUR_CTX
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    f.__name__ = name
    return f


TIME_DOUBLE = types.SimpleNamespace(time=late_rec("time.time", lambda: {}))


# ── sqlite doubles ──
class _Cursor:
    def __init__(self, raw):
        self.raw = raw if isinstance(raw, dict) else {}

    def fetchone(self):
        rows = self.raw.get("rows") or []
        return rows[0] if rows else None

    def fetchall(self):
        return self.raw.get("rows") or []

    @property
    def rowcount(self):
        return self.raw.get("rowcount", -1)


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=()):
        raw = self.ctx.call("conn.execute", {"sql": sql, "params": list(params)})
        return _Cursor(raw)


# ── write_txn context-manager double ──
class _TxnCM:
    def __init__(self, ctx):
        self.ctx = ctx

    def __enter__(self):
        return self.ctx.call("kb.write_txn.__enter__", {})

    def __exit__(self, et, ev, tb):
        r = self.ctx.call("kb.write_txn.__exit__",
                          {"exc": et.__name__ if et is not None else None})
        return bool(r)


# ── registered stub modules (in-function import resolution) ──
HC_PKG = new_module("hermes_cli")
KB_MOD = new_module("hermes_cli.kanban_db")
HC_PKG.kanban_db = KB_MOD
DISP_MOD = new_module("hermes_cli.kanban_db_dispatch")
HC_PKG.kanban_db_dispatch = DISP_MOD
KOR_PKG = new_module("hermes_cli.kanban_owner_recovery")
HC_PKG.kanban_owner_recovery = KOR_PKG
AGENT_PKG = new_module("agent")
I18N_MOD = new_module("agent.i18n")
AGENT_PKG.i18n = I18N_MOD

# ── extract the live bodies ──
DISP_SRC = open(DISP_SRC_PATH, encoding="utf-8").read()
QSQ_SRC = open(QSQ_SRC_PATH, encoding="utf-8").read()

NS_DISP = {"__name__": "hermes_cli.kanban_db_dispatch", "__builtins__": __builtins__,
           "re": re, "time": TIME_DOUBLE, "dataclass": dataclass, "field": field,
           "_kb": KB_MOD}
# the exit trailer constant lives in quiet_single_query (module-level import)
seed_named(QSQ_SRC, NS_DISP, QSQ_SRC_PATH, {"KANBAN_WORKER_EXIT_TRAILER"})
seed_named(DISP_SRC, NS_DISP, DISP_SRC_PATH,
           {"_EXIT_TRAILER_RE", "_LOG_CHROME", "WORKER_OUTPUT_SEP",
            "_PROTOCOL_VIOLATION_ERROR", "_PROTOCOL_VIOLATION_FAILURE_LIMIT",
            "UNVERIFIED_WORKER_FINGERPRINT"})

# dataclasses first (the function bodies construct them at call time)
for node in ast.parse(DISP_SRC).body:
    if isinstance(node, ast.ClassDef) and node.name in ("_DeadWorker", "_CrashSweep"):
        exec_node(DISP_SRC, node, NS_DISP, DISP_SRC_PATH)
_DeadWorker = NS_DISP["_DeadWorker"]
_CrashSweep = NS_DISP["_CrashSweep"]

PORTED_FUNCS = ["describe_suppression", "_worker_log_exit_code", "_exit_summary_marker",
                "_log_noise_prefixes", "_worker_final_output", "_classify_dead_worker",
                "_classify_dead_worker_exit", "_reclaim_dead_workers", "_account_crashes",
                "_is_handoff_event", "adopt_worker_pid"]
PORTED = PORTED_FUNCS + ["run_outcome"]


def extract(src, ns, path, names):
    got = {}
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            exec_node(src, node, ns, path)
            got[node.name] = ns[node.name]
    return got


REAL_DISP = extract(DISP_SRC, NS_DISP, DISP_SRC_PATH, PORTED_FUNCS)
_missing_defs = [nm for nm in PORTED_FUNCS if nm not in REAL_DISP]
if not hasattr(_DeadWorker, "run_outcome"):
    _missing_defs.append("run_outcome")
if _missing_defs:
    print(f"FATAL oracle_kanban_db_dispatch_flow2: defs not extracted: {_missing_defs}")
    sys.exit(2)


def _dead_build(raw):
    return _DeadWorker(**raw) if isinstance(raw, dict) else raw


def _dead_proj(dead):
    return vars(dead) if isinstance(dead, _DeadWorker) else dead


# sibling seam arg projections
def _cdw_args(pid, claimer, exit_code=None, *, task_id=None, board=None):
    return {"pid": pid, "claimer": claimer, "exit_code": exit_code,
            "task_id": task_id, "board": board}


SIB_ARGS = {
    "_worker_log_exit_code": lambda task_id, board=None: {"task_id": task_id, "board": board},
    "_exit_summary_marker": lambda: {},
    "_log_noise_prefixes": lambda: {},
    "_worker_final_output": lambda task_id, board=None: {"task_id": task_id, "board": board},
    "_classify_dead_worker_exit": _cdw_args,
    "_classify_dead_worker": _cdw_args,
}
SIB_BUILD = {
    "_log_noise_prefixes": lambda raw: tuple(raw) if isinstance(raw, list) else raw,
    "_classify_dead_worker_exit": _dead_build,
    "_classify_dead_worker": _dead_build,
}


def install_stubs(ctx, func):
    # restore real ported bodies, then seam every sibling except the one driven
    for nm, real in REAL_DISP.items():
        NS_DISP[nm] = real
    for live in PORTED_FUNCS:
        if live == func or live not in SIB_ARGS:
            continue
        NS_DISP[live] = rec(ctx, live, SIB_ARGS[live], build=SIB_BUILD.get(live))

    # claimed siblings (always seamed)
    NS_DISP["_classify_worker_exit"] = rec(ctx, "_classify_worker_exit", lambda pid: {"pid": pid})
    NS_DISP["_exit_code_kind"] = rec(ctx, "_exit_code_kind", lambda code: {"code": code})
    NS_DISP["_error_fingerprint"] = rec(ctx, "_error_fingerprint", lambda error_text: {"error_text": error_text})
    NS_DISP["_protocol_violation_streak"] = rec(ctx, "_protocol_violation_streak",
                                                lambda conn, task_id: {"task_id": task_id})
    NS_DISP["_worker_alive"] = rec(ctx, "_worker_alive",
                                   lambda pid, started_at: {"pid": pid, "started_at": started_at})
    NS_DISP["_process_fingerprint"] = rec(ctx, "_process_fingerprint", lambda pid: {"pid": pid})

    _OMIT = object()

    def _rtf(conn, task_id, *, error, outcome, failure_limit=_OMIT, force_trip=_OMIT,
             release_claim=False, end_run=False, event_payload_extra=None):
        # project only the kwargs each call site actually passes (the else
        # branch omits force_trip; failure_limit=None is dropped null-side)
        d = {"task_id": task_id, "error": error, "outcome": outcome,
             "release_claim": release_claim, "end_run": end_run,
             "event_payload_extra": event_payload_extra}
        if failure_limit is not _OMIT:
            d["failure_limit"] = failure_limit
        if force_trip is not _OMIT:
            d["force_trip"] = force_trip
        return d
    NS_DISP["_record_task_failure"] = rec(ctx, "_record_task_failure", _rtf)

    # _kb collaborators (always seamed)
    KB_MOD.read_worker_log = rec(ctx, "kb.read_worker_log",
                                 lambda task_id, tail_bytes=4000, board=None:
                                 {"task_id": task_id, "tail_bytes": tail_bytes, "board": board})
    KB_MOD._host_prefix = rec(ctx, "kb._host_prefix", lambda: {})
    KB_MOD._to_epoch = rec(ctx, "kb._to_epoch", lambda value: {"value": value})
    KB_MOD._row_get = rec(ctx, "kb._row_get",
                          lambda row, col, default=None: {"row": row, "col": col, "default": default})
    KB_MOD._resolve_crash_grace_seconds = rec(ctx, "kb._resolve_crash_grace_seconds", lambda: {})
    KB_MOD._retry_status_for_run = rec(ctx, "kb._retry_status_for_run",
                                       lambda conn, task_id: {"task_id": task_id})

    def _end_run(conn, task_id, *, outcome, status, error, metadata):
        return {"task_id": task_id, "outcome": outcome, "status": status,
                "error": error, "metadata": metadata}
    KB_MOD._end_run = rec(ctx, "kb._end_run", _end_run)
    KB_MOD._append_event = rec(ctx, "kb._append_event",
                               lambda conn, task_id, kind, payload, run_id=None:
                               {"task_id": task_id, "kind": kind, "payload": payload, "run_id": run_id})
    KB_MOD._json_or = rec(ctx, "kb._json_or",
                          lambda value, default=None: {"value": value, "default": default})
    KB_MOD.write_txn = lambda conn, **kw: _TxnCM(ctx)
    NS_DISP["_kb"] = KB_MOD
    NS_DISP["time"] = TIME_DOUBLE

    # in-function imports
    KOR_PKG.owner_reclaim_blocked = rec(ctx, "owner_reclaim_blocked",
                                        lambda conn, task_id, crash_sweep=False:
                                        {"task_id": task_id, "crash_sweep": crash_sweep})
    KOR_PKG.managed_run_exit_code = rec(ctx, "managed_run_exit_code",
                                        lambda conn, task_id, pid, claim_lock:
                                        {"task_id": task_id, "pid": pid, "claim_lock": claim_lock})
    KOR_PKG.apply_attempt_note = rec(ctx, "apply_attempt_note",
                                     lambda conn, task_id, pid, dead:
                                     {"task_id": task_id, "pid": pid, "dead": _dead_proj(dead)})
    I18N_MOD.t = rec(ctx, "i18n.t", lambda key: {"key": key})
    sys.modules["hermes_cli.kanban_owner_recovery"] = KOR_PKG
    sys.modules["agent.i18n"] = I18N_MOD


def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = copy.deepcopy(inp.get("state") or {})
    saved = {m: sys.modules.get(m)
             for m in ("time", "hermes_cli.kanban_owner_recovery", "agent.i18n")}
    install_stubs(ctx, func)
    conn = _Conn(ctx)

    def kw(*names):
        return {k: args[k] for k in names if k in args}

    ret = None
    try:
        if func == "describe_suppression":
            results = [types.SimpleNamespace(**r) if isinstance(r, dict) else r
                       for r in (args.get("results") or [])]
            ret = NS_DISP[func](results)
        elif func in ("_worker_log_exit_code", "_worker_final_output"):
            ret = NS_DISP[func](**kw("task_id", "board"))
        elif func in ("_exit_summary_marker", "_log_noise_prefixes"):
            ret = NS_DISP[func]()
        elif func == "run_outcome":
            ret = _DeadWorker(**args["dead"]).run_outcome
        elif func in ("_classify_dead_worker", "_classify_dead_worker_exit"):
            ret = NS_DISP[func](**kw("pid", "claimer", "exit_code", "task_id", "board"))
        elif func == "_reclaim_dead_workers":
            ret = NS_DISP[func](conn, **kw("board"))
        elif func == "_account_crashes":
            details = [[tid, pid, claimer, _DeadWorker(**d)]
                       for tid, pid, claimer, d in (args.get("crash_details") or [])]
            ret = NS_DISP[func](conn, details)
        elif func == "_is_handoff_event":
            ret = NS_DISP[func](**kw("kind", "payload"))
        elif func == "adopt_worker_pid":
            ret = NS_DISP[func](conn, **kw("task_id", "run_id", "pid"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:   # SystemExit included
        ret = proj_exc(exc)
    finally:
        for m, sv in saved.items():
            if sv is not None:
                sys.modules[m] = sv
            else:
                sys.modules.pop(m, None)
        _CUR_CTX = None
    state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    # ported siblings
    "_worker_log_exit_code", "_exit_summary_marker", "_log_noise_prefixes",
    "_worker_final_output", "_classify_dead_worker_exit", "_classify_dead_worker",
    # claimed siblings
    "_classify_worker_exit", "_exit_code_kind", "_error_fingerprint",
    "_protocol_violation_streak", "_record_task_failure", "_worker_alive",
    "_process_fingerprint",
    # _kb collaborators
    "kb.read_worker_log", "kb.write_txn.__enter__", "kb.write_txn.__exit__",
    "kb._host_prefix", "kb._to_epoch", "kb._row_get",
    "kb._resolve_crash_grace_seconds", "kb._retry_status_for_run", "kb._end_run",
    "kb._append_event", "kb._json_or",
    # in-function imports
    "owner_reclaim_blocked", "managed_run_exit_code", "apply_attempt_note", "i18n.t",
    # infrastructure
    "conn.execute", "time.time",
]
# top-level targets nothing else calls are never C seams
C_EXPECTED = set(SEAM_NAMES) - {
    "_worker_log_exit_code", "_exit_summary_marker", "_log_noise_prefixes",
    "_worker_final_output", "_classify_dead_worker", "_reclaim_dead_workers",
    "_account_crashes", "_is_handoff_event", "adopt_worker_pid", "describe_suppression",
}


def cross_check():
    csrc = open(_REPO / "src" / "cli" / "kanban_db_dispatch_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_seams = set(re.findall(r'kdd2_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_kanban_db_dispatch_flow2: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    missing_c = [n for n in C_EXPECTED if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_kanban_db_dispatch_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|late_rec)\(\s*\n?\s*(?:ctx,\s*)?"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_kanban_db_dispatch_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "kanban_db_dispatch_flow2.jsonl"
    if _os_real.path.exists(fix):
        fix_keys, funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
            funcs.add(case["func"])
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_kanban_db_dispatch_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_kanban_db_dispatch_flow2: fixture funcs not extracted: {sorted(badf)}")
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
    print(f"ORACLE kanban_db_dispatch_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "kanban_db_dispatch_flow2.jsonl"
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
    spot = drive("describe_suppression", {"args": {"results": []}})
    assert spot["ret"] == "", spot
    spot = drive("describe_suppression", {"args": {"results": [
        {"respawn_guarded": [["t1", "active_pr"], ["t2", "active_pr"]],
         "rate_limited": ["t3"], "skipped_locked": True, "memory_pressure": "critical"}]}})
    assert spot["ret"] == "active_pr=2, rate_limited=1, skipped_locked=1, memory_pressure=critical", spot
    spot = drive("_worker_log_exit_code", {"args": {"task_id": "t"},
                                           "io": {"kb.read_worker_log": "x\n[kanban-worker-exit] rc=75\n"}})
    assert spot["ret"] == 75, spot
    spot = drive("_worker_log_exit_code", {"args": {"task_id": "t"},
                                           "io": {"kb.read_worker_log": {"__script__": True, "raise": {"type": "FileNotFoundError", "message": "no log"}}}})
    assert spot["ret"] is None, spot
    spot = drive("_exit_summary_marker", {"io": {"i18n.t": "Resume"}})
    assert spot["ret"] == "Resume" and spot["trace"][0]["args"] == {"key": "cli.session.exit_resume_hint"}, spot
    spot = drive("_log_noise_prefixes", {"io": {"i18n.t": "Init"}})
    assert spot["ret"] == ["session_id:", "Query:", "Init"], spot
    spot = drive("_worker_final_output", {"args": {"task_id": "t"},
                                          "io": {"kb.read_worker_log": "real reason\n",
                                                 "_exit_summary_marker": "EXIT",
                                                 "_log_noise_prefixes": {"__script__": True, "ret": ["session_id:", "Query:", "Init"]}}})
    assert spot["ret"] == "real reason", spot
    spot = drive("run_outcome", {"args": {"dead": {"kind": "rate_limited", "code": 75, "error_text": "e",
                                                   "event_kind": "rate_limited", "event_payload": {}, "rate_limited": True}}})
    assert spot["ret"] == "rate_limited", spot
    spot = drive("run_outcome", {"args": {"dead": {"kind": "nonzero_exit", "code": 1, "error_text": "e",
                                                   "event_kind": "crashed", "event_payload": {}}}})
    assert spot["ret"] == "crashed", spot
    spot = drive("_is_handoff_event", {"args": {"kind": "comment", "payload": None}})
    assert spot["ret"] is True and spot["trace"] == [], spot
    spot = drive("_is_handoff_event", {"args": {"kind": "assigned", "payload": "p"},
                                       "io": {"kb._json_or": {"assignee": "b", "from": "a"}}})
    assert spot["ret"] is True, spot
    spot = drive("_is_handoff_event", {"args": {"kind": "assigned", "payload": "p"},
                                       "io": {"kb._json_or": {"assignee": "b", "from": "b"}}})
    assert spot["ret"] is False, spot
    spot = drive("_classify_dead_worker_exit", {"args": {"pid": 5, "claimer": "c"},
                                                "io": {"_classify_worker_exit": {"__script__": True, "ret": ["signaled", 9]}}})
    assert spot["ret"]["error_text"] == "pid 5 killed by signal 9", spot
    assert spot["ret"]["event_payload"]["exit_code"] == 9, spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST kanban_db_dispatch_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    main()
