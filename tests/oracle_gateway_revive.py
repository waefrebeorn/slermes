#!/usr/bin/env python3
"""Oracle for the revived gateway gap ports (DEVIL'S-ADVOCATE REVIVAL):
  src/hermes_cli/gateway_gaps2.c  — hermes_cli/gateway.py + gateway_launchd.py
  src/gateway/gateway_gaps.c      — plugin_keyless_inject / session_command_sink /
                                    status_scoped_locks / systemd_stop_mark /
                                    session_skin / gateway_side
  src/gateway/run_gaps.c          — run.py / run_shutdown.py / run_startup.py /
                                    run_adapters.py

AST-extracts the LIVE defs (no heavy quarry import), seeds module-level
Assign + AnnAssign constants, and replaces every live I/O dependency with
recording stubs driven by the SAME per-name scripts the C harness feeds its
universal seams (in.io). Compares the ordered seam-call trace plus the return
value (and the mutated config for instantiate_adapter).

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion); {"__raise__": kind,
"msg": text} raises a real exception of that kind; absent -> None.

Modeling: AttrDouble attribute access raises AttributeError("missing attribute
'<name>'") exactly like the C ports; Path doubles normalize with as_posix();
socket.AF_UNIX/AF_INET seeded 1/2 (Windows Python lacks AF_UNIX); psutil
parent chains modeled by pid; deferred-worker futures/agents modeled by id
strings (shared AgentDouble references give id() dedup); the async redeliver
flow runs on a synchronous asyncio double (to_thread/ensure_future/shield)
where a scripted CancelledError sweep may carry "owned" committed claim rows.

Input: stdin JSON lines from t_port_gateway_revive.
Summary: ORACLE gateway_revive: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import contextlib
import contextvars
import json
import os
import subprocess
import sys
import types
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    """Consume the case script for a seam and record the event (C twin)."""
    io = CUR["in"].get("io") or {}
    sc = io.get(name)
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


class AuthorityRefusal(Exception):
    pass


class _ScriptedTimeoutExpired(subprocess.TimeoutExpired):
    def __str__(self):
        return getattr(self, "_smsg", "")


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "TimeoutExpired":
        e = _ScriptedTimeoutExpired("launchctl", 5)
        e._smsg = msg
        return e
    if kind == "AuthorityRefusal":
        return AuthorityRefusal(msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError,
        "AttributeError": AttributeError, "KeyError": KeyError,
        "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError,
        "ImportError": ImportError,
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


class AttrDouble:
    """Attribute-bearing double; missing attrs raise the C ports' exact text."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class ConfigDouble(AttrDouble):
    @property
    def extra(self):
        d = object.__getattribute__(self, "_d")
        if "extra" in d:
            return d["extra"]
        raise AttributeError("missing attribute 'extra'")


class PathDouble:
    def __init__(self, s):
        self.s = str(s)

    def is_socket(self):
        return materialize(script("path.is_socket", {"path": self.s}))

    def as_posix(self):
        return self.s.replace("\\", "/")

    def __str__(self):
        return self.s

    def __truediv__(self, other):
        base = self.as_posix().rstrip("/")
        return PathDouble(base + "/" + str(other) if base else str(other))


class ProcessDouble:
    def __init__(self, pid):
        self.pid = pid

    def parent(self):
        v = materialize(script("process.parent", {"pid": self.pid}))
        return ProcessDouble(v) if v is not None else None

    def name(self):
        return materialize(script("process.name", {"pid": self.pid}))


class FutureDouble:
    def __init__(self, fid):
        self.fid = fid

    def done(self):
        return materialize(script("future.done", {"future": self.fid}))


class AgentDouble:
    def __init__(self, label):
        self.label = label


def denorm(x):
    if isinstance(x, AttrDouble):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, PathDouble):
        return x.as_posix()
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    return x


def norm_nums(x):
    """Integral floats -> ints so live str()/int() see JSON's int identity."""
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


