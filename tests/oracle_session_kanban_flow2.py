#!/usr/bin/env python3
"""Oracle for the session_kanban remainder FLOW port
(src/gateway/session_kanban_flow2.c).

MODULE RESOLUTION: gateway/session_kanban.py is fully claimed by
tui_methods_kanban_flow.c (all 22 defs). The unclaimed REMAINDER of that flow
is the collaborator cluster session_kanban.py drives but tmk SEAMS:
agent/kanban_turn_recovery.py (recover_failed_kanban_turns <- _run_task_turns,
worker_claim_is_live <- _bound_claim_is_live) and hermes_cli/turn_exit.py
(turn_exit_code / credential_failure_flags <- _run_worker_turns /
record_start_failure). Those are the REAL defining modules the port credits.

Drives the LIVE recovery-engine and exit-policy bodies with SCRIPTED seams:
every module global (os.environ / os.getpid / time / sqlite3 / Path / logger),
every callback and every ported sibling is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
return ("ret") and the module-global mirror ("state": os.environ, echoed
verbatim — nothing here mutates state).

Live defs are AST-extracted from agent/kanban_turn_recovery.py and
hermes_cli/turn_exit.py (module Assign constants seeded, FunctionDefs exec'd
with the __future__ annotations flag into registered sys.modules modules —
py3.14). FailoverReason is AST-extracted from agent/error_classifier.py so
_NON_RECOVERABLE_REASONS resolves to the real enum values. The heavy quarry
import chain is never imported. Ported siblings are SEAMED inside flows (each
is exercised by its own direct cases, where the real body runs).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE session_kanban_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds pass through every except-Exception site. The FULL
  builtin hierarchy plus AuthError(RuntimeError) ride make_exc.
- credential_failure_flags models the cause/context chain as a fixture ARRAY
  of node descriptors (node 0 = head); cause/context are INDICES (null=None).
  Distinct indices are distinct objects (id() identity); a self-index is a
  real cycle the seen-set walk terminates exactly as live.
- os.environ is a STATE dict (in.state.environ); reads are direct (no seam).
- conn rows are fixture dicts (key access); str()/slicing fold the ASCII
  corpus; Paths are POSIX (Path.exists/absolute/as_uri monkeypatched live).
- Tuples project to lists; sets to sorted lists; integral floats to ints.
- Seam args drop None-valued TOP-LEVEL keys on both sides; nested nulls
  (log positional args) are preserved.
- Source literals carry their exact UTF-8 bytes (em-dash / ellipsis); fixture
  DATA is ASCII.
"""
import ast
import copy
import enum as _enum
import json
import os as _os_real
import re
import sqlite3 as _sqlite3_real
import sys
import textwrap
import types
import typing
import __future__ as _fut
from pathlib import Path, PurePath, PurePosixPath, PureWindowsPath, PosixPath, WindowsPath

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
KTR_SRC_PATH = _os_real.path.join(Q, "agent", "kanban_turn_recovery.py")
TE_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "turn_exit.py")
EC_SRC_PATH = _os_real.path.join(Q, "agent", "error_classifier.py")

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


# ── exception classes (shared identity between make_exc and except sites) ──
PersistenceError = type("PersistenceError", (Exception,), {})
AuthorityRefusal = type("AuthorityRefusal", (PersistenceError, ValueError), {})
SqliteError = type("sqlite3.Error", (Exception,), {})
JSONDecodeError = type("JSONDecodeError", (ValueError,), {})
AuthError = type("AuthError", (RuntimeError,), {})

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
    "PersistenceError": PersistenceError, "AuthorityRefusal": AuthorityRefusal,
    "sqlite3.Error": SqliteError, "AuthError": AuthError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    e = cls(spec.get("message", ""))
    if isinstance(e, AuthError):
        e.relogin_required = spec.get("relogin_required", False)
        e.code = spec.get("code")
    return e


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
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
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── per-case script backend (mirrors the C sk2_call exactly) ──
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


# late-binding recorder (reads the current case ctx at CALL time) — used for
# module globals captured at def time (time.sleep default) and the logger.
_CUR_CTX = None
NODE_OF = {}


def late_rec(name, args_fn=None, build=None):
    def f(*a, **k):
        ctx = _CUR_CTX
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    f.__name__ = name
    return f


