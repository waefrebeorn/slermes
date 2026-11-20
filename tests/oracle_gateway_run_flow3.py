#!/usr/bin/env python3
"""Oracle for the gateway/run.py FLOW cores + leaves, third wave
(src/gateway/gateway_run_flow3.c).

AST-extracts the LIVE unclaimed defs (9 top-level + 6 GatewayRunner methods,
no heavy gateway import), seeds the module constants (_TELEGRAM_COMMAND_
MENTION_RE, _BUILTIN_ADAPTERS, _HOUSEKEEPING_MAX_WORKERS, _AGENT_PENDING_
SENTINEL, Platform) and replaces every live I/O dependency with recording
stubs driven by the SAME per-name scripts the C harness feeds its universal
seams (in.io). Compares the ordered seam-call trace plus the return value.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused;
a scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds (SystemExit /
KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec.

Modeling (twin of the port header): callable markers "cls:X"/"fn:X" fire
seam X when called (cls: carries {config}, fn: carries {}) and denorm back
to the FULL marker string; threading
events/locks, futures, stores, executors, loops and agents travel as marker
strings with attribute doubles; id(a) projects to the agent marker; the
pending sentinel is {"__sentinel__": true}; float("inf") projects to "inf";
on_summary projects to "on_summary" and fires logger.info per in.summary_msgs
entry after run_idle_jobs returns; _get_or_create_pool returning null means
"pool absent -> make_pool()" (fires ThreadPoolExecutor); await is identity on
the run_in_executor seam result; CM flows (_profile_runtime_scope,
_session_env_scope, live_stores) fire enter/exit seams with Python finally
semantics (exit raise replaces, enter raise skips body+exit); the restart
handler drive invokes the returned closure; Platform enums travel as value
strings validated before dispatch. float()/int() coercions accept int/float/
bool (int truncates toward zero), null raises the exact TypeError, str values
are outside the fixture domain. Paths are PurePosixPath (POSIX on Windows).

Input: stdin JSON lines from t_port_gateway_run_flow3.
Summary: ORACLE gateway_run_flow3: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import contextlib
import copy
import importlib.util as _importlib_util_real
import json
import os
import sys
import textwrap
import types
import __future__
from enum import Enum
from pathlib import Path, PurePosixPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": [], "idx": {}}

# Full builtin exception hierarchy (real classes, so except-site identity and
# BaseException propagation are faithful); unknown kinds -> synthetic subclass.
_BUILTIN_EXC = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    cls = _BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return raise_spec(a)
        return denorm(a)

    def _rec(self, name, fmt, a, k):
        args = {"fmt": fmt}
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return maybe_raise(seam(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", fmt, a, k)


# ── attribute doubles (denorm projects back to markers/dicts) ──

class MarkerDouble:
    def __init__(self, marker):
        self._marker = marker


class CallableDouble(MarkerDouble):
    """Marker keeps the FULL "cls:X"/"fn:X" string (denorm projects it back
    unchanged, matching the C raw-value passthrough); the seam name is the
    suffix."""
    def __init__(self, marker, shape):
        super().__init__(marker)
        self._shape = shape

    def __call__(self, *a, **k):
        name = self._marker.split(":", 1)[1]
        if self._shape == "config":
            args = {"config": denorm(a[0]) if a else None}
        else:
            args = {}
        return seam(name, args)


class EventDouble(MarkerDouble):
    def wait(self, timeout):
        return seam("worker_done.wait", {"timeout": timeout})


class LockDouble(MarkerDouble):
    pass


class FutureDouble(MarkerDouble):
    def result(self, timeout=None):
        return seam("fut.result", {"timeout": timeout})


class StoreDouble(MarkerDouble):
    def close(self):
        return seam("store.close", {})


class LoopDouble(MarkerDouble):
    def run_in_executor(self, executor, fn, func, *args):
        r = seam("loop.run_in_executor",
                 {"executor": denorm(executor), "fn": denorm(fn),
                  "func": denorm(func), "args": [denorm(a) for a in args]})
        return DoneAwaitable(r)


class DoneAwaitable:
    def __init__(self, v):
        self.v = v

    def __await__(self):
        if False:
            yield
        return self.v


class AgentMarker(MarkerDouble):
    pass


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class WatchAgent(AttrDouble):
    def __init__(self, d):
        super().__init__(d)
        if "get_activity_summary" in d:
            object.__setattr__(self, "get_activity_summary",
                               lambda: seam("get_activity_summary", {}))


class FtsStore:
    def __init__(self, d):
        self._marker = d.get("marker", "")
        self.scope = d.get("scope")
        self.search = types.SimpleNamespace(
            retry_deferred_search_recovery=lambda: seam(
                "retry_deferred_search_recovery", {"store": self._marker}))


class GatewayRunnerDouble:
    """Instance-method presence gate: fixture dict keys bind recorders;
    missing names raise the live-shaped AttributeError. Ported methods are
    bound over it in build_runner; _get_or_create_pool is class-level."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return runner_method_rec(name)
        raise AttributeError("'GatewayRunner' object has no attribute '%s'" % name)