def extract(rel, tops=(), methods=(), need=(), pre=None, seed=True):
    path = os.path.join(QUARRY, rel)
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "gw_revive_" + rel.replace("/", "_").replace(".", "_"),
          "__file__": path}
    if pre:
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

    if seed:
        seed_assigns()
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in tops:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in methods:
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), path, "exec", flags=flags), ns)
    if seed:
        seed_assigns()
    for n in need:
        if n not in ns:
            raise RuntimeError("seed missing: %s in %s" % (n, rel))
    return ns


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# ── namespaces (live defs) ──

NS_GW = extract(
    "hermes_cli/gateway.py",
    tops=("_probe_loop_tick_socket", "_probe_loop_tick_tcp",
          "_probe_loop_tick_socket_sustained", "_parse_launchd_pid_from_print_output",
          "_launchd_print_service_pid", "_reaper_candidate_is_supervisor_owned",
          "_windows_scheduled_task_supervises", "_windows_console_window_attached",
          "_windows_gateway_breakaway_state", "named_profile_served_by_running_multiplexer",
          "_positive_pid"),
    need=("_REAPER_SUPERVISOR_WALK_LIMIT", "_WINDOWS_TASK_SUPERVISOR_STATES", "_CAPTURE_TEXT"))
assert NS_GW["_REAPER_SUPERVISOR_WALK_LIMIT"] == 12
assert NS_GW["_WINDOWS_TASK_SUPERVISOR_STATES"] == frozenset({"Running", "Ready", "Queued"})

NS_LAUNCHD = extract("hermes_cli/gateway_launchd.py",
                     tops=("_launchctl_label_supervising_process", "_wait_for_launchd_service_pid"))
NS_DEADLINE = extract("agent/deadline.py", tops=("poll_until",))
NS_STATUS = extract("gateway/status.py", tops=("_start_times_conflict",))
NS_KEYLESS = extract("gateway/plugin_keyless_inject.py", tops=("_launched_by_classic_cli",))
NS_SINK = extract("gateway/session_command_sink.py", tops=("active_sink",),
                  pre={"contextvars": contextvars})
NS_SCOPED = extract("gateway/status_scoped_locks.py", tops=("_scoped_lock_record_is_stale",))
NS_STOPMARK = extract("gateway/systemd_stop_mark.py", tops=("_resolve_target_pid",))
NS_SKIN = extract("gateway/session_skin.py", tops=("resolved_skin",))
NS_SIDE = extract("hermes_cli/gateway_side.py", tops=("connection_for",))
NS_RUN = extract("gateway/run.py",
                 tops=("_warm_turn_machinery_sync", "_drain_restart_safe_cron_deliveries"),
                 seed=False)
NS_SHUTDOWN = extract("gateway/run_shutdown.py",
                      tops=("_log_suppressed",),
                      methods=("_active_deferred_agent_worker_count", "_interrupt_deferred_agent_workers"),
                      need=("_log_suppressed",),
                      pre={"contextmanager": contextlib.contextmanager, "logger": RecLogger()})
NS_ADAPTERS = extract("gateway/run_adapters.py",
                      methods=("_instantiate_adapter",),
                      pre={"logging": types.SimpleNamespace(DEBUG=10), "logger": RecLogger()},
                      seed=False)
NS_STARTUP = extract("gateway/run_startup.py",
                     methods=("_redeliver_failed_obligations_for_platform",),
                     pre={"suppress": contextlib.suppress, "logger": RecLogger()},
                     seed=False)
NS_SHUTDOWN["_log_suppressed"].__globals__["logger"] = RecLogger()
NS_ADAPTERS["_log_suppressed"] = NS_SHUTDOWN["_log_suppressed"]

# ── foreign-module doubles (in-function imports) ──