def _log_args(fmt, *a, exc_info=None):
    return {"fmt": fmt, "args": list(a) if a else None, "exc_info": exc_info or None}


LOGGER = types.SimpleNamespace(
    warning=late_rec("log.warning", _log_args),
    debug=late_rec("log.debug", _log_args))
LOGGING_MOD = types.SimpleNamespace(getLogger=lambda name: LOGGER)
TIME_DOUBLE = types.SimpleNamespace(
    time=late_rec("time.time", lambda: {}),
    sleep=late_rec("time.sleep", lambda seconds: {"seconds": seconds}))


# ── Path projections (POSIX fixture scope) ──
def _path_exists(self):
    return _CUR_CTX.call("path.exists", {"path": self.as_posix()})


def _path_absolute(self):
    return self   # identity on the absolute-POSIX fixture domain


def _path_as_uri(self):
    return _CUR_CTX.call("path.as_uri", {"path": self.as_posix()})


Path.exists = _path_exists
Path.absolute = _path_absolute
# WindowsPath/PureWindowsPath OVERRIDE as_uri (drive-relative POSIX paths raise
# "relative paths can't be expressed as file URIs"); patch every concrete class
# so the live reference uses the seam model on this Windows box.
for _cls in (PurePath, PurePosixPath, PureWindowsPath, PosixPath, WindowsPath):
    _cls.as_uri = _path_as_uri
    _cls.absolute = _path_absolute
    _cls.exists = _path_exists


# ── sqlite doubles ──
class _Cursor:
    def __init__(self, raw):
        self.raw = raw

    def fetchone(self):
        return self.raw


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx
        self.row_factory = None

    def execute(self, sql, params=()):
        raw = self.ctx.call("conn.execute", {"sql": sql, "params": list(params)})
        return _Cursor(raw)

    def close(self):
        return self.ctx.call("conn.close", {})


# ── FailoverReason (AST-extracted so _NON_RECOVERABLE_REASONS uses real values) ──
KTR_SRC = open(KTR_SRC_PATH, encoding="utf-8").read()
TE_SRC = open(TE_SRC_PATH, encoding="utf-8").read()
EC_SRC = open(EC_SRC_PATH, encoding="utf-8").read()

FailoverReason = None
for _node in ast.parse(EC_SRC).body:
    if isinstance(_node, ast.ClassDef) and _node.name == "FailoverReason":
        _ns = {"enum": _enum, "__name__": "agent.error_classifier"}
        exec_node(EC_SRC, _node, _ns, EC_SRC_PATH)
        FailoverReason = _ns["FailoverReason"]
if FailoverReason is None:
    print("FATAL oracle_session_kanban_flow2: FailoverReason not extracted")
    sys.exit(2)

# ── registered stub modules (in-function import resolution) ──
AGENT_PKG = new_module("agent")
KTR_MOD = new_module("agent.kanban_turn_recovery")
AGENT_PKG.kanban_turn_recovery = KTR_MOD
EC_MOD = new_module("agent.error_classifier")
AGENT_PKG.error_classifier = EC_MOD
EC_MOD.FailoverReason = FailoverReason
HC_PKG = new_module("hermes_cli")
TE_MOD = new_module("hermes_cli.turn_exit")
HC_PKG.turn_exit = TE_MOD
AUTH_MOD = new_module("hermes_cli.auth")
HC_PKG.auth = AUTH_MOD
AUTH_MOD.AuthError = AuthError
KB_MOD = new_module("hermes_cli.kanban_db")
HC_PKG.kanban_db = KB_MOD
KB_MOD.KANBAN_RATE_LIMIT_EXIT_CODE = 75
KB_MOD.KANBAN_TERMINAL_PROVIDER_EXIT_CODE = 78

# ── extract the recovery engine ──
NS_KTR = {
    "__name__": "agent.kanban_turn_recovery", "__builtins__": __builtins__,
    "logging": LOGGING_MOD, "time": TIME_DOUBLE, "Path": Path,
    "FailoverReason": FailoverReason, "os": _os_real, "sqlite3": _sqlite3_real,
    "Any": typing.Any, "Callable": typing.Callable, "Optional": typing.Optional,
}
seed_constants(KTR_SRC, NS_KTR, KTR_SRC_PATH)
NS_KTR["logger"] = LOGGER   # re-set after the Assign seeding loop

