#!/usr/bin/env python3
"""Oracle for the tools/terminal_tool.py UNCLAIMED-remainder FLOW cores
(src/tools/terminal_tool_flow3.c, 12 functions).

AST-extracts the LIVE defs (no heavy tools.terminal_tool import), seeds the
module globals/classes, and replaces every live I/O dependency with recording
stubs driven by the SAME per-name scripts the C harness feeds its universal
seams (in.io). Compares the ordered seam-call trace + return + mutated state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last repeats, empty -> None); {"__raise__": kind, "msg": text}
raises a real exception of that kind; scripted null -> None; ABSENT key ->
FAIL-CLOSED RuntimeError "no script for <name>" (recorded as ret, then raised).
BaseException kinds pass through every except site; the full builtin hierarchy
is modeled on both sides. Input: stdin JSON lines from t_port_terminal_tool_flow3.

Summary: ORACLE terminal_tool_flow3: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import contextlib
import dataclasses
import json
import logging
import os
import sys
import threading
import types
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": args, "ret": spec})
        return spec
    sc = io[name]
    val = sc
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"]
        idx = SEQPOS.get(name, 0)
        if not seq:
            val = None
        else:
            if idx >= len(seq):
                idx = len(seq) - 1
            val = seq[idx]
            SEQPOS[name] = idx + 1
    rec({"ev": name, "args": args, "ret": val})
    return val


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    if kind == "_Rejected":
        return NS["_Rejected"](msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "PermissionError": PermissionError, "RuntimeError": RuntimeError,
        "TimeoutError": TimeoutError, "ImportError": ImportError,
        "ModuleNotFoundError": ModuleNotFoundError, "OverflowError": OverflowError,
        "IndexError": IndexError, "NameError": NameError, "StopIteration": StopIteration,
        "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
        "UnicodeDecodeError": UnicodeDecodeError, "BlockingIOError": BlockingIOError,
        "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
        "ProcessLookupError": ProcessLookupError, "FileExistsError": FileExistsError,
        "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
        "InterruptedError": InterruptedError, "ChildProcessError": ChildProcessError,
        "ConnectionResetError": ConnectionResetError, "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError, "EOFError": EOFError,
        "MemoryError": MemoryError, "BufferError": BufferError, "ReferenceError": ReferenceError,
        "AssertionError": AssertionError, "SyntaxError": SyntaxError, "IndentationError": IndentationError,
        "TabError": TabError, "SystemError": SystemError, "UnboundLocalError": UnboundLocalError,
        "ZeroDivisionError": ZeroDivisionError, "FloatingPointError": FloatingPointError,
        "ArithmeticError": ArithmeticError, "LookupError": LookupError, "Warning": Warning,
        "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
        "FutureWarning": FutureWarning, "RuntimeWarning": RuntimeWarning,
        "SyntaxWarning": SyntaxWarning, "ResourceWarning": ResourceWarning,
        "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
        "UnicodeTranslateError": UnicodeTranslateError, "Exception": Exception,
        "BaseException": BaseException, "ExceptionGroup": ExceptionGroup,
        "BaseExceptionGroup": BaseExceptionGroup,
    }
    cls = builtins.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


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
            return {"__raise__": type(a).__name__, "msg": str(a)}
        return a

    def _rec(self, name, level, fmt, a, k):
        args = {"fmt": fmt}
        if level is not None:
            args["level"] = level
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return materialize(script(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", None, fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", None, fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", None, fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", None, fmt, a, k)

    def log(self, level, fmt, *a, **k):
        return self._rec("logger.log", level, fmt, a, k)


logger = RecLogger()


@contextlib.contextmanager
def _quiet(label, *args, exc=Exception, level=logging.DEBUG):
    """Best-effort block from terminal_tool_config.py (swallow Exception, log DEBUG)."""
    try:
        yield
    except exc:
        logger.log(level, label, *args, exc_info=True)


class LocalEnvironmentDouble:
    """env materialized from {"__local_env__": {...}}: isinstance LocalEnvironment."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def _usable_cwd(self, workdir):
        return materialize(script("usable_cwd", {"workdir": workdir}))


def mat_env(v):
    if isinstance(v, dict) and v.get("__local_env__"):
        return LocalEnvironmentDouble(v)
    return v


def denorm(x):
    if isinstance(x, LocalEnvironmentDouble):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return {k: denorm(v) for k, v in dataclasses.asdict(x).items()}
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    return x