M_GATEWAY = mod("gateway")
M_HERMES_CLI = mod("hermes_cli")
M_PERSISTENCE = mod("persistence")
M_PERSISTENCE_CORE = mod("persistence.core")
M_CRON = mod("cron")
M_TOOLS = mod("tools")
M_GATEWAY.session_policy = mod("gateway.session_policy")
M_PERSISTENCE.core = M_PERSISTENCE_CORE
M_PERSISTENCE_CORE.errors = mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
M_GATEWAY.shutdown_watchdog = mod("gateway.shutdown_watchdog")
M_GATEWAY.delivery_ledger = mod("gateway.delivery_ledger")
M_GATEWAY.platform_registry = mod("gateway.platform_registry")
M_GATEWAY.run = mod("gateway.run")
M_HERMES_CLI.profiles = mod("hermes_cli.profiles")
M_HERMES_CLI.gateway_multiplex_served = mod("hermes_cli.gateway_multiplex_served")
M_HERMES_CLI.gateway_multiplex_mode = mod("hermes_cli.gateway_multiplex_mode")
M_HERMES_CLI._subprocess_compat = mod("hermes_cli._subprocess_compat",
                                      _WINDOWS_GATEWAY_BREAKAWAY_ENV="_HERMES_GATEWAY_BREAKAWAY")
M_HERMES_CLI.config_effective = mod("hermes_cli.config_effective")
M_HERMES_CLI.skin_engine = mod("hermes_cli.skin_engine")
M_HERMES_CLI.gateway_tools_notify = mod("hermes_cli.gateway_tools_notify")
M_HERMES_CLI.config = mod("hermes_cli.config")
M_HERMES_CLI.tools_config = mod("hermes_cli.tools_config")
M_HERMES_CONSTANTS = mod("hermes_constants")
M_CRON.scheduler = mod("cron.scheduler")
M_CRON.scheduler_preflight = mod("cron.scheduler_preflight")
M_TOOLS.env_probe = mod("tools.env_probe")
M_MODEL_TOOLS = mod("model_tools")
mod("run_agent")
M_PSUTIL = mod("psutil", Process=ProcessDouble)
M_CTYPES = mod("ctypes")


class _Claim:
    def __init__(self, result=None, exc=None, cancel_shield=False):
        self.result, self.exc, self.cancel_shield = result, exc, cancel_shield

    def __await__(self):
        if self.exc is not None:
            raise self.exc
        if False:
            yield
        return self.result


class _AsyncioDouble:
    CancelledError = asyncio.CancelledError

    @staticmethod
    def to_thread(fn, *a, **k):
        try:
            r = fn(*a, **k)
        except BaseException as e:
            owned = CUR.pop("sweep_cancel_owned", None)
            if isinstance(e, asyncio.CancelledError) and owned is not None:
                return _Claim(result=owned, cancel_shield=True)
            return _Claim(exc=e)
        return _Claim(result=r)

    @staticmethod
    def ensure_future(x):
        return x

    @staticmethod
    def shield(x):
        if isinstance(x, _Claim) and x.cancel_shield:
            return _Claim(exc=asyncio.CancelledError())
        return x


def run_coro(coro):
    try:
        coro.send(None)
    except StopIteration as si:
        return si.value
    raise RuntimeError("coroutine yielded unexpectedly")


class _ScopeCM:
    def __init__(self, home):
        self.home = home

    def __enter__(self):
        h = self.home.as_posix() if isinstance(self.home, PathDouble) else self.home
        materialize(script("profile_runtime_scope.enter", {"profile_home": h}))
        return self

    def __exit__(self, *a):
        materialize(script("profile_runtime_scope.exit", {}))
        return False