KTR_FUNCS = [
    "kanban_task_id", "_resolve_task_id", "kanban_turn_recovery_enabled",
    "max_recovery_attempts", "recovery_delay_seconds", "_turn_exit_is_terminal",
    "should_recover_turn", "_kanban_db_path", "_now", "worker_claim_is_live",
    "_truncate", "build_recovery_nudge", "_emit_recovery_skipped",
    "recover_failed_kanban_turns",
]
for _node in ast.parse(KTR_SRC).body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in KTR_FUNCS:
        exec_node(KTR_SRC, _node, NS_KTR, KTR_SRC_PATH)
REAL_KTR = {nm: NS_KTR[nm] for nm in KTR_FUNCS if nm in NS_KTR}

# ── extract the exit policy ──
NS_TE = {"__name__": "hermes_cli.turn_exit", "__builtins__": __builtins__}
seed_constants(TE_SRC, NS_TE, TE_SRC_PATH)
TE_FUNCS = ["turn_exit_code", "credential_failure_flags"]
for _node in ast.parse(TE_SRC).body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in TE_FUNCS:
        exec_node(TE_SRC, _node, NS_TE, TE_SRC_PATH)
REAL_TE = {nm: NS_TE[nm] for nm in TE_FUNCS if nm in NS_TE}

PORTED = KTR_FUNCS + TE_FUNCS
_missing_defs = [nm for nm in PORTED if nm not in REAL_KTR and nm not in REAL_TE]
if _missing_defs:
    print(f"FATAL oracle_session_kanban_flow2: defs not extracted: {_missing_defs}")
    sys.exit(2)

# ── sibling seam arg projections ──
ARGS_FN_KTR = {
    "kanban_task_id": lambda: {},
    "_resolve_task_id": lambda task_id: {"task_id": task_id},
    "kanban_turn_recovery_enabled": lambda task_id=None: {"task_id": task_id},
    "max_recovery_attempts": lambda: {},
    "recovery_delay_seconds": lambda attempt: {"attempt": attempt},
    "_turn_exit_is_terminal": lambda result: {"result": result},
    "should_recover_turn": lambda result, *, attempt, task_id=None:
        {"result": result, "attempt": attempt, "task_id": task_id},
    "_kanban_db_path": lambda: {},
    "_now": lambda: {},
    "worker_claim_is_live": lambda: {},   # only ever the default check, no args
    "_truncate": lambda text, limit=300: {"text": text, "limit": limit},
    "build_recovery_nudge": lambda result, *, attempt, max_attempts, task_id=None:
        {"result": result, "attempt": attempt, "max_attempts": max_attempts, "task_id": task_id},
    "_emit_recovery_skipped": lambda emit, task_id=None:
        {"emit": emit.__name__ if emit is not None else None, "task_id": task_id},
    "recover_failed_kanban_turns": lambda turn_fn, get_result, *, sleep_fn=None, emit=None,
        claim_check=None, task_id=None:
        {"turn_fn": turn_fn.__name__, "get_result": get_result.__name__, "task_id": task_id,
         "claim_check": claim_check.__name__ if claim_check else None},
}
ARGS_FN_TE = {
    "turn_exit_code": lambda result, *, kanban_worker, **kw:
        {"result": result, "kanban_worker": kanban_worker, **kw},
    "credential_failure_flags": lambda exc: {"exc": proj_exc(exc)},
}


