#!/usr/bin/env python3
"""Oracle for the TUI kanban method FLOW port
(src/gateway/tui_methods_kanban_flow.c).

MODULE RESOLUTION: the task named tui_gateway/methods_kanban.py, which does
NOT exist in the quarry. The TUI's only kanban RPC verb is "kanban.run"
(tui_gateway/contracts/canonical.py), dispatched by
gateway/session_controls.py:kanban_run to gateway/session_kanban.py. Those
are the REAL defining modules the port credits.

Drives the LIVE session_kanban engine bodies (and the kanban_run RPC method)
with SCRIPTED seams: every in-function import, module global, sqlite call,
Path projection and sibling flow is a recording stub fed the same scripts
the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace"), the final return
("ret") and the mutated module-global mirror ("state": _TURNS_DONE /
os.environ / the closure result box).

Live defs are AST-extracted from gateway/session_kanban.py (module
Assign/AnnAssign constants seeded, FunctionDefs/AsyncFunctionDefs exec'd
WITH their decorators into a registered sys.modules module — py3.14);
kanban_run is extracted from the SessionControls class body. The heavy
quarry import chain is never imported. Ported siblings are SEAMED inside
flows (each is exercised by its own direct cases, where the real body runs).
The six nested closures (admit_claim, watch, recover, status, block, turn)
are extracted from their parents and driven in a synthetic enclosing scope
that supplies the captured free variables; recover/turn read the mutated
nonlocal result back through a getter.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE tui_methods_kanban_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site. The FULL builtin
  hierarchy plus AuthorityRefusal(PersistenceError, ValueError),
  sqlite3.Error and JSONDecodeError(ValueError) ride make_exc, so
  except (ValueError, OSError, sqlite3.Error) catches AuthorityRefusal and
  JSONDecodeError exactly as live, while KeyError/TypeError pass through.
- Doubles: connection/actor/authority/store/ref/task/agent/saved/committed
  are SimpleNamespaces; policy is a dynamic "Policy" dataclass (replace()/
  asdict() run live); receipt is a dynamic dataclass (asdict). Seam args
  project markers: connection/authority/agent/conn/scope/process, ref ->
  session_id, actor/policy/frame/params/context -> dict projections.
- Path.resolve is monkeypatched to the "path.resolve" seam and returns a
  PurePosixPath (POSIX fixture scope; str() stays POSIX); PurePath.as_uri
  is monkeypatched to the "path.as_uri" seam. sqlite3.connect /
  conn.execute / conn.close are seams; the execute script carries the
  fetchone() row (dict for key access, [payload, ...] for index access).
  connect_closing returns a conn CM whose exit fires conn.close;
  write_txn / worker_turn_scope enter+exit are silent.
- os._exit(1) models as SystemExit(1) (the catchable analog of a hard exit).
- asyncio.to_thread records {fn: fn.__name__, args?, kwargs?} and returns
  the script WITHOUT calling fn (the off-loop result is scripted).
  tracked_write records, then runs its fn synchronously.
  recover_failed_kanban_turns does NOT drive its callbacks (recover/status/
  block/turn have their own cases); the lambdas project to "<lambda>".
- Tuples project to lists; sets to sorted lists; integral floats to ints.
- Seam args drop None-valued TOP-LEVEL keys on both sides; nested nulls
  (event payloads) are preserved.
- Getter scripts (kb.get_task) live in {null, non-empty record}: a live
  Task is always truthy, an empty {} double would diverge on `if task`.
- Payload / kanban_json strings are VALID JSON in the fixture domain.
"""
import ast
import asyncio as _asyncio_real
import contextlib
import copy
import dataclasses
import json
import os as _os_real
import re
import sys
import textwrap
import threading as _threading_real
import types
import __future__ as _fut
from pathlib import Path, PurePath, PurePosixPath

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
SRC = _os_real.path.join(Q, "gateway", "session_kanban.py")
CTRL_SRC = _os_real.path.join(Q, "gateway", "session_controls.py")

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

EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": _asyncio_real.CancelledError,
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
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "TypeError": TypeError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": JSONDecodeError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
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
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
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


# ── per-case script backend (mirrors the C tmk_call exactly) ──
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
    return f


