#!/usr/bin/env python3
"""Oracle for the tools/terminal_tool.py UNCLAIMED-remainder FLOW cores
(src/tools/terminal_tool_flow2.c).

AST-extracts the LIVE top-level helpers (no heavy tools.terminal_tool import),
seeds the module constants / classes / logger, and replaces every live I/O
dependency with recording stubs driven by the SAME per-name scripts the C
harness feeds its universal seams (in.io). Compares the ordered seam-call trace
plus the return value and the mutated module state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text} raises a real exception of that kind; a scripted
null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for <name>"
(recorded as ret, then raised). BaseException kinds pass through every except
site (the only catches are `except OSError` / `except ImportError`). Ported
siblings run inline (real) on both sides; claimed / foreign siblings are seams.

Input: stdin JSON lines from t_port_terminal_tool_flow2.
Summary: ORACLE terminal_tool_flow2: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import dataclasses
import hashlib  # noqa: F401  (workspace_key_for's in-function import resolves real)
import json
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


class EnvDouble:
    """Env attribute double: getattr-with-default reads (absent -> the default)."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


def mat_env(v):
    return EnvDouble(v) if isinstance(v, dict) else v


def denorm(x):
    if isinstance(x, EnvDouble):
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
    "_container_profile", "_sanitize_cwd_for_live_env", "_shell_state_chain",
    "_session_scope", "_routed_home_task_key", "_sessionless_container_key",
    "workspace_key_for", "_workspace_container_key", "_select_image",
    "_lookup_active_env", "_session_workspace_mount_source", "_backend_default_cwd",
    "_resolve_config_cwd", "_rewrite_via_env_mount", "_mount_envs",
    "_container_visible_cwd", "_container_visible_default", "_fatal_error_json",
    "_acquire_env", "_yield_kwargs",
)
CLASSES = ("_SessionScope", "_ExecPlan", "_Rejected")
CONSTS = ("_IMAGE_KEY_BY_BACKEND", "_DEFAULT_CWD_BY_BACKEND")


def extract():
    path = os.path.join(QUARRY, "tools", "terminal_tool.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "tt2_extracted", "__file__": path,
          "dataclass": dataclasses.dataclass, "logger": RecLogger()}
    sys.modules.setdefault("tt2_extracted", types.ModuleType("tt2_extracted"))
    flags = __future__.annotations.compiler_flag
    tree = ast.parse(src)
    # _VERCEL_SANDBOX_DEFAULT_CWD lives in terminal_tool_backends.py
    bpath = os.path.join(QUARRY, "tools", "terminal_tool_backends.py")
    bsrc = open(bpath, encoding="utf-8").read()
    for node in ast.parse(bsrc).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_VERCEL_SANDBOX_DEFAULT_CWD" for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), bpath, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in CONSTS for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in PORTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = (set(PORTED) | set(CLASSES) | set(CONSTS)) - set(ns)
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


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