def install_stubs(ctx, state_in, func):
    for nm, real in REAL_KTR.items():
        NS_KTR[nm] = real
    for live in KTR_FUNCS:
        if live == func:
            continue
        NS_KTR[live] = rec(ctx, live, ARGS_FN_KTR[live])
    for nm, real in REAL_TE.items():
        NS_TE[nm] = real
    for live in TE_FUNCS:
        if live == func:
            continue
        NS_TE[live] = rec(ctx, live, ARGS_FN_TE[live])

    env_dict = dict(state_in.get("environ") or {})
    os_double = types.SimpleNamespace(
        getpid=rec(ctx, "os.getpid", lambda: {}), environ=env_dict)
    NS_KTR["os"] = os_double
    sys.modules["os"] = os_double
    sqlite3_double = types.SimpleNamespace(
        connect=rec(ctx, "sqlite3.connect", lambda path, uri=False: {"path": path, "uri": uri},
                    build=lambda raw: _Conn(ctx)),
        Row=object(), Error=SqliteError)
    NS_KTR["sqlite3"] = sqlite3_double
    sys.modules["sqlite3"] = sqlite3_double
    sys.modules["time"] = TIME_DOUBLE
    sys.modules["logging"] = LOGGING_MOD
    NS_KTR["logger"] = LOGGER
    AUTH_MOD.is_rate_limited_auth_error = rec(
        ctx, "is_rate_limited_auth_error",
        lambda error: {"error": NODE_OF[id(error)] if id(error) in NODE_OF else proj_exc(error)})


def build_exc_chain(spec):
    global NODE_OF
    if not isinstance(spec, list) or not spec:
        return None
    objs = []
    for node in spec:
        kind = node.get("kind", "RuntimeError")
        cls = EXC_CLASSES.get(kind)
        if cls is None:
            cls = type(kind, (Exception,), {})
        e = cls(node.get("message", ""))
        if isinstance(e, AuthError):
            e.relogin_required = node.get("relogin_required", False)
            e.code = node.get("code")
        objs.append(e)
        NODE_OF[id(e)] = node
    for i, node in enumerate(spec):
        c, x = node.get("cause"), node.get("context")
        objs[i].__cause__ = objs[c] if isinstance(c, int) and 0 <= c < len(objs) else None
        objs[i].__context__ = objs[x] if isinstance(x, int) and 0 <= x < len(objs) else None
    return objs[0]