def rec_async(ctx, name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def ns_double(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def _marker(name):
    def f(*a, **k):
        raise AssertionError(f"{name} must not be called directly")
    f.__name__ = name
    return f


# ── Path projections (POSIX fixture scope) ──
_CUR_CTX = None


def _path_resolve(self, strict=False):
    raw = _CUR_CTX.call("path.resolve", {"path": self.as_posix(), "strict": strict})
    return PurePosixPath(raw if isinstance(raw, str) else "")


def _path_as_uri(self):
    return _CUR_CTX.call("path.as_uri", {"path": self.as_posix()})


# keep the live __name__ ("resolve" / "as_uri") — to_thread projects fn.__name__
_path_resolve.__name__ = "resolve"
_path_as_uri.__name__ = "as_uri"
Path.resolve = _path_resolve
PurePath.as_uri = _path_as_uri


# ── sqlite / CM doubles ──
class _Cursor:
    def __init__(self, raw):
        self.raw = raw

    def fetchone(self):
        return self.raw


class _Conn:
    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, sql, params=()):
        raw = self.ctx.call("conn.execute", {"sql": sql, "params": list(params)})
        return _Cursor(raw)

    def close(self):
        return self.ctx.call("conn.close", {})


class _ConnCM:
    """connect_closing: yields a conn, fires conn.close on exit."""
    def __init__(self, ctx):
        self.ctx = ctx
        self.conn = _Conn(ctx)

    def __enter__(self):
        return self.conn

    def __exit__(self, *exc):
        self.conn.close()
        return False


class _SilentCM:
    def __enter__(self):
        return None

    def __exit__(self, *exc):
        return False


class _YieldCM:
    """Seamed _worker_workspace: the script is the yielded value."""
    def __init__(self, raw):
        self.raw = raw

    def __enter__(self):
        return self.raw

    def __exit__(self, *exc):
        return False


class _Thread:
    def __init__(self, ctx):
        self.start = rec(ctx, "thread.start", lambda: {})


class _Msg:
    def __init__(self, ctx):
        self.ctx = ctx

    def as_openai_message(self):
        return self.ctx.call("message.as_openai_message", {})


class _Log:
    def __init__(self, ctx):
        self.ctx = ctx

    def exception(self, fmt, *args):
        return self.ctx.call("log.exception", {"fmt": fmt})


def policy_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    fields = [(k, object) for k in raw]
    ns = {"config": lambda self: ctx.call("policy.config", {})}
    cls = dataclasses.make_dataclass("Policy", fields, namespace=ns)
    return cls(**raw)


def receipt_build(raw):
    if not isinstance(raw, dict):
        return raw
    cls = dataclasses.make_dataclass("Receipt", [(k, object) for k in raw])
    return cls(**raw)


def store_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    scope = dict(raw.get("scope") or {})
    if isinstance(scope.get("profile_dir"), str):
        scope["profile_dir"] = Path(scope["profile_dir"])
    return types.SimpleNamespace(
        scope=types.SimpleNamespace(**scope),
        sessions=raw.get("sessions") or {},
        bindings=types.SimpleNamespace(
            receipt=rec(ctx, "store.bindings.receipt", lambda sid: {"sid": sid},
                        build=ns_double)),
        admissions=types.SimpleNamespace(find=_marker("find")),
    )


def msg_list_build(raw, ctx):
    if isinstance(raw, list):
        return [_Msg(ctx) for _ in raw]
    return raw


# ── extraction ──
SRC_TEXT = open(SRC, encoding="utf-8").read()
CTRL_TEXT = open(CTRL_SRC, encoding="utf-8").read()

PORTED = [
    "build_kanban_policy", "run_task", "bind_worker_context", "_watch_submitter",
    "run_worker_turns", "_worker_workspace", "_run_worker_turns",
    "record_start_failure", "_record_worker_result", "record_worker_loss",
    "_bound_worker_matches", "_bound_claim_is_live", "worker_result",
    "worker_exit_code", "_stored_conversation", "_run_task_turns",
]
CLOSURES = ["admit_claim", "watch", "recover", "status", "block", "turn"]


def extract():
    mod = new_module("tmk_oracle_session_kanban")
    ns = mod.__dict__
    ns.update({
        "asyncio": _asyncio_real, "contextlib": contextlib,
        "closing": contextlib.closing, "asdict": dataclasses.asdict,
        "replace": dataclasses.replace, "threading": _threading_real,
        "json": json, "Path": Path, "sqlite3": __import__("sqlite3"),
        "AuthorityRefusal": AuthorityRefusal,
    })
    seed_constants(SRC_TEXT, ns, SRC)
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(SRC_TEXT, node, ns, SRC)
    # kanban_run from the SessionControls class body (dedented method)
    for node in ast.walk(ast.parse(CTRL_TEXT)):
        if isinstance(node, ast.AsyncFunctionDef) and node.name == "kanban_run":
            exec_node(CTRL_TEXT, node, ns, CTRL_SRC)
            break
    required = PORTED + ["kanban_run", "_TURNS_DONE", "KANBAN_TURN_AUTHOR"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_kanban_flow: session_kanban missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED + ["kanban_run"]}

# nested closure sources (for the synthetic enclosing scopes)
def closure_src(parent, name):
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == parent:
            for sub in ast.walk(node):
                if isinstance(sub, ast.FunctionDef) and sub.name == name:
                    return textwrap.dedent(seg(SRC_TEXT, sub))
    print(f"FATAL oracle_tui_methods_kanban_flow: closure {parent}.{name} not found")
    sys.exit(2)


CLOSURE_SRC = {
    "admit_claim": closure_src("run_task", "admit_claim"),
    "watch": closure_src("_watch_submitter", "watch"),
    "recover": closure_src("_run_task_turns", "recover"),
    "status": closure_src("_run_task_turns", "status"),
    "block": closure_src("_run_task_turns", "block"),
    "turn": closure_src("_run_task_turns", "turn"),
}

# live-name -> seam-name for the ported siblings
SIBLING_SEAMS = {
    "build_kanban_policy": "build_kanban_policy",
    "run_task": "run_task",
    "_watch_submitter": "watch_submitter",
    "_run_worker_turns": "run_worker_turns_inner",
    "_worker_workspace": "worker_workspace",
    "_record_worker_result": "record_worker_result",
    "_bound_worker_matches": "bound_worker_matches",
    "_stored_conversation": "stored_conversation",
    "_run_task_turns": "run_task_turns",
    "worker_result": "worker_result",
}


def sibling_args(live):
    if live == "build_kanban_policy":
        return lambda connection, params, config: {
            "connection": "connection", "params": params, "config": config}
    if live == "run_task":
        return lambda connection, params: {"connection": "connection", "params": params}
    if live == "_watch_submitter":
        return lambda db, run_id: {"db": db.as_posix(), "run_id": run_id}
    if live == "_run_worker_turns":
        return lambda agent, frame, history: {
            "agent": "agent", "frame": frame, "history": history}
    if live == "_worker_workspace":
        return lambda frame: {"frame": frame}
    if live == "_record_worker_result":
        return lambda context, code, last_output: {
            "context": context, "code": code, "last_output": last_output}
    if live == "_bound_worker_matches":
        return lambda conn, context: {"conn": "conn", "context": context}
    if live == "_bound_claim_is_live":
        return lambda context: {"context": context}
    if live == "_stored_conversation":
        return lambda agent: {"agent": "agent"}
    if live == "_run_task_turns":
        return lambda agent, frame, history, context: {
            "agent": "agent", "frame": frame, "history": history, "context": context}
    if live == "worker_result":
        return lambda path, params: {"path": path, "params": params}
    raise KeyError(live)


# ── sys.modules stub packages (created once, rebound per case) ──
for _pkg in ("hermes_cli", "gateway", "persistence", "persistence.core",
             "persistence.authority", "agent"):
    if _pkg not in sys.modules:
        new_module(_pkg)
    sys.modules[_pkg].__path__ = []

KB_MOD = new_module("hermes_cli.kanban_db")
KBC_MOD = new_module("hermes_cli.kanban_db_connect")
KBD_MOD = new_module("hermes_cli.kanban_db_dispatch")
PROFILES_MOD = new_module("hermes_cli.profiles")
TURN_EXIT_MOD = new_module("hermes_cli.turn_exit")
GOALS_MOD = new_module("hermes_cli.goals")
PROC_ID_MOD = new_module("hermes_cli.process_identity")
SESS_POLICY_MOD = new_module("gateway.session_policy")
GW_RUN_MOD = new_module("gateway.run")
SESS_LOCAL_MOD = new_module("gateway.session_local")
SESS_CONTRACT_MOD = new_module("gateway.session_contract")
SESS_RECOVERY_MOD = new_module("gateway.session_local_recovery")
SESS_PERSIST_MOD = new_module("gateway.session_persistence")
SESS_WORKERS_MOD = new_module("gateway.session_runtime_workers")
SESS_MANAGED_MOD = new_module("gateway.session_managed_worker")
SESS_SURFACE_MOD = new_module("gateway.session_surface")
SESS_AUTH_MOD = new_module("gateway.session_authorities")
SESS_CTX_MOD = new_module("gateway.session_context")
SESS_DISPLAY_MOD = new_module("gateway.session_display")
SESS_KANBAN_MOD = new_module("gateway.session_kanban")
BINDINGS_MOD = new_module("persistence.authority.bindings")
ERRORS_MOD = new_module("persistence.core.errors")
PASM_MOD = new_module("persistence_assembly")
SKEYS_MOD = new_module("session_keys")
KTR_MOD = new_module("agent.kanban_turn_recovery")
AIS_MOD = new_module("agent.agent_init_session")
SKILL_MOD = new_module("agent.skill_commands")
PSUTIL_MOD = new_module("psutil")
LOGGING_MOD = new_module("logging")

sys.modules["hermes_cli"].kanban_db = KB_MOD
sys.modules["hermes_cli"].kanban_db_connect = KBC_MOD
sys.modules["hermes_cli"].kanban_db_dispatch = KBD_MOD
sys.modules["hermes_cli"].profiles = PROFILES_MOD
sys.modules["hermes_cli"].turn_exit = TURN_EXIT_MOD
sys.modules["hermes_cli"].goals = GOALS_MOD
sys.modules["hermes_cli"].process_identity = PROC_ID_MOD
sys.modules["gateway"].session_policy = SESS_POLICY_MOD
sys.modules["gateway"].run = GW_RUN_MOD
sys.modules["gateway"].session_local = SESS_LOCAL_MOD
sys.modules["gateway"].session_contract = SESS_CONTRACT_MOD
sys.modules["gateway"].session_local_recovery = SESS_RECOVERY_MOD
sys.modules["gateway"].session_persistence = SESS_PERSIST_MOD
sys.modules["gateway"].session_runtime_workers = SESS_WORKERS_MOD
sys.modules["gateway"].session_managed_worker = SESS_MANAGED_MOD
sys.modules["gateway"].session_surface = SESS_SURFACE_MOD
sys.modules["gateway"].session_authorities = SESS_AUTH_MOD
sys.modules["gateway"].session_context = SESS_CTX_MOD
sys.modules["gateway"].session_display = SESS_DISPLAY_MOD
sys.modules["gateway"].session_kanban = SESS_KANBAN_MOD
sys.modules["persistence.authority"].bindings = BINDINGS_MOD
sys.modules["persistence.core"].errors = ERRORS_MOD
sys.modules["agent"].kanban_turn_recovery = KTR_MOD
sys.modules["agent"].agent_init_session = AIS_MOD
sys.modules["agent"].skill_commands = SKILL_MOD
ERRORS_MOD.AuthorityRefusal = AuthorityRefusal
KBD_MOD.UNVERIFIED_WORKER_FINGERPRINT = "unverified"
GOALS_MOD.DEFAULT_MAX_TURNS = 20


@dataclasses.dataclass
class _Submission:
    request_id: str
    ref: object
    payload: dict
    intent: str


SESS_CONTRACT_MOD.Submission = _Submission


def install_stubs(ctx, state_in):
    """Rebind every seam slot for one case. Returns (os_double, td_box)."""
    # ported siblings: restore real, then seam all but the target
    for nm, real in REAL.items():
        NS[nm] = real
    for live, seam in SIBLING_SEAMS.items():
        build = None
        if live == "_worker_workspace":
            build = lambda raw: _YieldCM(raw)
        if live == "build_kanban_policy":
            build = lambda raw: ([policy_build(raw[0], ctx), raw[1]]
                                 if isinstance(raw, list) and len(raw) == 2 else raw)
        if live == "run_task":
            NS[live] = rec_async(ctx, seam, sibling_args(live))
        else:
            NS[live] = rec(ctx, seam, sibling_args(live), build=build)
    SESS_KANBAN_MOD.run_task = NS["run_task"]

    # kanban_db family
    KB_MOD._require_slug = rec(ctx, "kb._require_slug", lambda slug: {"slug": slug})
    KB_MOD.kanban_db_path = rec(ctx, "kb.kanban_db_path",
                                lambda board=None: {"board": board},
                                build=lambda raw: Path(raw) if isinstance(raw, str) else raw)
    KB_MOD.get_task = rec(ctx, "kb.get_task",
                          lambda conn, task_id: {"conn": "conn", "task_id": task_id},
                          build=ns_double)
    KB_MOD.build_worker_context = rec(
        ctx, "kb.build_worker_context",
        lambda conn, task_id: {"conn": "conn", "task_id": task_id})
    KB_MOD._retry_status_for_run = rec(
        ctx, "kb._retry_status_for_run",
        lambda conn, task_id, run_id: {"conn": "conn", "task_id": task_id, "run_id": run_id})
    KB_MOD.workspaces_root = rec(ctx, "kb.workspaces_root",
                                 lambda board=None: {"board": board},
                                 build=lambda raw: Path(raw) if isinstance(raw, str) else raw)
    KB_MOD.write_txn = rec(ctx, "kb.write_txn", lambda conn: {"conn": "conn"},
                           build=lambda raw: _SilentCM())
    KB_MOD._append_event = rec(
        ctx, "kb._append_event",
        lambda conn, task_id, kind, payload, run_id=None:
        {"conn": "conn", "task_id": task_id, "kind": kind, "payload": payload,
         "run_id": run_id})
    KB_MOD.redact_review_value = rec(ctx, "kb.redact_review_value",
                                     lambda value: {"value": value})
    KB_MOD.goal_run_status = rec(
        ctx, "kb.goal_run_status",
        lambda conn, task_id, run_id: {"conn": "conn", "task_id": task_id, "run_id": run_id})
    KB_MOD.block_task = rec(
        ctx, "kb.block_task",
        lambda conn, task_id, reason=None, expected_run_id=None:
        {"conn": "conn", "task_id": task_id, "reason": reason,
         "expected_run_id": expected_run_id})
    KBC_MOD.connect_closing = rec(
        ctx, "kbc.connect_closing",
        lambda path=None: {"path": path.as_posix() if path is not None else None},
        build=lambda raw: _ConnCM(ctx))
    KBD_MOD._worker_terminal_timeout_env = rec(
        ctx, "kbd._worker_terminal_timeout_env",
        lambda max_runtime_seconds, current:
        {"max_runtime_seconds": max_runtime_seconds, "current": current})
    KBD_MOD._process_fingerprint = rec(ctx, "kbd._process_fingerprint",
                                       lambda pid: {"pid": pid})
    KBD_MOD._worker_alive = rec(ctx, "kbd._worker_alive",
                                lambda pid, started_at: {"pid": pid, "started_at": started_at})
    PROFILES_MOD.resolve_profile_env = rec(ctx, "resolve_profile_env",
                                           lambda profile: {"profile": profile})
    TURN_EXIT_MOD.turn_exit_code = rec(
        ctx, "turn_exit_code",
        lambda result, *, kanban_worker, **kw:
        {"result": result, "kanban_worker": kanban_worker, **kw})
    TURN_EXIT_MOD.credential_failure_flags = rec(
        ctx, "credential_failure_flags", lambda exc: {"exc": proj_exc(exc)})
    GOALS_MOD.run_kanban_goal_loop = rec(
        ctx, "run_kanban_goal_loop",
        lambda task_id, goal_text, run_turn, task_status_fn, block_fn, max_turns,
        first_response:
        {"task_id": task_id, "goal_text": goal_text, "run_turn": run_turn.__name__,
         "task_status_fn": task_status_fn.__name__, "block_fn": block_fn.__name__,
         "max_turns": max_turns, "first_response": first_response})
    PROC_ID_MOD.worker_birth = rec(ctx, "worker_birth",
                                   lambda process: {"process": "process"})

    # gateway family
    SESS_POLICY_MOD.build_policy = rec(
        ctx, "build_policy",
        lambda params, config, private_secrets=None:
        {"params": params, "config": config, "private_secrets": private_secrets},
        build=lambda raw: policy_build(raw, ctx))
    SESS_POLICY_MOD.restore_policy = rec(
        ctx, "restore_policy", lambda data: {"data": data},
        build=lambda raw: policy_build(raw, ctx))
    SESS_POLICY_MOD.bind_turn_workspace = rec(
        ctx, "bind_turn_workspace",
        lambda policy, session_id: {"policy": policy, "session_id": session_id})
    GW_RUN_MOD._resolve_gateway_model = rec(ctx, "resolve_gateway_model",
                                            lambda config: {"config": config})
    GW_RUN_MOD._load_gateway_config = rec(ctx, "load_gateway_config", lambda: {})
    SESS_LOCAL_MOD.create_local_session = rec(
        ctx, "create_local_session",
        lambda authority, actor, params, trusted_policy=None, trusted_secrets=None:
        {"authority": "authority", "actor": vars(actor), "params": params,
         "trusted_policy": trusted_policy, "trusted_secrets": trusted_secrets},
        build=ns_double)
    SESS_RECOVERY_MOD.restore_local_session = rec(
        ctx, "restore_local_session",
        lambda authority, sid: {"authority": "authority", "sid": sid},
        build=ns_double)
    SESS_PERSIST_MOD.authority_persistence = rec(
        ctx, "authority_persistence", lambda authority: {"authority": "authority"},
        build=lambda raw: store_build(raw, ctx))

    async def _tracked_write(authority, write, then=None, *, ordered=False, after=None):
        ctx.call("tracked_write", {"authority": "authority", "fn": write.__name__})
        return write()
    SESS_WORKERS_MOD.tracked_write = _tracked_write

    SESS_MANAGED_MOD.worker_turn_scope = rec(
        ctx, "worker_turn_scope", lambda frame: {"frame": frame},
        build=lambda raw: _SilentCM())
    SESS_SURFACE_MOD.arm_surface_turn = rec(ctx, "arm_surface_turn",
                                            lambda agent: {"agent": "agent"})
    SESS_AUTH_MOD.served_profile_name = rec(ctx, "served_profile_name",
                                            lambda home: {"home": home})
    SESS_CTX_MOD.set_session_vars = rec(
        ctx, "set_session_vars",
        lambda session_key, session_id, profile:
        {"session_key": session_key, "session_id": session_id, "profile": profile})
    SESS_CTX_MOD.clear_session_vars = rec(ctx, "clear_session_vars",
                                          lambda tokens: {"tokens": tokens})
    SESS_DISPLAY_MOD.worker_display_kwargs = rec(ctx, "worker_display_kwargs", lambda: {})
    BINDINGS_MOD.local_session_id = rec(
        ctx, "local_session_id",
        lambda profile_id, subject, request_id:
        {"profile_id": profile_id, "subject": subject, "request_id": request_id})
    PASM_MOD.store_location = rec(ctx, "store_location", lambda scope: {"scope": "scope"},
                                  build=lambda raw: Path(raw) if isinstance(raw, str) else raw)
    SKEYS_MOD.profile_from_session_key = rec(ctx, "profile_from_session_key",
                                             lambda route: {"route": route})
    KTR_MOD.worker_claim_is_live = rec(
        ctx, "worker_claim_is_live",
        lambda *, task_id=None, db_path=None, run_id=None, claim_lock=None,
        verify_pid=True:
        {"task_id": task_id, "db_path": db_path, "run_id": run_id,
         "claim_lock": claim_lock, "verify_pid": verify_pid})
    KTR_MOD.recover_failed_kanban_turns = rec(
        ctx, "recover_failed_kanban_turns",
        lambda turn_fn, get_result, *, task_id=None, claim_check=None, **kw:
        {"turn_fn": turn_fn.__name__, "get_result": get_result.__name__,
         "task_id": task_id, "claim_check": claim_check.__name__ if claim_check else None})
    AIS_MOD.agent_interface = rec(
        ctx, "agent_interface", lambda agent: {"agent": "agent"},
        build=lambda raw: types.SimpleNamespace(
            messages=types.SimpleNamespace(
                read_conversation=rec(ctx, "store.read_conversation",
                                      lambda session_id: {"session_id": session_id},
                                      build=lambda r: msg_list_build(r, ctx))))
        if isinstance(raw, dict) else raw)
    SKILL_MOD.build_preloaded_skills_prompt = rec(
        ctx, "build_preloaded_skills_prompt",
        lambda skills, task_id=None: {"skills": skills, "task_id": task_id})
    PSUTIL_MOD.Process = rec(ctx, "psutil.Process", lambda: {})
    LOGGING_MOD.getLogger = lambda name: _Log(ctx)

    # module-global doubles (module-level imports of session_kanban)
    sqlite3_double = types.SimpleNamespace(
        connect=rec(ctx, "sqlite3.connect",
                    lambda path, uri=False: {"path": path, "uri": uri},
                    build=lambda raw: _Conn(ctx)),
        Error=SqliteError, Row=object())
    NS["sqlite3"] = sqlite3_double
    NS["threading"] = types.SimpleNamespace(
        Thread=rec(ctx, "threading.Thread",
                   lambda target, name, daemon:
                   {"target": target.__name__, "name": name, "daemon": daemon},
                   build=lambda raw: _Thread(ctx)))
    NS["asyncio"] = types.SimpleNamespace(
        to_thread=rec_async(ctx, "asyncio.to_thread", _to_thread_args, build=ns_double))

    td_box = {"_TURNS_DONE": bool(state_in.get("_TURNS_DONE", False))}

    def _td_set():
        ctx.call("turns_done.set", {})
        td_box["_TURNS_DONE"] = True

    NS["_TURNS_DONE"] = types.SimpleNamespace(
        wait=rec(ctx, "turns_done.wait", lambda timeout: {"timeout": timeout}),
        set=_td_set)

    env_dict = dict(state_in.get("environ") or {})

    def _os_exit(code):
        ctx.call("os._exit", {"code": code})
        raise SystemExit(code)

    os_double = types.SimpleNamespace(
        getpid=rec(ctx, "os.getpid", lambda: {}),
        environ=env_dict,
        chdir=rec(ctx, "os.chdir", lambda path: {"path": path}),
        _exit=_os_exit)
    return os_double, td_box, sqlite3_double


def _to_thread_args(fn, *a, **k):
    out = {"fn": fn.__name__}
    if a:
        out["args"] = list(a)
    if k:
        out["kwargs"] = k
    return out


def connection_double(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    actor = types.SimpleNamespace(**(raw.get("actor") or {}))
    authority = types.SimpleNamespace(**(raw.get("authority") or {}))

    async def _submit(actor_, submission, _kanban_card=None):
        raw = ctx.call("authority.submit", {
            "actor": vars(actor_),
            "submission": {"request_id": submission.request_id,
                           "ref": submission.ref.session_id,
                           "payload": submission.payload,
                           "intent": submission.intent},
            "kanban_card": _kanban_card})
        return receipt_build(raw)
    authority.submit = _submit

    async def _resume(ref, params):
        return ctx.call("connection.resume",
                        {"ref": ref.session_id, "params": params})
    conn = types.SimpleNamespace(**{k: v for k, v in raw.items()
                                    if k not in ("actor", "authority")})
    conn.actor = actor
    conn.authority = authority
    conn.resume = _resume
    return conn


def agent_double(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    sdb = types.SimpleNamespace(
        get_messages_as_conversation=rec(
            ctx, "agent.get_messages_as_conversation",
            lambda session_id: {"session_id": session_id}))
    agent = types.SimpleNamespace(**{k: v for k, v in raw.items() if k != "_session_db"})
    agent._session_db = sdb
    agent.run_conversation = rec(
        ctx, "agent.run_conversation",
        lambda text, conversation_history=None, **kw:
        {"text": text, "conversation_history": conversation_history, **kw})
    return agent


def make_closure(name, freevars):
    """Exec the nested def inside a synthetic enclosing scope."""
    body = CLOSURE_SRC[name]
    if name in ("recover", "turn"):
        wrapper = (f"def _mk({', '.join(freevars)}):\n"
                   + textwrap.indent(body, "    ")
                   + "\n    def _get():\n        return result\n"
                   + f"    return {name}, _get\n")
    else:
        wrapper = (f"def _mk({', '.join(freevars)}):\n"
                   + textwrap.indent(body, "    ")
                   + f"\n    return {name}\n")
    g = dict(NS)
    exec(compile(wrapper, f"<closure {name}>", "exec", flags=_FLAGS, dont_inherit=True), g)
    return g["_mk"]


def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = copy.deepcopy(inp.get("state") or {})
    saved_os = sys.modules.get("os")
    saved_logging = sys.modules.get("logging")
    os_double, td_box, sqlite3_double = install_stubs(ctx, state_in)
    sys.modules["os"] = os_double
    sys.modules["logging"] = LOGGING_MOD
    # the target runs REAL even when it is also a seamed sibling
    if func in REAL:
        NS[func] = REAL[func]
    ret = None
    closure_result = _MISSING
    try:
        if func == "build_kanban_policy":
            ret = NS[func](connection_double(args.get("connection"), ctx),
                           args.get("params"), args.get("config"))
        elif func == "run_task":
            ret = _asyncio_real.run(NS[func](
                connection_double(args.get("connection"), ctx), args.get("params")))
        elif func == "admit_claim":
            mk = make_closure("admit_claim",
                              ["context", "params", "ref", "owner_db", "request_id",
                               "connect_closing", "kb", "os", "psutil", "worker_birth"])
            fn = mk(args.get("context"), args.get("params"),
                    ns_double(args.get("ref")), args.get("owner_db"),
                    args.get("request_id"), KBC_MOD.connect_closing, KB_MOD,
                    os_double, PSUTIL_MOD, PROC_ID_MOD.worker_birth)
            ret = fn()
        elif func == "bind_worker_context":
            ret = NS[func](args.get("frame"))
        elif func == "_watch_submitter":
            ret = NS[func](Path(args["db"]), args.get("run_id"))
        elif func == "watch":
            mk = make_closure("watch", ["db", "run_id", "_TURNS_DONE", "closing",
                                        "sqlite3", "os", "_worker_alive"])
            fn = mk(Path(args["db"]), args.get("run_id"), NS["_TURNS_DONE"],
                    contextlib.closing, sqlite3_double, os_double,
                    KBD_MOD._worker_alive)
            ret = fn()
        elif func == "run_worker_turns":
            ret = NS[func](agent_double(args.get("agent"), ctx),
                           args.get("frame"), args.get("history"))
        elif func == "_worker_workspace":
            cm = NS[func](args.get("frame"))
            ret = cm.__enter__()
            cm.__exit__(None, None, None)
        elif func == "_run_worker_turns":
            ret = NS[func](agent_double(args.get("agent"), ctx),
                           args.get("frame"), args.get("history"))
        elif func == "record_start_failure":
            ret = NS[func](args.get("frame"), make_exc(args["exc"]))
        elif func == "_record_worker_result":
            ret = NS[func](args.get("context"), args.get("code"),
                           args.get("last_output"))
        elif func == "record_worker_loss":
            ret = NS[func](ns_double(args.get("policy")), args.get("reason"),
                           pid=args.get("pid"), bound=args.get("bound", False))
        elif func == "_bound_worker_matches":
            ret = NS[func](_Conn(ctx), args.get("context"))
        elif func == "_bound_claim_is_live":
            ret = NS[func](args.get("context"))
        elif func in ("worker_result", "worker_exit_code"):
            ret = NS[func](args.get("path"), args.get("params"))
        elif func == "_stored_conversation":
            ret = NS[func](agent_double(args.get("agent"), ctx))
        elif func == "_run_task_turns":
            ret = NS[func](agent_double(args.get("agent"), ctx),
                           args.get("frame"), args.get("history"), args.get("context"))
        elif func == "recover":
            mk = make_closure("recover", ["agent", "result"])
            fn, get = mk(agent_double(args.get("agent"), ctx), args.get("result"))
            ret = fn(args.get("nudge"))
            closure_result = get()
        elif func == "status":
            mk = make_closure("status", ["connect_closing", "kb", "context"])
            fn = mk(KBC_MOD.connect_closing, KB_MOD, args.get("context"))
            ret = fn()
        elif func == "block":
            mk = make_closure("block", ["connect_closing", "kb", "context"])
            fn = mk(KBC_MOD.connect_closing, KB_MOD, args.get("context"))
            ret = fn(args.get("reason"))
        elif func == "turn":
            mk = make_closure("turn", ["agent", "result"])
            fn, get = mk(agent_double(args.get("agent"), ctx), args.get("result"))
            ret = fn(args.get("prompt"))
            closure_result = get()
        elif func == "kanban_run":
            ret = _asyncio_real.run(NS[func](
                connection_double(args.get("connection"), ctx),
                args.get("ref"), args.get("params")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    finally:
        if saved_os is not None:
            sys.modules["os"] = saved_os
        else:
            del sys.modules["os"]
        if saved_logging is not None:
            sys.modules["logging"] = saved_logging
        _CUR_CTX = None
    state_out = dict(state_in)
    if func == "bind_worker_context":
        state_out["environ"] = os_double.environ
    if func == "_record_worker_result":
        state_out["_TURNS_DONE"] = td_box["_TURNS_DONE"]
    if closure_result is not _MISSING:
        state_out["result"] = closure_result
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "build_kanban_policy", "run_task", "watch_submitter", "run_worker_turns_inner",
    "worker_workspace", "record_worker_result", "bound_worker_matches",
    "stored_conversation", "run_task_turns", "worker_result",
    "kb._require_slug", "kb.kanban_db_path", "kb.get_task", "kb.build_worker_context",
    "kb._retry_status_for_run", "kb.workspaces_root", "kb.write_txn",
    "kb._append_event", "kb.redact_review_value", "kb.goal_run_status", "kb.block_task",
    "kbc.connect_closing", "sqlite3.connect", "conn.execute", "conn.close",
    "path.resolve", "path.as_uri", "resolve_profile_env", "build_policy",
    "policy.config", "resolve_gateway_model", "load_gateway_config", "restore_policy",
    "authority_persistence", "store_location", "store.bindings.receipt",
    "asyncio.to_thread", "local_session_id", "create_local_session",
    "restore_local_session", "tracked_write", "connection.resume", "authority.submit",
    "psutil.Process", "worker_birth", "os.getpid", "os.chdir", "os._exit",
    "kbd._worker_terminal_timeout_env", "kbd._process_fingerprint", "kbd._worker_alive",
    "threading.Thread", "thread.start", "turns_done.wait", "turns_done.set",
    "worker_turn_scope", "arm_surface_turn", "worker_display_kwargs",
    "agent.run_conversation", "turn_exit_code", "credential_failure_flags",
    "log.exception", "worker_claim_is_live", "agent_interface",
    "agent.get_messages_as_conversation", "store.read_conversation",
    "message.as_openai_message", "build_preloaded_skills_prompt",
    "recover_failed_kanban_turns", "run_kanban_goal_loop", "served_profile_name",
    "profile_from_session_key", "set_session_vars", "bind_turn_workspace",
    "clear_session_vars",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_kanban_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_kanban_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmk_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_kanban_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec(?:_async)?\(\s*\n?\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"tracked_write"}   # the bespoke async stub
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_kanban_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_kanban_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_kanban_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
    # fixture func set vs the extraction map
    if fix.exists():
        funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                funcs.add(json.loads(line)["func"])
        known = set(PORTED) | set(CLOSURES) | {"kanban_run"}
        bad = funcs - known
        if bad:
            print(f"FATAL oracle_tui_methods_kanban_flow: fixture funcs not extracted: {sorted(bad)}")
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
    print(f"ORACLE tui_methods_kanban_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_kanban_flow.jsonl"
    n = bad = 0
    counts = {}
    known_names = set(PORTED) | set(CLOSURES) | {"kanban_run"} | set(SEAM_NAMES)
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
        # uniform AttributeError family guard: a raise naming a ported method
        # or seam stub reads as a binding bug, not a live decision
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in known_names:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("worker_exit_code", {"args": {"path": "/b.db", "params": {}},
                                      "io": {"worker_result": {"exit_code": 7}}})
    assert spot["ret"] == 7, spot
    spot = drive("worker_exit_code", {"args": {"path": "/b.db", "params": {}},
                                      "io": {"worker_result": {}}})
    assert spot["ret"] == 1, spot
    spot = drive("_bound_worker_matches",
                 {"args": {"context": {"task_id": "t1", "run_id": 5, "claim_lock": "L"},
                           "conn": "conn"},
                  "io": {"conn.execute": [['{"pid": 9, "claim_lock": "L"}']],
                         "os.getpid": 9}})
    assert spot["ret"] is True, spot
    spot = drive("record_worker_loss",
                 {"args": {"policy": {"kanban_json": "null"}, "reason": "x"}})
    assert spot["ret"] is None and spot["trace"] == [], spot
    spot = drive("kanban_run",
                 {"args": {"connection": {}, "params": {"a": 1}},
                  "io": {"run_task": {"session_id": "s1"}}})
    assert spot["ret"] == {"session_id": "s1"}, spot
    assert spot["trace"][0]["name"] == "run_task", spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_kanban_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()