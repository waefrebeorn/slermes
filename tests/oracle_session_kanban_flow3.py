#!/usr/bin/env python3
"""Oracle for the session_kanban remainder FLOW port
(src/gateway/session_kanban_flow3.c).

MODULE RESOLUTION: gateway/session_kanban.py is fully claimed by
tui_methods_kanban_flow.c (all 22 defs). session_kanban_flow2.c already ported
the turn-recovery engine (agent/kanban_turn_recovery.py) and the worker exit
policy (hermes_cli/turn_exit.py). The unclaimed REMAINDER of the flow is the
kanban DB / dispatch / process-identity / session-key collaborator cluster
session_kanban.py drives but tmk + flow2 SEAM. Those are the REAL defining
modules the port credits:
  hermes_cli/kanban_db.py, hermes_cli/kanban_db_dispatch.py,
  hermes_cli/process_identity.py, session_keys.py.

Drives the LIVE bodies with SCRIPTED seams: every module global (os.environ /
time / Path / Task / the board resolvers), every in-function import
(gateway.status / gateway.drain_control / psutil) and every ported sibling is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace"), the final return ("ret") and the module-global mirror ("state":
os.environ + psutil flags, echoed verbatim — nothing here mutates state).

Live defs are AST-extracted from the four modules (targeted module Assign
constants seeded, FunctionDefs exec'd with the __future__ annotations flag into
registered sys.modules modules — py3.14). The heavy quarry import chain is
never imported. Ported siblings are SEAMED inside flows (each is exercised by
its own direct cases, where the real body runs).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE session_kanban_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds pass through every except-Exception site. The FULL
  builtin hierarchy plus the quarry customs ride make_exc.
- os.environ is a STATE dict (in.state.environ); psutil flags are STATE
  (in.state.psutil.{MACOS,LINUX}); reads are direct (no seam).
- conn rows are fixture dicts (key access); the fixture domain carries every
  key a flow reads. task records materialize as SimpleNamespace; an absent
  attribute read raises "'types.SimpleNamespace' object has no attribute ...".
  proc is always a SimpleNamespace double built from the args.proc spec.
  Paths are POSIX; Path.expanduser monkeypatched; joinpath/'/' fold to
  normalized POSIX concatenation.
- Tuples project to lists; sets to sorted lists; integral floats to ints.
- Seam args drop None-valued TOP-LEVEL keys on both sides; nested nulls
  (conn.execute params) are preserved.
- _json_or models json.loads with a strict grammar; _json_or_null models
  json.dumps(ensure_ascii=False). Fixture DATA is ASCII.
"""
import ast
import copy
import json
import os as _os_real
import re
import sys
import textwrap
import types
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
KB_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "kanban_db.py")
DISP_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "kanban_db_dispatch.py")
PI_SRC_PATH = _os_real.path.join(Q, "hermes_cli", "process_identity.py")
SK_SRC_PATH = _os_real.path.join(Q, "session_keys.py")

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
PersistenceError = type("PersistenceError", (Exception,), {})
AuthorityRefusal = type("AuthorityRefusal", (PersistenceError, ValueError), {})
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
    "PersistenceError": PersistenceError, "AuthorityRefusal": AuthorityRefusal,
    "sqlite3.Error": SqliteError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


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


# ── per-case script backend (mirrors the C sk3_call exactly) ──
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


# ── Path projections (POSIX fixture scope) ──
def _path_expanduser(self):
    return _CUR_CTX.call("path.expanduser", {"path": self.as_posix()})


for _cls in (Path, PurePath, PurePosixPath, PureWindowsPath, PosixPath, WindowsPath):
    _cls.expanduser = _path_expanduser


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


def _ns_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


# ── registered stub modules (in-function import resolution) ──
HC_PKG = new_module("hermes_cli")
KB_MOD = new_module("hermes_cli.kanban_db")
HC_PKG.kanban_db = KB_MOD
DISP_MOD = new_module("hermes_cli.kanban_db_dispatch")
HC_PKG.kanban_db_dispatch = DISP_MOD
PI_MOD = new_module("hermes_cli.process_identity")
HC_PKG.process_identity = PI_MOD
GW_PKG = new_module("gateway")
GS_MOD = new_module("gateway.status")
GW_PKG.status = GS_MOD
DC_MOD = new_module("gateway.drain_control")
GW_PKG.drain_control = DC_MOD
SK_MOD = new_module("session_keys")

# ── extract the live bodies ──
KB_SRC = open(KB_SRC_PATH, encoding="utf-8").read()
DISP_SRC = open(DISP_SRC_PATH, encoding="utf-8").read()
PI_SRC = open(PI_SRC_PATH, encoding="utf-8").read()
SK_SRC = open(SK_SRC_PATH, encoding="utf-8").read()