def drive(func, inp):
    global _CUR_CTX, NODE_OF
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    NODE_OF = {}
    state_in = copy.deepcopy(inp.get("state") or {})
    saved = {m: sys.modules.get(m) for m in ("os", "time", "logging", "sqlite3")}
    install_stubs(ctx, state_in, func)
    ret = None
    try:
        if func == "kanban_task_id":
            ret = NS_KTR[func]()
        elif func == "_resolve_task_id":
            ret = NS_KTR[func](args.get("task_id"))
        elif func == "kanban_turn_recovery_enabled":
            ret = NS_KTR[func](args.get("task_id"))
        elif func == "max_recovery_attempts":
            ret = NS_KTR[func]()
        elif func == "recovery_delay_seconds":
            ret = NS_KTR[func](args["attempt"]) if "attempt" in args else NS_KTR[func]()
        elif func == "_turn_exit_is_terminal":
            ret = NS_KTR[func](args.get("result"))
        elif func == "should_recover_turn":
            kw = {}
            if "attempt" in args:
                kw["attempt"] = args["attempt"]
            if "task_id" in args:
                kw["task_id"] = args["task_id"]
            ret = NS_KTR[func](args.get("result"), **kw)
        elif func == "_kanban_db_path":
            ret = NS_KTR[func]()
        elif func == "_now":
            ret = NS_KTR[func]()
        elif func == "worker_claim_is_live":
            kw = {k: args[k] for k in ("task_id", "db_path", "run_id", "claim_lock", "verify_pid")
                  if k in args}
            ret = NS_KTR[func](**kw)
        elif func == "_truncate":
            kw = {"limit": args["limit"]} if "limit" in args else {}
            ret = NS_KTR[func](args.get("text"), **kw)
        elif func == "build_recovery_nudge":
            kw = {k: args[k] for k in ("attempt", "max_attempts", "task_id") if k in args}
            ret = NS_KTR[func](args.get("result"), **kw)
        elif func == "_emit_recovery_skipped":
            kw = {}
            if "emit" in args:
                kw["emit"] = rec(ctx, "emit", lambda message: {"message": message}) if args.get("emit") else None
            if "task_id" in args:
                kw["task_id"] = args["task_id"]
            ret = NS_KTR[func](**kw)
        elif func == "recover_failed_kanban_turns":
            pos = []
            if "turn_fn" in args:
                pos.append(rec(ctx, "turn_fn", lambda nudge: {"nudge": nudge}))
            if "get_result" in args:
                pos.append(rec(ctx, "get_result", lambda: {}))
            kw = {}
            if args.get("sleep_fn"):
                kw["sleep_fn"] = rec(ctx, "sleep_fn", lambda seconds: {"seconds": seconds})
            if args.get("emit"):
                kw["emit"] = rec(ctx, "emit", lambda message: {"message": message})
            if args.get("claim_check"):
                kw["claim_check"] = rec(ctx, "claim_check", lambda: {})
            if "task_id" in args:
                kw["task_id"] = args["task_id"]
            ret = NS_KTR[func](*pos, **kw)
        elif func == "turn_exit_code":
            kw = {k: args[k] for k in ("kanban_worker", "credentials_rate_limited",
                  "credentials_terminal", "transient_reasons", "terminal_reasons") if k in args}
            ret = NS_TE[func](args.get("result"), **kw)
        elif func == "credential_failure_flags":
            ret = NS_TE[func](build_exc_chain(args.get("exc")))
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
        NODE_OF = {}
    state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "kanban_task_id", "_resolve_task_id", "kanban_turn_recovery_enabled",
    "max_recovery_attempts", "recovery_delay_seconds", "_turn_exit_is_terminal",
    "should_recover_turn", "_kanban_db_path", "_now", "worker_claim_is_live",
    "_truncate", "build_recovery_nudge", "_emit_recovery_skipped",
    "recover_failed_kanban_turns", "claim_check", "get_result", "turn_fn", "emit",
    "sleep_fn", "time.sleep", "time.time", "os.getpid", "path.exists", "path.as_uri",
    "sqlite3.connect", "conn.execute", "conn.close", "log.warning", "log.debug",
    "is_rate_limited_auth_error", "turn_exit_code", "credential_failure_flags",
]
# the three top-level targets nothing else calls are never C seams
C_EXPECTED = set(SEAM_NAMES) - {"turn_exit_code", "credential_failure_flags",
                                "recover_failed_kanban_turns"}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_kanban_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_seams = set(re.findall(r'sk2_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_session_kanban_flow2: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    # presence by quoted name (some seams are called via a variable: check_name
    # = claim_check/worker_claim_is_live, sleep_name = sleep_fn/time.sleep)
    missing_c = [n for n in C_EXPECTED if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_kanban_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|late_rec)\(\s*\n?\s*(?:ctx,\s*)?"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_session_kanban_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_kanban_flow2.jsonl"
    if _os_real.path.exists(fix):   # Path.exists is monkeypatched to the seam model
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
            print(f"FATAL oracle_session_kanban_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_session_kanban_flow2: fixture funcs not extracted: {sorted(badf)}")
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
    print(f"ORACLE session_kanban_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "session_kanban_flow2.jsonl"
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
    spot = drive("turn_exit_code", {"args": {"result": {"completed": True}, "kanban_worker": True}})
    assert spot["ret"] == 0 and spot["trace"] == [], spot
    spot = drive("turn_exit_code", {"args": {"result": None, "kanban_worker": True,
                                             "credentials_rate_limited": True}})
    assert spot["ret"] == 75, spot
    spot = drive("turn_exit_code", {"args": {"result": {"failed": True, "failure_reason": "auth"},
                                             "kanban_worker": True}})
    assert spot["ret"] == 78, spot
    spot = drive("max_recovery_attempts", {"state": {"environ": {"HERMES_KANBAN_TURN_RECOVERY": "5"}}})
    assert spot["ret"] == 5, spot
    spot = drive("recovery_delay_seconds", {"args": {"attempt": 2}})
    assert spot["ret"] == 45, spot
    spot = drive("_truncate", {"args": {"text": "a\nb"}})
    assert spot["ret"] == "a b", spot
    spot = drive("credential_failure_flags",
                 {"args": {"exc": [{"kind": "AuthError", "message": "m", "relogin_required": True}]},
                  "io": {"is_rate_limited_auth_error": False}})
    assert spot["ret"] == {"credentials_rate_limited": False, "credentials_terminal": True}, spot
    spot = drive("should_recover_turn",
                 {"args": {"result": {"failed": True, "failure_retryable": True}, "attempt": 0},
                  "io": {"kanban_turn_recovery_enabled": True, "max_recovery_attempts": 3,
                         "_turn_exit_is_terminal": False}})
    assert spot["ret"] is True, spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST session_kanban_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    main()