def runner_method_rec(name):
    def rec(*a, **k):
        if name == "_running_agent_items":
            r = seam(name, {})
            return [(p[0], mat_item_agent(p[1])) for p in r]
        if name == "request_restart":
            return seam(name, dict(k))
        if name in ("_authorization_home_for_source", "_admit_bot_message"):
            return seam(name, {"source": denorm(a[0])})
        if name == "_set_session_env":
            return seam(name, {"context": denorm(a[0])})
        if name == "_clear_session_env":
            return seam(name, {"tokens": denorm(a[0])})
        return seam(name, {"args": [denorm(x) for x in a],
                           "kwargs": {kk: denorm(vv) for kk, vv in k.items()}})
    return rec


SENTINEL = object()


def mat_item_agent(a):
    if a is None:
        return None
    if isinstance(a, dict) and "__sentinel__" in a:
        return SENTINEL
    if isinstance(a, str):
        return AgentMarker(a)
    return a


def mat_callable(v):
    if isinstance(v, str) and v.startswith("cls:"):
        return CallableDouble(v, "config")
    if isinstance(v, str) and v.startswith("fn:"):
        return CallableDouble(v, "none")
    return v


def denorm(x):
    if isinstance(x, MarkerDouble):
        return x._marker
    if isinstance(x, (AttrDouble, GatewayRunnerDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, FtsStore):
        return {"marker": x._marker, "scope": denorm(x.scope)}
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, PurePath):
        return x.as_posix()
    if isinstance(x, Enum):
        return x.value
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(denorm(v) for v in x)
    return x


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


def run_coro(coro):
    try:
        coro.send(None)
    except StopIteration as si:
        return si.value
    raise RuntimeError("coroutine yielded unexpectedly")


def run_flow(fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": None}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": None}


# ── live-def extraction ──

WANTED_TOPS = (
    "_telegramize_command_mentions", "_watch_gateway_turn_inactivity",
    "_builtin_adapter_import", "_instantiate_builtin_adapter",
    "_housekeeping_channel_directory", "_housekeeping_curator",
    "_housekeeping_state_db_maintenance", "_housekeeping_deferred_fts_retry",
    "_start_gateway_make_restart_signal_handler",
)
WANTED_METHODS = (
    "_running_agent_ids", "_admit_bot_message_for_source",
    "_under_authorization_profile", "_session_env_scope",
    "_run_housekeeping_in_executor", "_get_housekeeping_executor",
)


def extract_platform():
    path = os.path.join(QUARRY, "gateway/config.py")
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "Platform":
            ns = {"Enum": Enum}
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            return ns["Platform"]
    raise KeyError("Platform")


Platform = extract_platform()