# ── live-def extraction ──
PORTED = (
    "_is_isolation_override_key", "_has_isolation_overrides",
    "_maybe_reap_docker_orphans", "register_container_alias",
    "_session_scope", "_docker_session_isolation_enabled",
    "_command_requires_pipe_stdin", "_resolve_notification_flag_conflict",
    "_resolve_command_cwd", "_refuse_missing_local_workdir",
    "_resolve_task_host_cwd",
)
CLASSES = ("_SessionScope", "_Rejected")


def extract():
    path = os.path.join(QUARRY, "tools", "terminal_tool.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "tt3_extracted", "__file__": path,
          "dataclass": dataclasses.dataclass, "logger": RecLogger()}
    sys.modules.setdefault("tt3_extracted", types.ModuleType("tt3_extracted"))
    flags = __future__.annotations.compiler_flag
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in PORTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = (set(PORTED) | set(CLASSES)) - set(ns)
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    return ns


NS = extract()
REAL_DSE = NS["_docker_session_isolation_enabled"]


def stubmod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def seamfn(name, argfn):
    def f(*a, **k):
        return materialize(script(name, argfn(*a, **k)))
    return f


# Docker module: the in-function `from ... import` fires ONE import_docker seam
# at the first attribute access (matching the C import gate's position, after
# the lifetime probe). A scripted ImportError there halts the import.
class _DockerModule(types.ModuleType):
    def __init__(self):
        super().__init__("tools.environments.docker")
        object.__setattr__(self, "_tt3_fired", False)
        object.__setattr__(self, "_tt3_impls", {
            "_container_identity": seamfn("container_identity", lambda x: {"config_key": x}),
            "reap_orphan_containers": seamfn(
                "reap_orphan_containers",
                lambda max_age_seconds, profile_filter: {
                    "max_age_seconds": max_age_seconds, "profile_filter": profile_filter}),
        })

    def __getattribute__(self, name):
        impls = object.__getattribute__(self, "_tt3_impls")
        if name in impls:
            if not object.__getattribute__(self, "_tt3_fired"):
                object.__setattr__(self, "_tt3_fired", True)
                materialize(script("import_docker", {}))
            return impls[name]
        return super().__getattribute__(name)


def make_docker_mod():
    sys.modules["tools.environments.docker"] = _DockerModule()


def install_globals():
    NS["logger"] = RecLogger()
    NS["logging"] = logging
    NS["contextlib"] = contextlib
    NS["threading"] = threading
    NS["_quiet"] = _quiet
    NS["os"] = types.SimpleNamespace(name="posix", environ={})
    for lk in ("_creation_locks_lock", "_container_alias_lock", "_docker_orphan_reaper_lock"):
        NS[lk] = threading.Lock()
    NS["_docker_orphan_reaper_ran"] = False

    # env / config leaves -> seams
    NS["_plugin_env_flag"] = seamfn(
        "plugin_env_flag",
        lambda env_type, attr, default=False: {"env_type": env_type, "attr": attr, "default": default})
    NS["_ensure_terminal_env_bridged"] = seamfn("ensure_terminal_env_bridged", lambda: {})
    NS["_tenv"] = seamfn("tenv", lambda name, default="": {"name": name, "default": default})
    NS["_tenv_bool"] = seamfn("tenv_bool", lambda name, default: {"name": name, "default": default})
    NS["_qualify_task_key"] = seamfn("qualify_task_key", lambda key: {"key": key})
    NS["_error_json"] = seamfn(
        "error_json", lambda error, *, exit_code=-1, status=None: {
            "error": error, "exit_code": exit_code, "status": status})
    NS["_container_visible_cwd"] = seamfn(
        "container_visible_cwd", lambda path, env_type, env: {
            "path": path, "env_type": env_type, "env": denorm(env)})
    NS["coerce_ssh_remote_cwd"] = seamfn(
        "coerce_ssh_remote_cwd", lambda cwd, env_type: {"cwd": cwd, "env_type": env_type})
    NS["get_session_cwd"] = seamfn("get_session_cwd", lambda session_key: {"session_key": session_key})
    NS["_is_container_backend"] = seamfn(
        "is_container_backend", lambda env_type: {"env_type": env_type})
    NS["_is_unusable_container_cwd"] = seamfn(
        "is_unusable_container_cwd", lambda cwd, *, mounted_host=None: {
            "cwd": cwd, "mounted_host": mounted_host})
    NS["_in_sandbox_home"] = seamfn(
        "in_sandbox_home", lambda cwd, env: {"cwd": cwd, "env": denorm(env)})
    NS["_is_mounted_host_cwd"] = seamfn(
        "is_mounted_host_cwd", lambda cwd, mounted_host: {"cwd": cwd, "mounted_host": mounted_host})
    NS["_container_visible_default"] = seamfn(
        "container_visible_default", lambda default_cwd, env_type, env: {
            "default_cwd": default_cwd, "env_type": env_type, "env": denorm(env)})

    # claimed siblings (for resolve_task_host_cwd) -> seams
    NS["_docker_session_isolation_enabled"] = seamfn("docker_session_isolation_enabled", lambda: {})
    NS["_resolve_container_task_id"] = seamfn(
        "resolve_container_task_id", lambda task_id: {"task_id": task_id})
    NS["_resolve_container_alias"] = seamfn(
        "resolve_container_alias", lambda task_id: {"task_id": task_id})
    NS["_container_profile"] = seamfn("container_profile", lambda: {})
    NS["profile_container_key"] = seamfn("profile_container_key", lambda profile: {"profile": profile})
    NS["_session_workspace_mount_source"] = seamfn(
        "session_workspace_mount_source", lambda owner: {"owner": owner})
    NS["workspace_key_for"] = seamfn("workspace_key_for", lambda cwd: {"cwd": cwd})

    # in-function imports -> sys.modules stubs
    pkg("tools")
    pkg("tools.environments")
    stubmod("tools.environments.local", LocalEnvironment=LocalEnvironmentDouble)
    make_docker_mod()