def install_case():
    """Fresh recording stubs for every seam, all reading CUR['in'].io."""
    SEQPOS.clear()
    CUR.pop("sweep_cancel_owned", None)
    logger = RecLogger()

    # gateway.py namespace
    NS_GW["logger"] = logger
    NS_GW["socket"] = types.SimpleNamespace(AF_UNIX=1, AF_INET=2)
    NS_GW["contextlib"] = contextlib
    NS_GW["is_windows"] = lambda: materialize(script("is_windows", {}))
    NS_GW["subprocess"] = types.SimpleNamespace(run=_subprocess_run)
    NS_GW["time"] = types.SimpleNamespace(
        sleep=lambda s: materialize(script("time.sleep", {"seconds": s})))
    NS_GW["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(
        pop=lambda key, default=None: materialize(script("os.environ.pop",
                                                         {"key": key, "default": default}))))
    NS_GW["_ping_loop_tick_witness"] = _ping
    NS_GW["_windows_scheduled_task_state"] = lambda name: materialize(
        script("_windows_scheduled_task_state", {"task_name": name}))
    NS_GW["_current_profile_name"] = lambda: materialize(script("_current_profile_name", {}))
    NS_GW["host_multiplexer_serving"] = _host_multiplexer
    M_GATEWAY.shutdown_watchdog.get_loop_tick_socket_path = _loop_tick_path
    M_HERMES_CLI.profiles.normalize_profile_name = lambda name: materialize(
        script("normalize_profile_name", {"name": name}))
    M_HERMES_CONSTANTS.get_default_hermes_root = lambda: materialize(
        script("get_default_hermes_root", {}))
    M_HERMES_CLI.gateway_multiplex_served.live_default_gateway_pid = lambda: materialize(
        script("live_default_gateway_pid", {}))
    M_HERMES_CLI.gateway_multiplex_served.recorded_served_profiles = lambda root: materialize(
        script("recorded_served_profiles", {"default_root": root}))
    M_HERMES_CLI.gateway_multiplex_mode.explicit_multiplex_flag = lambda root: materialize(
        script("explicit_multiplex_flag", {"default_root": root}))
    M_CTYPES.windll = types.SimpleNamespace(kernel32=types.SimpleNamespace(
        GetConsoleWindow=lambda: materialize(script("GetConsoleWindow", {}))))

    # gateway_launchd.py namespace
    NS_LAUNCHD["_gw"] = lambda: types.SimpleNamespace(
        _launchctl_supervised_pid=lambda label: materialize(
            script("_launchctl_supervised_pid", {"label": label})),
        _launchd_print_service_pid=NS_GW["_launchd_print_service_pid"])
    NS_LAUNCHD["poll_until"] = NS_DEADLINE["poll_until"]
    NS_DEADLINE["time"] = types.SimpleNamespace(
        monotonic=lambda: materialize(script("time.monotonic", {})),
        sleep=lambda s: materialize(script("time.sleep", {"seconds": s})))

    # plugin_keyless_inject / session_command_sink
    M_GATEWAY.session_policy.policy_for_source = _policy_for_source
    NS_SINK["_SINK"] = types.SimpleNamespace(
        get=lambda: materialize(script("_SINK.get", {})))

    # status_scoped_locks
    NS_SCOPED["scoped_lock_pid_is_ours"] = lambda record: materialize(
        script("scoped_lock_pid_is_ours", {"record": record}))
    NS_SCOPED["_foreign_owner_is_live"] = lambda existing: materialize(
        script("_foreign_owner_is_live", {"existing": existing}))
    NS_SCOPED["_status"] = types.SimpleNamespace(
        logger=logger,
        _pid_exists=lambda pid: materialize(script("_pid_exists", {"pid": pid})),
        _get_process_start_time=lambda pid: materialize(script("_get_process_start_time", {"pid": pid})),
        _start_times_conflict=NS_STATUS["_start_times_conflict"],
        _looks_like_gateway_process=lambda pid: materialize(script("_looks_like_gateway_process", {"pid": pid})),
        _read_process_cmdline=lambda pid: materialize(script("_read_process_cmdline", {"pid": pid})),
        _record_looks_like_gateway=lambda record: materialize(script("_record_looks_like_gateway", {"record": record})),
        _process_is_stopped=lambda pid: materialize(script("_process_is_stopped", {"pid": pid})))

    # systemd_stop_mark
    NS_STOPMARK["os"] = types.SimpleNamespace(environ=types.SimpleNamespace(
        get=lambda key: materialize(script("os.environ.get", {"key": key}))))

    # session_skin
    NS_SKIN["logger"] = logger
    M_HERMES_CONSTANTS.get_hermes_home = lambda: PathDouble(materialize(script("get_hermes_home", {})))
    M_HERMES_CLI.config_effective.load_user_config_effective = lambda path: materialize(
        script("load_user_config_effective", {"path": path.as_posix()}))
    M_HERMES_CLI.skin_engine.init_skin_from_config = lambda config: materialize(
        script("init_skin_from_config", {"config": config}))
    M_HERMES_CLI.skin_engine.get_active_skin = _active_skin

    # gateway_side
    M_HERMES_CLI.gateway_tools_notify.gateway_store = _gateway_store

    # run.py
    NS_RUN["logger"] = logger
    NS_RUN["get_hermes_home"] = lambda: materialize(script("get_hermes_home", {}))
    NS_RUN["_resolve_gateway_model_context"] = _model_context
    NS_RUN["_handoff_watch_scopes"] = lambda runner: materialize(
        script("_handoff_watch_scopes", {"runner": denorm(runner)}))
    NS_RUN["_profile_runtime_scope"] = lambda home: _ScopeCM(home)
    M_HERMES_CLI.config.load_config_readonly = lambda: materialize(script("load_config_readonly", {}))
    M_HERMES_CLI.tools_config._get_platform_tools = lambda config, platform: materialize(
        script("_get_platform_tools", {"config": config, "platform": platform}))
    M_MODEL_TOOLS.get_tool_definitions = lambda enabled_toolsets=None, quiet_mode=False: materialize(
        script("get_tool_definitions", {"enabled_toolsets": enabled_toolsets, "quiet_mode": quiet_mode}))
    M_TOOLS.env_probe.get_environment_probe_line = lambda: materialize(
        script("get_environment_probe_line", {}))
    M_CRON.scheduler.drain_delivery_queue = lambda adapters, loop: materialize(
        script("drain_delivery_queue", {"adapters": adapters, "loop": loop}))
    M_CRON.scheduler_preflight.has_credential_platform = lambda pa, owned: materialize(
        script("has_credential_platform", {"profile_adapters": pa, "owned": owned}))
    M_CRON.scheduler_preflight.shared_route_adapters_for_current_home = lambda adapters: materialize(
        script("shared_route_adapters_for_current_home", {"adapters": adapters}))

    # run_shutdown.py
    NS_SHUTDOWN["logger"] = logger
    M_GATEWAY.run._INTERRUPT_TOOL_REASON_GATEWAY_SHUTDOWN = "gateway shutdown"
    M_GATEWAY.run.request_hard_interrupt = _request_hard_interrupt

    # run_adapters.py
    NS_ADAPTERS["logger"] = logger
    NS_ADAPTERS["_log_suppressed"] = NS_SHUTDOWN["_log_suppressed"]
    M_GATEWAY.platform_registry.platform_registry = types.SimpleNamespace(
        is_registered=lambda value: materialize(script("platform_registry.is_registered", {"platform": value})),
        create_adapter=_create_adapter)
    M_GATEWAY.run._instantiate_builtin_adapter = _builtin_adapter

    # run_startup.py (async redeliver)
    NS_STARTUP["logger"] = logger
    NS_STARTUP["asyncio"] = _AsyncioDouble
    M_GATEWAY.delivery_ledger.ledger_enabled = lambda: materialize(script("ledger_enabled", {}))
    M_GATEWAY.delivery_ledger.sweep_failed_for_runtime = _sweep
    M_GATEWAY.delivery_ledger.release_runtime_claim = lambda obligation_id, error=None: materialize(
        script("release_runtime_claim", {"obligation_id": obligation_id, "error": error}))