NS_KB = {"__name__": "hermes_cli.kanban_db", "__builtins__": __builtins__,
         "re": re, "json": json, "Path": Path, "time": TIME_DOUBLE}
seed_named(KB_SRC, NS_KB, KB_SRC_PATH,
           {"_BOARD_SLUG_RE", "DEFAULT_BOARD", "_RUN_OUTCOME_TERMINAL_STATUS"})
NS_DISP = {"__name__": "hermes_cli.kanban_db_dispatch", "__builtins__": __builtins__}
seed_named(DISP_SRC, NS_DISP, DISP_SRC_PATH,
           {"KANBAN_TERMINAL_TIMEOUT_GRACE_SECONDS", "UNVERIFIED_WORKER_FINGERPRINT"})
NS_PI = {"__name__": "hermes_cli.process_identity", "__builtins__": __builtins__}
NS_SK = {"__name__": "session_keys", "__builtins__": __builtins__}

KB_PORTED = ["_normalize_board_slug", "_require_slug", "_board_path", "kanban_db_path",
             "workspaces_root", "get_task", "_json_or", "_json_or_null", "_append_event",
             "_current_run_id", "_latest_event", "_json_dict", "_row_get",
             "_retry_status_for_run", "goal_run_status"]
DISP_PORTED = ["_worker_terminal_timeout_env", "_process_fingerprint", "_pid_recycled", "_worker_alive"]
PI_PORTED = ["process_birth", "worker_birth"]
SK_PORTED = ["profile_from_session_key"]


def extract(src, ns, path, names):
    got = {}
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            exec_node(src, node, ns, path)
            got[node.name] = ns[node.name]
    return got


REAL_KB = extract(KB_SRC, NS_KB, KB_SRC_PATH, KB_PORTED)
REAL_DISP = extract(DISP_SRC, NS_DISP, DISP_SRC_PATH, DISP_PORTED)
REAL_PI = extract(PI_SRC, NS_PI, PI_SRC_PATH, PI_PORTED)
REAL_SK = extract(SK_SRC, NS_SK, SK_SRC_PATH, SK_PORTED)

PORTED = KB_PORTED + DISP_PORTED + PI_PORTED + SK_PORTED
_missing_defs = [nm for nm in PORTED
                 if nm not in REAL_KB and nm not in REAL_DISP
                 and nm not in REAL_PI and nm not in REAL_SK]
if _missing_defs:
    print(f"FATAL oracle_session_kanban_flow3: defs not extracted: {_missing_defs}")
    sys.exit(2)

# ── sibling seam arg projections ──
KB_SIB_ARGS = {
    "_normalize_board_slug": lambda slug: {"slug": slug},
    "_board_path": lambda env_var, board, default_parts, leaf:
        {"env_var": env_var, "board": board, "default_parts": list(default_parts), "leaf": leaf},
    "get_task": lambda conn, task_id: {"task_id": task_id},
    "_json_or": lambda value, default=None: {"value": value, "default": default},
    "_json_or_null": lambda obj: {"obj": obj},
    "_current_run_id": lambda conn, task_id: {"task_id": task_id},
    "_latest_event": lambda conn, task_id, kind, run_id=None:
        {"task_id": task_id, "kind": kind, "run_id": run_id},
    "_json_dict": lambda value: {"value": value},
    "_row_get": lambda row, col, default=None: {"row": row, "col": col, "default": default},
}
DISP_SIB_ARGS = {
    "_process_fingerprint": lambda pid: {"pid": pid},
    "_pid_recycled": lambda pid, started_at: {"pid": pid, "started_at": started_at},
}
PI_SIB_ARGS = {
    "process_birth": lambda proc: {"proc": "proc"},
}


def build_proc(ctx, spec):
    p = types.SimpleNamespace()
    p.create_time = rec(ctx, "proc.create_time", lambda: {})
    if isinstance(spec, dict) and "_proc" in spec:
        if spec["_proc"] is None:
            p._proc = None
        else:
            pp = types.SimpleNamespace()
            pp.create_time = rec(ctx, "proc._proc.create_time",
                                 lambda monotonic=False: {"monotonic": monotonic})
            p._proc = pp
    return p