def extract_run(pre):
    path = os.path.join(QUARRY, "gateway/run.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "gr3_extracted_gateway_run_py", "__file__": path}
    ns.update(pre)
    tree = ast.parse(src)

    def seed_assigns():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (isinstance(node.target, ast.Name) and node.value is not None):
                    continue
            else:
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            except Exception:
                pass

    seed_assigns()
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_TOPS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    # class methods: dedent the source segment, strip decorators, compile
    lines = src.splitlines()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayRunner":
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in WANTED_METHODS:
                    seg = textwrap.dedent("\n".join(lines[m.lineno - 1:m.end_lineno]))
                    mnode = ast.parse(seg).body[0]
                    mnode.decorator_list = []
                    exec(compile(ast.Module(body=[mnode], type_ignores=[]), path, "exec", flags=flags), ns)
    seed_assigns()
    return ns


# ── recording stubs for module-level collaborators ──

def abandon_stub(**kw):
    return seam("_abandon_timed_out_gateway_turn", {k: denorm(v) for k, v in kw.items()})


def sched_stub(coro, loop, logger=None, log_message=None):
    r = seam("safe_schedule_threadsafe",
             {"coro": denorm(coro), "loop": denorm(loop),
              "logger": "logger", "log_message": log_message})
    if r is None:
        return None
    return FutureDouble(r)


@contextlib.contextmanager
def profile_runtime_scope_stub(profile_home, prepared_secret_scope=None, *,
                               hydrate_secrets=True, prepared_terminal_scope=None):
    seam("_profile_runtime_scope.enter", {"home": denorm(profile_home)})
    try:
        yield
    finally:
        seam("_profile_runtime_scope.exit", {})


def profile_sessions_dir_stub(launch):
    return seam("_profile_sessions_dir", {"launch": denorm(launch)})


class AwakeIdleMeterDouble:
    def measure(self, seconds):
        return seam("awake_idle.measure", {"seconds": seconds})


def gocp_stub(self, attr, make_pool):
    r = seam("_get_or_create_pool",
             {"runner": denorm(self), "attr": attr, "make_pool": "ThreadPoolExecutor"})
    if r is None:
        return make_pool()
    return r


GatewayRunnerDouble._get_or_create_pool = staticmethod(gocp_stub)


def tpe_ctor(**kw):
    return seam("ThreadPoolExecutor", dict(kw))


def copy_context_stub():
    seam("copy_context", {})
    return types.SimpleNamespace(run=CallableDouble("copy_context.run", "none"))


def get_running_loop_stub():
    r = seam("asyncio.get_running_loop", {})
    if r is None:
        return None
    return LoopDouble(r)


NS = extract_run(pre={
    "re": __import__("re"),
    "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger(),
                                     DEBUG=10, INFO=20, WARNING=30, ERROR=40),
    "logger": RecLogger(),
    "json": json,
    "Path": PurePosixPath,
    "Platform": Platform,
    "time": types.SimpleNamespace(monotonic=lambda: seam("time.monotonic", {})),
    "AwakeIdleMeter": AwakeIdleMeterDouble,
    "_abandon_timed_out_gateway_turn": abandon_stub,
    "safe_schedule_threadsafe": sched_stub,
    "_profile_runtime_scope": profile_runtime_scope_stub,
    "_profile_sessions_dir": profile_sessions_dir_stub,
    "asyncio": types.SimpleNamespace(get_running_loop=get_running_loop_stub,
                                     CancelledError=asyncio.CancelledError),
    "copy_context": copy_context_stub,
    "concurrent": types.SimpleNamespace(
        futures=types.SimpleNamespace(ThreadPoolExecutor=tpe_ctor)),
    "GatewayRunner": GatewayRunnerDouble,
    "id": lambda a: a._marker,
    "_AGENT_PENDING_SENTINEL": SENTINEL,
})
# module-level Assign seeding re-execs `logger = logging.getLogger(__name__)`
# and `_AGENT_PENDING_SENTINEL = object()`; restore the doubles AFTER.
NS["logger"] = RecLogger()
NS["Path"] = PurePosixPath
NS["Platform"] = Platform
NS["GatewayRunner"] = GatewayRunnerDouble
NS["id"] = lambda a: a._marker
NS["_AGENT_PENDING_SENTINEL"] = SENTINEL
NS["_abandon_timed_out_gateway_turn"] = abandon_stub
NS["safe_schedule_threadsafe"] = sched_stub
NS["_profile_runtime_scope"] = profile_runtime_scope_stub
NS["_profile_sessions_dir"] = profile_sessions_dir_stub
NS["time"] = types.SimpleNamespace(monotonic=lambda: seam("time.monotonic", {}))
NS["AwakeIdleMeter"] = AwakeIdleMeterDouble
NS["asyncio"] = types.SimpleNamespace(get_running_loop=get_running_loop_stub,
                                      CancelledError=asyncio.CancelledError)
NS["copy_context"] = copy_context_stub
NS["concurrent"] = types.SimpleNamespace(
    futures=types.SimpleNamespace(ThreadPoolExecutor=tpe_ctor))


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    if "." in name:
        parent, child = name.rsplit(".", 1)
        if parent in sys.modules:
            setattr(sys.modules[parent], child, m)
    return m