def install_globals():
    NS["logger"] = RecLogger()
    NS["json"] = json   # _fatal_error_json's json.dumps (pure serialization, real)
    NS["threading"] = threading
    NS["time"] = types.SimpleNamespace(time=lambda: materialize(script("time.time", {})))
    NS["os"] = types.SimpleNamespace(
        name="posix", environ={},
        path=types.SimpleNamespace(
            expanduser=seamfn("os.path.expanduser", lambda path: {"path": path}),
            abspath=seamfn("os.path.abspath", lambda path: {"path": path}),
            isdir=seamfn("os.path.isdir", lambda path: {"path": path}),
            isabs=seamfn("os.path.isabs", lambda path: {"path": path}),
            realpath=seamfn("os.path.realpath", lambda path: {"path": path}),
        ),
    )
    # module locks (single-threaded model: real locks, no contention)
    for lk in ("_env_lock", "_creation_locks_lock", "_container_alias_lock", "_session_cwd_lock"):
        NS[lk] = threading.Lock()
    # claimed siblings (bare globals) -> seams
    NS["_current_session_profile"] = seamfn("current_session_profile", lambda: {})
    NS["_ensure_terminal_env_bridged"] = seamfn("ensure_terminal_env_bridged", lambda: {})
    NS["_resolve_container_alias"] = seamfn("resolve_container_alias", lambda task_id: {"task_id": task_id})
    NS["_qualify_task_key"] = seamfn("qualify_task_key", lambda key: {"key": key})
    NS["resolve_task_overrides"] = seamfn("resolve_task_overrides", lambda task_id: {"task_id": task_id})
    NS["_redact_terminal_error_text"] = seamfn("redact_terminal_error_text", lambda value: {"value": value})
    NS["_start_cleanup_thread"] = seamfn("start_cleanup_thread", lambda: {})

    def _error_json(error, *, exit_code=-1, status=None, **extra):
        args = {"error": error, "exit_code": exit_code, "status": status}
        args.update(extra)
        return materialize(script("error_json", args))
    NS["_error_json"] = _error_json

    # foreign terminal_tool_config leaves (bare globals) -> seams
    NS["home_profile_name"] = seamfn("home_profile_name", lambda: {})
    NS["profile_container_key"] = seamfn("profile_container_key", lambda profile: {"profile": profile})
    NS["coerce_ssh_remote_cwd"] = seamfn("coerce_ssh_remote_cwd", lambda cwd, env_type: {"cwd": cwd, "env_type": env_type})
    NS["_is_container_backend"] = seamfn("is_container_backend", lambda env_type: {"env_type": env_type})
    NS["_is_unusable_container_cwd"] = seamfn(
        "is_unusable_container_cwd", lambda cwd, *, mounted_host=None: {"cwd": cwd, "mounted_host": mounted_host})
    NS["_is_mounted_host_cwd"] = seamfn("is_mounted_host_cwd", lambda cwd, mounted_host: {"cwd": cwd, "mounted_host": mounted_host})
    NS["translate_mounted_host_path"] = seamfn(
        "translate_mounted_host_path",
        lambda path, host_root, container_root: {"path": path, "host_root": host_root, "container_root": container_root})
    NS["_tenv"] = seamfn("tenv", lambda name, default="": {"name": name, "default": default})
    NS["_tenv_bool"] = seamfn("tenv_bool", lambda name, default: {"name": name, "default": default})
    NS["_safe_getcwd"] = seamfn("safe_getcwd", lambda: {})
    NS["_plugin_env_flag"] = seamfn("plugin_env_flag", lambda env_type, attr, default=False: {"env_type": env_type, "attr": attr, "default": default})
    NS["_is_host_cwd"] = seamfn("is_host_cwd", lambda path: {"path": path})
    NS["_is_windows_drive_path"] = seamfn("is_windows_drive_path", lambda path: {"path": path})
    # foreign lifecycle leaf
    NS["_create_configured_env"] = seamfn(
        "create_configured_env",
        lambda config, env_type, *, image, cwd, timeout, task_id, host_cwd: {
            "config": config, "env_type": env_type, "image": image, "cwd": cwd,
            "timeout": timeout, "task_id": task_id, "host_cwd": host_cwd})

    # yield_to_background_handler: REAL strict kwarg signature, exec'd in a
    # namespace whose __name__ matches the oracle module name so CPython's
    # binding TypeErrors carry the deterministic prefix the C port replicates.
    yns = {"__name__": "oracle_terminal_tool_flow2", "__rec__": None}
    exec(
        "def yield_to_background_handler(*, command, env_type, cwd, effective_task_id, task_id, session_key):\n"
        "    return __rec__({'command': command, 'env_type': env_type, 'cwd': cwd,\n"
        "                    'effective_task_id': effective_task_id, 'task_id': task_id, 'session_key': session_key})\n",
        yns)

    def _ytbh_rec(args):
        return materialize(script("yield_to_background_handler", args))
    yns["__rec__"] = _ytbh_rec
    NS["yield_to_background_handler"] = yns["yield_to_background_handler"]

    # in-function imports -> sys.modules stubs (resolve at call time)
    pkg("tools"); pkg("tools.environments"); pkg("hermes_cli")
    stubmod("hermes_constants",
            get_hermes_home_override=seamfn("get_hermes_home_override", lambda: {}),
            profile_name_for_home=seamfn("profile_name_for_home", lambda path: {"home": path}))
    stubmod("tools.environments.local",
            _is_routed_home=seamfn("is_routed_home", lambda target_home: {"home": target_home}))
    stubmod("tools.environments.sandbox_scope",
            hosted_scope=seamfn("hosted_scope", lambda: {}),
            admit_env=seamfn("admit_env", lambda env: {"env": denorm(env)}))
    stubmod("hermes_cli.config",
            _is_ssh_remote_tilde_cwd=seamfn("is_ssh_remote_tilde_cwd", lambda backend, cwd: {"backend": backend, "cwd": cwd}))
    stubmod("traceback", format_exc=seamfn("traceback.format_exc", lambda: {}))