def install_stubs(ctx, state_in, func):
    for nm, real in REAL_KB.items():
        NS_KB[nm] = real
    for nm, real in REAL_DISP.items():
        NS_DISP[nm] = real
    for nm, real in REAL_PI.items():
        NS_PI[nm] = real
    for nm, real in REAL_SK.items():
        NS_SK[nm] = real
    for live in KB_PORTED:
        if live == func or live not in KB_SIB_ARGS:
            continue
        build = _ns_build if live == "get_task" else None
        NS_KB[live] = rec(ctx, live, KB_SIB_ARGS[live], build=build)
    for live in DISP_PORTED:
        if live == func or live not in DISP_SIB_ARGS:
            continue
        NS_DISP[live] = rec(ctx, live, DISP_SIB_ARGS[live])
    for live in PI_PORTED:
        if live == func or live not in PI_SIB_ARGS:
            continue
        NS_PI[live] = rec(ctx, live, PI_SIB_ARGS[live])

    # kanban_db external collaborators (always seamed)
    NS_KB["_explicit_board_slug"] = rec(ctx, "_explicit_board_slug", lambda board: {"board": board})
    NS_KB["_explicit_board_intent_pinned"] = rec(ctx, "_explicit_board_intent_pinned", lambda: {})
    NS_KB["get_current_board"] = rec(ctx, "get_current_board", lambda: {})
    NS_KB["kanban_home"] = rec(ctx, "kanban_home", lambda: {}, build=lambda raw: Path(raw))
    NS_KB["board_dir"] = rec(ctx, "board_dir", lambda board: {"board": board}, build=lambda raw: Path(raw))

    class _TaskDouble:
        pass
    _TaskDouble.from_row = staticmethod(rec(ctx, "task.from_row", lambda row: {"row": row}, build=_ns_build))
    NS_KB["Task"] = _TaskDouble

    env_dict = dict(state_in.get("environ") or {})
    os_double = types.SimpleNamespace(environ=env_dict)
    NS_KB["os"] = os_double
    NS_KB["time"] = TIME_DOUBLE
    sys.modules["time"] = TIME_DOUBLE

    # dispatch's _kb collaborator
    KB_MOD._pid_alive = rec(ctx, "_pid_alive", lambda pid: {"pid": pid})
    NS_DISP["_kb"] = KB_MOD

    # in-function imports
    GS_MOD.get_process_start_time = rec(ctx, "get_process_start_time", lambda pid: {"pid": pid})
    GS_MOD._start_times_agree = rec(ctx, "_start_times_agree",
                                    lambda current, started_at: {"current": current, "started_at": started_at})
    DC_MOD.current_instantiation_epoch = rec(ctx, "current_instantiation_epoch", lambda: {})
    sys.modules["gateway.status"] = GS_MOD
    sys.modules["gateway.drain_control"] = DC_MOD
    ps = state_in.get("psutil") or {}
    sys.modules["psutil"] = types.SimpleNamespace(
        MACOS=bool(ps.get("MACOS", False)), LINUX=bool(ps.get("LINUX", False)))

    NS_SK["profile_from_session_key_namespace"] = rec(
        ctx, "profile_from_session_key_namespace", lambda namespace: {"namespace": namespace})