def _subprocess_run(argv, timeout=None, **kw):
    args = {"argv": list(argv), "timeout": timeout}
    args.update(kw)
    v = materialize(script("subprocess.run", args))
    if v is None:
        return None
    return types.SimpleNamespace(**v)


def _ping(family, address, timeout):
    addr = list(address) if isinstance(address, tuple) else str(address)
    return materialize(script("_ping_loop_tick_witness",
                              {"family": family, "address": addr, "timeout": timeout}))


def _loop_tick_path(home, pid):
    v = materialize(script("get_loop_tick_socket_path",
                           {"home": home.as_posix() if home is not None else None, "pid": pid}))
    return PathDouble(v) if v is not None else None


def _host_multiplexer(suffix):
    v = materialize(script("host_multiplexer_serving", {"suffix": suffix}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _policy_for_source(runner, source):
    v = materialize(script("policy_for_source", {"runner": runner, "source": source}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _active_skin():
    v = materialize(script("get_active_skin", {}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _gateway_store(cli):
    v = materialize(script("gateway_store", {"cli": CUR["in"].get("cli")}))
    if not isinstance(v, dict):
        return v
    conn = v.get("connection")
    return AttrDouble({"connection": AttrDouble(conn) if isinstance(conn, dict) else conn})


def _model_context():
    v = materialize(script("_resolve_gateway_model_context", {}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _request_hard_interrupt(agent, reason, tool_reason=None):
    return materialize(script("request_hard_interrupt",
                              {"agent": agent.label if agent is not None else None,
                               "reason": reason, "tool_reason": tool_reason}))


def _create_adapter(value, config):
    v = materialize(script("platform_registry.create_adapter",
                           {"platform": value, "config": object.__getattribute__(config, "_d")}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _builtin_adapter(platform, config):
    v = materialize(script("_instantiate_builtin_adapter",
                           {"platform": object.__getattribute__(platform, "_d"),
                            "config": object.__getattribute__(config, "_d")}))
    return AttrDouble(v) if isinstance(v, dict) else v


def _sweep(platform, profile=None):
    v = script("sweep_failed_for_runtime", {"platform": platform, "profile": profile})
    if isinstance(v, dict) and v.get("__raise__") == "CancelledError" and "owned" in v:
        CUR["sweep_cancel_owned"] = v["owned"]
    return materialize(v)


def workers_double(inp):
    runner = inp.get("runner") or {}
    w = runner.get("_deferred_agent_workers", "__missing__")
    out = {}
    if isinstance(w, list):
        agents = {}
        workers = {}
        for entry in w:
            f = FutureDouble(entry["future"])
            label = entry.get("agent")
            if label is None:
                workers[f] = None
            else:
                if label not in agents:
                    agents[label] = AgentDouble(label)
                workers[f] = agents[label]
        out["_deferred_agent_workers"] = workers
    elif w != "__missing__":
        out["_deferred_agent_workers"] = w
    for k, v in runner.items():
        if k != "_deferred_agent_workers":
            out[k] = v
    return AttrDouble(out)


def drive(func, inp):
    if func == "probe_loop_tick_socket":
        home = PathDouble(inp["home"]) if inp.get("home") is not None else None
        return {"ret": NS_GW["_probe_loop_tick_socket"](inp.get("pid", 0), home,
                                                        timeout=inp.get("timeout", 1.0))}
    if func == "probe_loop_tick_tcp":
        return {"ret": NS_GW["_probe_loop_tick_tcp"](inp.get("port"), timeout=inp.get("timeout", 1.0))}
    if func == "probe_loop_tick_socket_sustained":
        home = PathDouble(inp["home"]) if inp.get("home") is not None else None
        return {"ret": NS_GW["_probe_loop_tick_socket_sustained"](
            inp.get("pid", 0), home, timeout=inp.get("timeout", 1.0),
            strikes=inp.get("strikes", 3), gap_s=inp.get("gap_s", 0.2),
            tcp_port=inp.get("tcp_port"))}
    if func == "parse_launchd_pid_from_print_output":
        return {"ret": NS_GW["_parse_launchd_pid_from_print_output"](inp.get("output", ""))}
    if func == "launchd_print_service_pid":
        return {"ret": NS_GW["_launchd_print_service_pid"](inp.get("domain", ""), inp.get("label", ""))}
    if func == "reaper_candidate_is_supervisor_owned":
        return {"ret": NS_GW["_reaper_candidate_is_supervisor_owned"](inp.get("pid", 0))}
    if func == "windows_scheduled_task_supervises":
        return {"ret": NS_GW["_windows_scheduled_task_supervises"](inp.get("task_name", ""))}
    if func == "windows_console_window_attached":
        return {"ret": NS_GW["_windows_console_window_attached"]()}
    if func == "windows_gateway_breakaway_state":
        return {"ret": NS_GW["_windows_gateway_breakaway_state"]()}
    if func == "launchctl_label_supervising_process":
        return {"ret": NS_LAUNCHD["_launchctl_label_supervising_process"](inp.get("label", ""))}
    if func == "wait_for_launchd_service_pid":
        return {"ret": NS_LAUNCHD["_wait_for_launchd_service_pid"](
            inp.get("label", ""), inp.get("old_pid"), inp.get("timeout", 10.0),
            domain=inp.get("domain", ""))}
    if func == "named_profile_served_by_running_multiplexer":
        return {"ret": NS_GW["named_profile_served_by_running_multiplexer"](inp.get("profile_name"))}
    if func == "gateway_plugin_keyless_inject_launched_by_classic_cli":
        live = AttrDouble(inp["live"]) if isinstance(inp.get("live"), dict) else inp.get("live")
        return {"ret": NS_KEYLESS["_launched_by_classic_cli"](inp.get("runner"), live)}
    if func == "gateway_session_command_sink_active_sink":
        return {"ret": NS_SINK["active_sink"]()}
    if func == "gateway_status_scoped_locks_scoped_lock_record_is_stale":
        return {"ret": NS_SCOPED["_scoped_lock_record_is_stale"](
            inp.get("existing") or {}, inp.get("existing_pid"))}
    if func == "gateway_systemd_stop_mark_resolve_target_pid":
        return {"ret": NS_STOPMARK["_resolve_target_pid"](inp.get("argv") or [])}
    if func == "gateway_session_skin_resolved_skin":
        return {"ret": NS_SKIN["resolved_skin"]()}
    if func == "hermes_cli_gateway_side_connection_for":
        cli = AttrDouble(inp["cli"]) if isinstance(inp.get("cli"), dict) else inp.get("cli")
        return {"ret": denorm(NS_SIDE["connection_for"](cli))}
    if func == "warm_turn_machinery_sync":
        return {"ret": NS_RUN["_warm_turn_machinery_sync"]()}
    if func == "active_deferred_agent_worker_count":
        return {"ret": NS_SHUTDOWN["_active_deferred_agent_worker_count"](workers_double(inp))}
    if func == "interrupt_deferred_agent_workers":
        return {"ret": NS_SHUTDOWN["_interrupt_deferred_agent_workers"](
            workers_double(inp), inp.get("reason", ""))}
    if func == "redeliver_failed_obligations_for_platform":
        gw = types.SimpleNamespace()

        def clear_stub(claimed, require_success=False):
            async def _c():
                return materialize(script("_clear_resume_pending_for_claimed_obligations",
                                          {"claimed": claimed, "require_success": require_success}))
            return _c()

        def release_quiet_stub(obligation_id, fmt, error=None):
            async def _r():
                return materialize(script("_release_runtime_claim_quiet",
                                          {"obligation_id": obligation_id, "fmt": fmt, "error": error}))
            return _r()

        def redeliver_stub(sendable):
            async def _d():
                return materialize(script("_redeliver_claimed_obligations", {"sendable": sendable}))
            return _d()

        gw._clear_resume_pending_for_claimed_obligations = clear_stub
        gw._release_runtime_claim_quiet = release_quiet_stub
        gw._redeliver_claimed_obligations = redeliver_stub
        plat = types.SimpleNamespace(value=inp.get("platform", ""))
        coro = NS_STARTUP["_redeliver_failed_obligations_for_platform"](
            gw, plat, profile=inp.get("profile"))
        return {"ret": run_coro(coro)}
    if func == "instantiate_adapter":
        self_d = types.SimpleNamespace(config=AttrDouble(inp.get("self_config") or {}))
        plat = AttrDouble(inp.get("platform") or {})
        cfg = ConfigDouble(inp.get("config") or {})
        try:
            ret = NS_ADAPTERS["_instantiate_adapter"](self_d, plat, cfg)
        except BaseException as e:
            return {"ret": raise_spec(e), "state": denorm(cfg._d)}
        return {"ret": denorm(ret), "state": denorm(cfg._d)}
    if func == "drain_restart_safe_cron_deliveries":
        runner = AttrDouble(inp["runner"]) if isinstance(inp.get("runner"), dict) else inp.get("runner")
        return {"ret": NS_RUN["_drain_restart_safe_cron_deliveries"](
            inp.get("adapters"), inp.get("loop"), runner=runner)}
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
    install_case()  # bind stubs once; they read CUR per case
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
            want = {"ret": raise_spec(e)}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gateway_revive: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())