install_globals()


def build_state(inp):
    st = inp.get("state") or {}
    NS["_active_environments"] = {k: mat_env(v) for k, v in (st.get("active_environments") or {}).items()}
    NS["_last_activity"] = dict(st.get("last_activity") or {})
    NS["_task_env_overrides"] = {k: (dict(v) if isinstance(v, dict) else v)
                                 for k, v in (st.get("task_env_overrides") or {}).items()}
    NS["_container_aliases"] = dict(st.get("container_aliases") or {})
    NS["_creation_locks"] = {k: threading.Lock() for k in (st.get("creation_locks") or {})}
    NS["os"].name = st.get("os_name", "posix")


def echo_state():
    return {
        "active_environments": {k: denorm(v) for k, v in NS["_active_environments"].items()},
        "last_activity": dict(NS["_last_activity"]),
        "task_env_overrides": {k: denorm(v) for k, v in NS["_task_env_overrides"].items()},
        "container_aliases": dict(NS["_container_aliases"]),
        "creation_locks": {k: True for k in NS["_creation_locks"]},
        "os_name": NS["os"].name,
    }


def run_flow(fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": echo_state()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": echo_state()}


def drive(func, inp):
    live = func if func == "workspace_key_for" else "_" + func
    build_state(inp)
    f = NS[live]

    if func == "container_profile":
        return run_flow(lambda: f())
    if func == "sanitize_cwd_for_live_env":
        return run_flow(lambda: f(mat_env(inp.get("env")), inp.get("new_cwd", "")))
    if func == "shell_state_chain":
        return run_flow(lambda: f(inp.get("task_id")))
    if func == "session_scope":
        return run_flow(lambda: f())
    if func == "routed_home_task_key":
        return run_flow(lambda: f(inp.get("profile_scoped", False)))
    if func == "sessionless_container_key":
        return run_flow(lambda: f(inp.get("profile_scoped", False)))
    if func == "workspace_key_for":
        return run_flow(lambda: f(inp.get("cwd", "")))
    if func == "workspace_container_key":
        return run_flow(lambda: f(inp.get("base", ""), inp.get("task_id")))
    if func == "select_image":
        return run_flow(lambda: f(inp.get("env_type", ""), inp.get("overrides"), inp.get("config")))
    if func == "lookup_active_env":
        return run_flow(lambda: f(inp.get("effective_task_id", ""), inp.get("task_id")))
    if func == "session_workspace_mount_source":
        return run_flow(lambda: f(inp.get("task_id")))
    if func == "backend_default_cwd":
        return run_flow(lambda: f(inp.get("env_type", "")))
    if func == "resolve_config_cwd":
        return run_flow(lambda: f(inp.get("env_type", ""), inp.get("mount_docker_cwd", False)))
    if func == "rewrite_via_env_mount":
        return run_flow(lambda: f(inp.get("path", ""), mat_env(inp.get("env"))))
    if func == "mount_envs":
        return run_flow(lambda: f(mat_env(inp.get("env"))))
    if func == "container_visible_cwd":
        return run_flow(lambda: f(inp.get("path", ""), inp.get("env_type"), mat_env(inp.get("env"))))
    if func == "container_visible_default":
        return run_flow(lambda: f(inp.get("default_cwd", ""), inp.get("env_type"), mat_env(inp.get("env"))))
    if func == "fatal_error_json":
        exc = make_exc(inp.get("exc") or {"__raise__": "RuntimeError", "msg": ""})
        return run_flow(lambda: f(exc))
    if func == "acquire_env":
        plan = NS["_ExecPlan"](**(inp.get("plan") or {}))
        return run_flow(lambda: f(plan, inp.get("task_id")))
    if func == "yield_kwargs":
        return run_flow(lambda: f(inp.get("command", ""), **(inp.get("ctx") or {})))
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
    return ("str", x)


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
    print("ORACLE terminal_tool_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())