def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = copy.deepcopy(inp.get("state") or {})
    saved = {m: sys.modules.get(m)
             for m in ("os", "time", "psutil", "gateway.status", "gateway.drain_control")}
    install_stubs(ctx, state_in, func)
    conn = _Conn(ctx)

    def kw(*names):
        return {k: args[k] for k in names if k in args}

    ret = None
    try:
        if func in ("_normalize_board_slug", "_require_slug"):
            ret = NS_KB[func](**kw("slug"))
        elif func == "_board_path":
            ret = NS_KB[func](**kw("env_var", "board", "default_parts", "leaf"))
        elif func in ("kanban_db_path", "workspaces_root"):
            ret = NS_KB[func](**kw("board"))
        elif func == "get_task":
            ret = NS_KB[func](conn, **kw("task_id"))
        elif func == "_json_or":
            ret = NS_KB[func](**kw("value", "default"))
        elif func == "_json_or_null":
            ret = NS_KB[func](**kw("obj"))
        elif func == "_append_event":
            ret = NS_KB[func](conn, **kw("task_id", "kind", "payload", "run_id"))
        elif func == "_current_run_id":
            ret = NS_KB[func](conn, **kw("task_id"))
        elif func == "_latest_event":
            ret = NS_KB[func](conn, **kw("task_id", "kind", "run_id"))
        elif func == "_json_dict":
            ret = NS_KB[func](**kw("value"))
        elif func == "_row_get":
            ret = NS_KB[func](**kw("row", "col", "default"))
        elif func == "_retry_status_for_run":
            ret = NS_KB[func](conn, **kw("task_id", "run_id"))
        elif func == "goal_run_status":
            ret = NS_KB[func](conn, **kw("task_id", "expected_run_id"))
        elif func == "_worker_terminal_timeout_env":
            ret = NS_DISP[func](**kw("max_runtime_seconds", "current_timeout"))
        elif func == "_process_fingerprint":
            ret = NS_DISP[func](**kw("pid"))
        elif func == "_pid_recycled":
            ret = NS_DISP[func](**kw("pid", "started_at"))
        elif func == "_worker_alive":
            ret = NS_DISP[func](**kw("pid", "started_at"))
        elif func in ("process_birth", "worker_birth"):
            if "proc" in args:
                ret = NS_PI[func](build_proc(ctx, args["proc"]))
            else:
                ret = NS_PI[func]()
        elif func == "profile_from_session_key":
            ret = NS_SK[func](**kw("session_key"))
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
    "_normalize_board_slug", "_board_path", "get_task", "_json_or", "_json_or_null",
    "_current_run_id", "_latest_event", "_json_dict", "_row_get",
    "_process_fingerprint", "_pid_recycled", "process_birth",
    # external collaborators
    "_explicit_board_slug", "_explicit_board_intent_pinned", "get_current_board",
    "kanban_home", "board_dir", "path.expanduser", "conn.execute", "task.from_row",
    "time.time", "get_process_start_time", "current_instantiation_epoch",
    "_start_times_agree", "_pid_alive", "proc.create_time", "proc._proc.create_time",
    "profile_from_session_key_namespace",
]
# top-level targets nothing else calls are never C seams
C_EXPECTED = set(SEAM_NAMES) - {
    "_require_slug", "kanban_db_path", "workspaces_root", "_append_event",
    "_retry_status_for_run", "goal_run_status", "_worker_terminal_timeout_env",
    "_worker_alive", "worker_birth", "profile_from_session_key",
}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_kanban_flow3.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_seams = set(re.findall(r'sk3_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_session_kanban_flow3: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    missing_c = [n for n in C_EXPECTED if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_kanban_flow3: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|late_rec)\(\s*\n?\s*(?:ctx,\s*)?"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_session_kanban_flow3: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_kanban_flow3.jsonl"
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
            print(f"FATAL oracle_session_kanban_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_session_kanban_flow3: fixture funcs not extracted: {sorted(badf)}")
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
    print(f"ORACLE session_kanban_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "session_kanban_flow3.jsonl"
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
    spot = drive("_normalize_board_slug", {"args": {"slug": "  My-Board_2  "}})
    assert spot["ret"] == "my-board_2", spot
    spot = drive("_require_slug", {"args": {"slug": "x"}, "io": {"_normalize_board_slug": None}})
    assert spot["ret"] == {"__raise__": "ValueError", "message": "board slug is required"}, spot
    spot = drive("_json_or", {"args": {"value": '{"a": 1}'}})
    assert spot["ret"] == {"a": 1}, spot
    spot = drive("_json_or", {"args": {"value": "01"}})
    assert spot["ret"] is None, spot
    spot = drive("_json_or_null", {"args": {"obj": {"a": 1, "b": [True, None]}}})
    assert spot["ret"] == '{"a": 1, "b": [true, null]}', spot
    spot = drive("_json_or_null", {"args": {"obj": {}}})
    assert spot["ret"] is None, spot
    spot = drive("_worker_terminal_timeout_env", {"args": {"max_runtime_seconds": 100, "current_timeout": None}})
    assert spot["ret"] == "70", spot
    spot = drive("_worker_terminal_timeout_env", {"args": {"max_runtime_seconds": 0, "current_timeout": None}})
    assert spot["ret"] is None, spot
    spot = drive("_worker_terminal_timeout_env", {"args": {"max_runtime_seconds": 10, "current_timeout": None}})
    assert spot["ret"] == "1", spot
    spot = drive("profile_from_session_key", {"args": {"session_key": "agent:beta:xyz"},
                                              "io": {"profile_from_session_key_namespace": "beta"}})
    assert spot["ret"] == "beta", spot
    spot = drive("profile_from_session_key", {"args": {"session_key": "agent::xyz"},
                                              "io": {"profile_from_session_key_namespace": "default"}})
    assert spot["trace"][0]["args"] == {"namespace": "main"}, spot
    spot = drive("_pid_recycled", {"args": {"pid": 5, "started_at": "unverified"}})
    assert spot["ret"] is True, spot
    spot = drive("_worker_alive", {"args": {"pid": 5, "started_at": None}, "io": {"_pid_alive": False}})
    assert spot["ret"] is False, spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST session_kanban_flow3: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    main()