install_globals()


def build_state(inp):
    st = inp.get("state") or {}
    NS["_docker_orphan_reaper_ran"] = bool(st.get("docker_orphan_reaper_ran", False))
    NS["_container_aliases"] = dict(st.get("container_aliases") or {})
    NS["_task_env_overrides"] = {k: (dict(v) if isinstance(v, dict) else v)
                                 for k, v in (st.get("task_env_overrides") or {}).items()}


def echo_state():
    return {
        "docker_orphan_reaper_ran": bool(NS["_docker_orphan_reaper_ran"]),
        "container_aliases": dict(NS["_container_aliases"]),
        "task_env_overrides": {k: denorm(v) for k, v in NS["_task_env_overrides"].items()},
    }


def run_flow(fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": echo_state()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": echo_state()}


def drive(func, inp):
    build_state(inp)
    if func == "command_requires_pipe_stdin":
        return run_flow(lambda: NS["_command_requires_pipe_stdin"](inp.get("command", "")))
    if func == "resolve_notification_flag_conflict":
        return run_flow(lambda: NS["_resolve_notification_flag_conflict"](
            notify_on_complete=inp.get("notify_on_complete", False),
            watch_patterns=inp.get("watch_patterns"), background=inp.get("background", False)))
    if func == "isolation_key":
        return run_flow(lambda: NS["_is_isolation_override_key"](inp.get("key", "")))
    if func in ("session_isolated", "docker_session_isolated", "docker_profile_scoped"):
        scope = NS["_SessionScope"](inp.get("env_type", ""), bool(inp.get("persistent", False)))
        return run_flow(lambda: getattr(scope, func))
    if func == "session_scope":
        return run_flow(lambda: NS["_session_scope"]())
    if func == "docker_session_isolation_enabled":
        return run_flow(lambda: REAL_DSE())
    if func == "has_isolation_overrides":
        return run_flow(lambda: NS["_has_isolation_overrides"](inp.get("task_id")))
    if func == "register_container_alias":
        return run_flow(lambda: NS["register_container_alias"](
            inp.get("child", ""), inp.get("parent")))
    if func == "maybe_reap_docker_orphans":
        # The import gate fires lazily at the live `from ... import` statement.
        make_docker_mod()
        return run_flow(lambda: NS["_maybe_reap_docker_orphans"](inp.get("config") or {}))
    if func == "refuse_missing_local_workdir":
        return run_flow(lambda: NS["_refuse_missing_local_workdir"](
            inp.get("workdir", ""), mat_env(inp.get("env"))))
    if func == "resolve_command_cwd":
        return run_flow(lambda: NS["_resolve_command_cwd"](
            workdir=inp.get("workdir"), default_cwd=inp.get("default_cwd", ""),
            session_key=inp.get("session_key"), env_type=inp.get("env_type"),
            mounted_host=inp.get("mounted_host"), env=inp.get("env")))
    if func == "resolve_task_host_cwd":
        return run_flow(lambda: NS["_resolve_task_host_cwd"](inp.get("config"), inp.get("task_id")))
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
    return ("str", str(x))


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
        SEQPOS.clear()
        NS["logger"] = RecLogger()
        logger = RecLogger()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": echo_state()}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE terminal_tool_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