# Stub packages FIRST (the quarry venv's editable install would otherwise
# resolve the real modules); every intermediate package is registered.
mod("gateway")
mod("hermes_cli")
mod("agent")

mod("hermes_cli.commands_platforms",
    _sanitize_telegram_name=lambda name: seam("sanitize_telegram_name", {"name": name}))


def import_module_stub(full):
    r = seam("importlib.import_module", {"module": full})
    if not isinstance(r, dict):
        return r
    m = types.ModuleType(full)
    for k, v in r.items():
        setattr(m, k, mat_callable(v))
    return m


mod("importlib", import_module=import_module_stub)
# keep the REAL importlib.util reachable: the stub parent must not break
# later `import importlib.util` in the same process (self-tests, tooling).
sys.modules["importlib.util"] = _importlib_util_real
sys.modules["importlib"].util = _importlib_util_real
mod("gateway.channel_directory",
    build_channel_directory=lambda adapters: seam("build_channel_directory",
                                                  {"adapters": denorm(adapters)}))


def run_idle_jobs_stub(*, idle_for_seconds, on_summary, fence=None):
    r = seam("run_idle_jobs", {"idle_for_seconds": "inf",
                               "on_summary": "on_summary", "fence": denorm(fence)})
    for msg in CUR["in"].get("summary_msgs") or []:
        on_summary(msg)
    return r


mod("hermes_cli.plugins_idle_tasks", run_idle_jobs=run_idle_jobs_stub)
mod("hermes_cli.config", load_config=lambda: seam("load_config", {}))
mod("gateway.session_ingress_media",
    native_inputs_root=lambda: mat_path(seam("native_inputs_root", {})))


def mat_path(r):
    if isinstance(r, str):
        return PurePosixPath(r)
    return r


def mat_scope(r):
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def mat_store(r):
    if r is None:
        return None
    return StoreDouble(r)


@contextlib.contextmanager
def live_stores_stub():
    r = seam("live_stores", {})
    yield [FtsStore(d) for d in r]


mod("persistence_assembly",
    profile_scope=lambda: mat_scope(seam("profile_scope", {})),
    open_persistence=lambda scope, eager=True: mat_store(
        seam("open_persistence", {"scope": denorm(scope), "eager": eager})),
    open_store_admin=lambda scope, read_only=False: seam(
        "open_store_admin", {"scope": denorm(scope), "read_only": read_only}),
    live_stores=live_stores_stub,
    store_location=lambda scope: seam("store_location", {"scope": denorm(scope)}))
mod("session_retention",
    run_auto_archive=lambda store, idle_days, min_interval_hours: seam(
        "run_auto_archive", {"store": denorm(store), "idle_days": idle_days,
                             "min_interval_hours": min_interval_hours}),
    run_auto_prune_and_vacuum=lambda store, admin, **kw: seam(
        "run_auto_prune_and_vacuum",
        dict({"store": denorm(store), "admin": denorm(admin)},
             **{k: denorm(v) for k, v in kw.items()})))


def install_case():
    CUR["idx"] = {}
    NS["logger"] = RecLogger()
    if CUR["in"].get("import_fail"):
        sys.modules["hermes_cli.commands_platforms"] = None
    else:
        mod("hermes_cli.commands_platforms",
            _sanitize_telegram_name=lambda name: seam("sanitize_telegram_name",
                                                      {"name": name}))


FUNC_NAMES = {
    "_telegramize_command_mentions": "telegramize_command_mentions",
    "_watch_gateway_turn_inactivity": "watch_gateway_turn_inactivity",
    "_builtin_adapter_import": "builtin_adapter_import",
    "_instantiate_builtin_adapter": "instantiate_builtin_adapter",
    "_housekeeping_channel_directory": "housekeeping_channel_directory",
    "_housekeeping_curator": "housekeeping_curator",
    "_housekeeping_state_db_maintenance": "housekeeping_state_db_maintenance",
    "_housekeeping_deferred_fts_retry": "housekeeping_deferred_fts_retry",
    "_start_gateway_make_restart_signal_handler": "start_gateway_make_restart_signal_handler",
    "_running_agent_ids": "running_agent_ids",
    "_admit_bot_message_for_source": "admit_bot_message_for_source",
    "_under_authorization_profile": "under_authorization_profile",
    "_session_env_scope": "session_env_scope",
    "_run_housekeeping_in_executor": "run_housekeeping_in_executor",
    "_get_housekeeping_executor": "get_housekeeping_executor",
}
FIXTURE_TO_LIVE = {v: k for k, v in FUNC_NAMES.items()}


def build_runner(inp):
    r = inp.get("runner")
    if r is None:
        return None
    rd = GatewayRunnerDouble(r)
    for nm in WANTED_METHODS:
        object.__setattr__(rd, nm, types.MethodType(NS[nm], rd))
    return rd


def mat_source(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**v)
    return v


def drive(func, inp):
    live = FIXTURE_TO_LIVE.get(func)
    if live is None:
        raise ValueError("unknown func: %s" % func)
    fn = NS[live]

    if func == "telegramize_command_mentions":
        plat = inp.get("platform")
        if isinstance(plat, dict):
            plat = AttrDouble(plat)
        return run_flow(lambda: fn(inp.get("text", ""), plat))
    if func == "watch_gateway_turn_inactivity":
        holder = inp.get("agent_holder")
        if isinstance(holder, list):
            holder = [WatchAgent(a) if isinstance(a, dict) else a for a in holder]
        isc = inp.get("is_still_current")
        if isinstance(isc, str):
            isc = CallableDouble(isc, "none")
        return run_flow(lambda: fn(
            agent_holder=holder, task_id=inp.get("task_id", ""),
            process_baseline=inp.get("process_baseline"),
            timeout=inp.get("timeout", 0),
            worker_done=EventDouble(inp.get("worker_done", "ev")),
            timeout_fired=EventDouble(inp.get("timeout_fired", "tf")),
            cleanup_lock=LockDouble(inp.get("cleanup_lock", "lk")),
            poll_interval=inp.get("poll_interval", 5.0),
            is_still_current=isc))
    if func == "builtin_adapter_import":
        return run_flow(lambda: fn(inp.get("module", ""), inp.get("adapter_name", ""),
                                   inp.get("requirement", "")))
    if func == "instantiate_builtin_adapter":
        # Platform has a dynamic _missing_ (plugin pseudo-members, filesystem
        # scan); the fixture domain is the STATIC enum values, validated
        # before dispatch exactly like the harness (ValueError otherwise).
        pv = inp.get("platform")
        def go():
            if pv not in {m.value for m in Platform}:
                raise ValueError("'%s' is not a valid Platform" % pv)
            return fn(Platform(pv), inp.get("config"))
        return run_flow(go)
    if func == "housekeeping_channel_directory":
        return run_flow(lambda: fn(inp.get("adapters"), inp.get("loop")))
    if func == "housekeeping_curator":
        return run_flow(lambda: fn(inp.get("idle_fence")))
    if func == "housekeeping_state_db_maintenance":
        launch = inp.get("launch")
        if isinstance(launch, list):
            launch = tuple(PurePosixPath(x) for x in launch)
        return run_flow(lambda: fn(launch))
    if func == "housekeeping_deferred_fts_retry":
        return run_flow(lambda: fn())
    if func == "start_gateway_make_restart_signal_handler":
        rd = build_runner(inp)
        def go():
            handler = fn(rd)
            return handler()
        return run_flow(go)
    if func == "running_agent_ids":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd))
    if func == "admit_bot_message_for_source":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd, mat_source(inp.get("source"))))
    if func == "under_authorization_profile":
        rd = build_runner(inp)
        cb = lambda: seam("check", {})
        return run_flow(lambda: fn(rd, mat_source(inp.get("source")), cb))
    if func == "session_env_scope":
        rd = build_runner(inp)
        ctx = mat_source(inp.get("context"))
        def go():
            cm = contextlib.contextmanager(NS["_session_env_scope"])(rd, ctx)
            with cm:
                seam("body", {})
            return None
        return run_flow(go)
    if func == "run_housekeeping_in_executor":
        rd = build_runner(inp)
        fmarker = inp.get("func")
        args = inp.get("args") or []
        return run_flow(lambda: run_coro(fn(rd, fmarker, *args)))
    if func == "get_housekeeping_executor":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd))
    raise ValueError("unknown func: %s" % func)


def main():
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        install_case()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gateway_run_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
