#!/usr/bin/env python3
"""Oracle for the gateway/run.py FLOW cores + leaves, second wave
(src/gateway/gateway_run_flow2.c).

AST-extracts the LIVE unclaimed defs (no heavy gateway import), seeds the
module constants (_AGENT_ENV_BRIDGE / _SESSIONS_ENV_BRIDGE / _DISPLAY_ENV_BRIDGE,
_DEFAULT_CONFIG fallback, MessageType, STANDALONE_DEPRECATION_NOTICE, the
host_attach outcome constants) and replaces every live I/O dependency with
recording stubs driven by the SAME per-name scripts the C harness feeds its
universal seams (in.io). Compares the ordered seam-call trace plus the return
value and the mutated env state.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused;
a scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds (SystemExit /
KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec.

Modeling: os.environ is mutable STATE (in.env -> out.state.env), reads and
writes are silent; os.path.expanduser is a seam. Claimed siblings are seams
(_is_ssh_remote_tilde_cwd, _event_media_type_at, _handoff_watch_scopes,
_start_gateway_replace_existing_instance); ported siblings compose inline.
Paths are PurePosixPath (POSIX separators on Windows too). MessageType enums
travel as value strings. str() positions are scalar-domain (str/int/bool/
null); json.dumps positions are ASCII containers; float(str) is decimal-
literal domain (no inf/nan/hex results). bridge_config_scalars and
apply_media_policy_env are seamed inert collaborators.

Input: stdin JSON lines from t_port_gateway_run_flow2.
Summary: ORACLE gateway_run_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import json
import math
import os
import sys
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
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError, "ImportError": ImportError,
        "OverflowError": OverflowError, "IndexError": IndexError, "Exception": Exception,
        "StopIteration": StopIteration,
    }
    cls = builtins.get(kind)
    if cls is None:
        if kind == "InvalidUserConfigError":
            # identity matters: the live `except InvalidUserConfigError` binds
            # the stub-module class, so the raised instance must be THAT class
            cls = InvalidUserConfigError
        else:
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


class EnvDouble:
    """os.environ double: silent mutable state (out.state.env carries it)."""
    def __init__(self, d):
        self._d = dict(d)

    def __setitem__(self, k, v):
        self._d[k] = v

    def __delitem__(self, k):
        del self._d[k]

    def __contains__(self, k):
        return k in self._d

    def get(self, k, default=None):
        return self._d.get(k, default)


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class RunnerDouble:
    def __init__(self):
        object.__setattr__(self, "_d", {})

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)

    def __setattr__(self, name, val):
        object.__getattribute__(self, "_d")[name] = val


def denorm(x):
    if isinstance(x, (RunnerDouble, AttrDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, EnvDouble):
        return dict(x._d)
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


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": state_fn()}
# ── live-def extraction ──

WANTED = (
    "_context_overflow_reply", "_rate_limit_reply",
    "_bridge_section_to_env", "_bridge_max_turns_to_env",
    "_bridge_terminal_config_to_env", "_bridge_auxiliary_config_to_env",
    "_bridge_config_to_env", "_load_bridge_config",
    "_best_effort", "_runtime_agent_kwargs", "_event_media_kind_is",
    "_format_gateway_process_notification", "_reconnect_attention_after_secs",
    "_write_runtime_status_quiet", "_clear_takeover_marker_quiet",
    "_housekeeping_chore", "_housekeeping_paste_sweep",
    "_housekeeping_misfire_catch_up", "_housekeeping_memory_trim",
    "_housekeeping_checkpoint_prune", "_housekeeping_plugin_update_check",
    "_launch_sessions_dir", "_profile_sessions_dir",
    "_guard_corrupt_user_config", "_wait_for_pid_exit",
    "_resolve_handoff_watch_scopes", "_log_standalone_profiles_at_boot",
    "_host_attach_or_none",
)


def extract(rel, tops=(), pre=None, seed=True):
    path = os.path.join(QUARRY, rel)
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "gr2_extracted_" + rel.replace("/", "_").replace(".", "_"),
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
    if seed:
        seed_assigns()
    return ns


def const_assign(rel, name):
    path = os.path.join(QUARRY, rel)
    tree = ast.parse(open(path, encoding="utf-8").read())
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            return ns[name]
    raise KeyError(name)


def default_reconnect_attention():
    """DEFAULT_CONFIG['agent']['reconnect_attention_after'] as an AST literal
    (the full dict references module helpers; the leaf value is a literal)."""
    path = os.path.join(QUARRY, "hermes_cli/config_defaults.py")
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "DEFAULT_CONFIG" for t in node.targets):
            for k, v in zip(node.value.keys, node.value.values):
                if isinstance(k, ast.Constant) and k.value == "agent" and isinstance(v, ast.Dict):
                    for k2, v2 in zip(v.keys, v.values):
                        if isinstance(k2, ast.Constant) and k2.value == "reconnect_attention_after":
                            return ast.literal_eval(v2)
    return 7200


def extract_message_type():
    path = os.path.join(QUARRY, "gateway/platforms/event.py")
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "MessageType":
            ns = {"Enum": Enum}
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            return ns["MessageType"]
    raise KeyError("MessageType")


MessageType = extract_message_type()
NOTICE = const_assign("hermes_cli/gateway_multiplex_mode.py", "STANDALONE_DEPRECATION_NOTICE")
ATTACH = const_assign("gateway/host_attach.py", "ATTACH")
REFUSE = const_assign("gateway/host_attach.py", "REFUSE")
REPLACE_HOST = const_assign("gateway/host_attach.py", "REPLACE_HOST")
_DEFAULT_RECONNECT = default_reconnect_attention()

SYS = types.SimpleNamespace(stderr=object(), stdout=object())


def rec_print(*a, file=None, **k):
    args = {"text": a[0] if a else ""}
    if file is SYS.stderr:
        args["file"] = "stderr"
    return seam("print", args)


def t_stub(key, lang=None, **kw):
    args = {"key": key, "kwargs": kw}
    if lang is not None:
        args["lang"] = lang
    return seam("t", args)


def get_hermes_home_stub():
    return PurePosixPath(seam("get_hermes_home", {}))


def bridge_config_scalars_stub(cfg, target):
    return seam("bridge_config_scalars", {"cfg": denorm(cfg), "target": "env"})


def ssh_tilde_stub(backend, cwd):
    return seam("_is_ssh_remote_tilde_cwd", {"backend": backend, "cwd": cwd})


def event_media_type_at_stub(event, index):
    return seam("_event_media_type_at", {"event": denorm(event), "index": index})


def handoff_watch_scopes_stub(runner):
    return seam("_handoff_watch_scopes", {"runner": denorm(runner)})
handoff_watch_scopes_stub.__name__ = "_handoff_watch_scopes"


async def replace_existing_stub(existing_pid, replace):
    return seam("_start_gateway_replace_existing_instance",
                {"existing_pid": existing_pid, "replace": replace})


async def asyncio_sleep_stub(delay):
    return seam("asyncio.sleep", {"delay": delay})


NS = extract("gateway/run.py", tops=WANTED, pre={
    "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger(),
                                     DEBUG=10, INFO=20, WARNING=30, ERROR=40),
    "logger": RecLogger(),
    "json": json, "os": types.SimpleNamespace(environ=EnvDouble({})),
    "re": __import__("re"), "Path": PurePosixPath,
    "t": t_stub, "get_hermes_home": get_hermes_home_stub,
    "bridge_config_scalars": bridge_config_scalars_stub,
    "_is_ssh_remote_tilde_cwd": ssh_tilde_stub,
    "_DEFAULT_CONFIG": {"agent": {"reconnect_attention_after": _DEFAULT_RECONNECT}},
    "MessageType": MessageType,
    "sys": SYS, "print": rec_print,
    "_event_media_type_at": event_media_type_at_stub,
    "_handoff_watch_scopes": handoff_watch_scopes_stub,
    "_start_gateway_replace_existing_instance": replace_existing_stub,
})
# module-level Assign seeding re-execs `logger = logging.getLogger(__name__)`;
# restore the recording logger AFTER extraction (and per case).
NS["logger"] = RecLogger()
NS["Path"] = PurePosixPath
NS["MessageType"] = MessageType
NS["_DEFAULT_CONFIG"] = {"agent": {"reconnect_attention_after": _DEFAULT_RECONNECT}}
NS["sys"] = SYS
NS["print"] = rec_print
NS["t"] = t_stub
NS["get_hermes_home"] = get_hermes_home_stub
NS["bridge_config_scalars"] = bridge_config_scalars_stub
NS["_is_ssh_remote_tilde_cwd"] = ssh_tilde_stub
NS["_event_media_type_at"] = event_media_type_at_stub
NS["_handoff_watch_scopes"] = handoff_watch_scopes_stub
NS["_start_gateway_replace_existing_instance"] = replace_existing_stub


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
mod("cron")
mod("tools")
mod("agent.checkpoints")

mod("agent.retry_utils",
    reset_delay_from_message=lambda text: seam("reset_delay_from_message", {"text": text}),
    format_reset_window=lambda seconds: seam("format_reset_window", {"seconds": seconds}))
mod("agent.safe_worker_policy",
    worker_config_snapshot=lambda: seam("worker_config_snapshot", {}))
mod("hermes_cli.config_effective",
    load_user_config_effective=lambda config_path: seam(
        "load_user_config_effective", {"config_path": denorm(config_path)}))
mod("hermes_cli.plugins",
    get_plugin_auxiliary_tasks=lambda: seam("get_plugin_auxiliary_tasks", {}))
mod("gateway.media_policy",
    apply_media_policy_env=lambda cfg: seam("apply_media_policy_env", {"cfg": denorm(cfg)}))
InvalidUserConfigError = type("InvalidUserConfigError", (Exception,), {})
mod("hermes_cli.config",
    load_config_readonly=lambda: seam("load_config_readonly", {}),
    require_parseable_user_config=lambda: seam("require_parseable_user_config", {}),
    InvalidUserConfigError=InvalidUserConfigError)
mod("gateway.status",
    publish_runtime_status=lambda **fields: seam("publish_runtime_status", dict(fields)),
    clear_takeover_marker=lambda: seam("clear_takeover_marker", {}),
    _pid_exists=lambda pid: seam("_pid_exists", {"pid": pid}))
mod("hermes_cli.debug",
    _sweep_expired_pastes=lambda: seam("_sweep_expired_pastes", {}))
mod("cron.scheduler_provider",
    fire_overdue_jobs=lambda cron_provider, adapters=None, loop=None: seam(
        "fire_overdue_jobs", {"cron_provider": denorm(cron_provider),
                              "adapters": denorm(adapters), "loop": denorm(loop)}))
mod("hermes_cli.mem_trim",
    trim_memory_when_idle=lambda reason=None: seam("trim_memory_when_idle", {"reason": reason}))
mod("agent.checkpoints.maintenance",
    auto_prune_from_config=lambda: seam("auto_prune_from_config", {}))
mod("hermes_cli.plugins_cadence",
    maybe_run_gateway_check=lambda log=None: seam("maybe_run_gateway_check", {"log": "logger"}))
mod("hermes_cli.profiles",
    profiles_to_serve=lambda multiplex, include_standalone=False, include_parked=False: seam(
        "profiles_to_serve", {"multiplex": multiplex,
                              "include_standalone": include_standalone,
                              "include_parked": include_parked}),
    profile_is_standalone=lambda home: seam("profile_is_standalone", {"home": denorm(home)}))
mod("hermes_cli.gateway_multiplex_mode", STANDALONE_DEPRECATION_NOTICE=NOTICE)


def decide_stub(our_home, *, replace=False):
    r = seam("decide", {"home": denorm(our_home), "replace": replace})
    if not isinstance(r, dict):
        return r
    d = dict(r)
    if isinstance(d.get("owner"), dict):
        od = AttrDouble(d["owner"])
        pid = d["owner"].get("pid")
        object.__setattr__(od, "describe", lambda: seam("owner.describe", {"pid": pid}))
        d["owner"] = od
    return AttrDouble(d)


mod("gateway.host_attach", ATTACH=ATTACH, REFUSE=REFUSE, REPLACE_HOST=REPLACE_HOST,
    decide=decide_stub)
mod("tools.process_registry_notifications",
    format_process_notification=lambda evt: seam("format_process_notification", {"evt": denorm(evt)}))


def install_case():
    CUR["idx"] = {}
    NS["logger"] = RecLogger()
    env = EnvDouble(CUR["in"].get("env") or {})
    NS["os"] = types.SimpleNamespace(
        environ=env,
        path=types.SimpleNamespace(expanduser=lambda p: seam("os.path.expanduser", {"path": p})))
    NS["asyncio"] = types.SimpleNamespace(sleep=asyncio_sleep_stub,
                                          CancelledError=asyncio.CancelledError)
    return env
FUNC_NAMES = {
    "_context_overflow_reply": "context_overflow_reply",
    "_rate_limit_reply": "rate_limit_reply",
    "_bridge_section_to_env": "bridge_section_to_env",
    "_bridge_max_turns_to_env": "bridge_max_turns_to_env",
    "_bridge_terminal_config_to_env": "bridge_terminal_config_to_env",
    "_bridge_auxiliary_config_to_env": "bridge_auxiliary_config_to_env",
    "_bridge_config_to_env": "bridge_config_to_env",
    "_load_bridge_config": "load_bridge_config",
    "_best_effort": "best_effort",
    "_runtime_agent_kwargs": "runtime_agent_kwargs",
    "_event_media_kind_is": "event_media_kind_is",
    "_format_gateway_process_notification": "format_gateway_process_notification",
    "_reconnect_attention_after_secs": "reconnect_attention_after_secs",
    "_write_runtime_status_quiet": "write_runtime_status_quiet",
    "_clear_takeover_marker_quiet": "clear_takeover_marker_quiet",
    "_housekeeping_chore": "housekeeping_chore",
    "_housekeeping_paste_sweep": "housekeeping_paste_sweep",
    "_housekeeping_misfire_catch_up": "housekeeping_misfire_catch_up",
    "_housekeeping_memory_trim": "housekeeping_memory_trim",
    "_housekeeping_checkpoint_prune": "housekeeping_checkpoint_prune",
    "_housekeeping_plugin_update_check": "housekeeping_plugin_update_check",
    "_launch_sessions_dir": "launch_sessions_dir",
    "_profile_sessions_dir": "profile_sessions_dir",
    "_guard_corrupt_user_config": "guard_corrupt_user_config",
    "_wait_for_pid_exit": "wait_for_pid_exit",
    "_resolve_handoff_watch_scopes": "resolve_handoff_watch_scopes",
    "_log_standalone_profiles_at_boot": "log_standalone_profiles_at_boot",
    "_host_attach_or_none": "host_attach_or_none",
}
BRIDGE_FUNCS = {"bridge_section_to_env", "bridge_max_turns_to_env",
                "bridge_terminal_config_to_env", "bridge_auxiliary_config_to_env",
                "bridge_config_to_env"}
FIXTURE_TO_LIVE = {v: k for k, v in FUNC_NAMES.items()}


def build_runner(inp):
    rd = RunnerDouble()
    for k, v in (inp.get("runner") or {}).items():
        if k == "config" and isinstance(v, dict):
            rd._d[k] = AttrDouble(v)
        else:
            rd._d[k] = v
    marker = rd._d.get("_run_in_executor_with_context")
    if isinstance(marker, dict) and marker:
        async def offload(fn, runner):
            return seam("run_in_executor_with_context",
                        {"fn": getattr(fn, "__name__", "fn"), "runner": denorm(runner)})
        object.__setattr__(rd, "_run_in_executor_with_context", offload)
    object.__setattr__(rd, "served_profile_names",
                       lambda: seam("served_profile_names", {}))
    return rd


def drive(func, inp):
    env = NS["os"].environ
    nostate = lambda: None
    envstate = lambda: {"env": dict(env._d)}
    live = FIXTURE_TO_LIVE.get(func)
    if live is None:
        raise ValueError("unknown func: %s" % func)
    fn = NS[live]
    state_fn = envstate if func in BRIDGE_FUNCS else nostate

    if func == "context_overflow_reply":
        return run_flow(lambda: fn(), nostate)
    if func == "rate_limit_reply":
        return run_flow(lambda: fn(inp.get("text", "")), nostate)
    if func == "bridge_section_to_env":
        return run_flow(lambda: fn(inp.get("section"), inp.get("mapping")), envstate)
    if func == "bridge_max_turns_to_env":
        return run_flow(lambda: fn(inp.get("agent_cfg")), envstate)
    if func == "bridge_terminal_config_to_env":
        return run_flow(lambda: fn(inp.get("terminal_cfg")), envstate)
    if func == "bridge_auxiliary_config_to_env":
        return run_flow(lambda: fn(inp.get("auxiliary_cfg")), envstate)
    if func == "bridge_config_to_env":
        return run_flow(lambda: fn(inp.get("cfg")), envstate)
    if func == "load_bridge_config":
        return run_flow(lambda: fn(PurePosixPath(inp.get("config_path", ""))), nostate)
    if func == "best_effort":
        cb = lambda: seam("fn", {})
        return run_flow(lambda: fn(cb, inp.get("debug_msg")), nostate)
    if func == "runtime_agent_kwargs":
        return run_flow(lambda: fn(inp.get("runtime")), nostate)
    if func == "event_media_kind_is":
        evd = inp.get("event")
        if isinstance(evd, dict):
            d = dict(evd)
            mt = d.get("message_type")
            if mt is not None:
                d["message_type"] = MessageType(mt)
            ev = AttrDouble(d)
        else:
            ev = evd
        fb = frozenset(MessageType(v) for v in (inp.get("fallback_types") or []))
        return run_flow(lambda: fn(ev, inp.get("index", 0), inp.get("mime_prefix", ""), fb),
                        nostate)
    if func == "format_gateway_process_notification":
        return run_flow(lambda: fn(inp.get("evt")), nostate)
    if func == "reconnect_attention_after_secs":
        return run_flow(lambda: fn(), nostate)
    if func == "write_runtime_status_quiet":
        return run_flow(lambda: fn(**(inp.get("fields") or {})), nostate)
    if func == "clear_takeover_marker_quiet":
        return run_flow(lambda: fn(), nostate)
    if func == "housekeeping_chore":
        cb = lambda *a, **k: seam("fn", {"args": list(a), "kwargs": k})
        return run_flow(lambda: fn(inp.get("label", ""), cb,
                                   *(inp.get("args") or []), **(inp.get("kwargs") or {})),
                        nostate)
    if func == "housekeeping_paste_sweep":
        return run_flow(lambda: fn(), nostate)
    if func == "housekeeping_misfire_catch_up":
        return run_flow(lambda: fn(inp.get("cron_provider"), inp.get("adapters"),
                                   inp.get("loop")), nostate)
    if func == "housekeeping_memory_trim":
        return run_flow(lambda: fn(), nostate)
    if func == "housekeeping_checkpoint_prune":
        return run_flow(lambda: fn(), nostate)
    if func == "housekeeping_plugin_update_check":
        return run_flow(lambda: fn(), nostate)
    if func == "launch_sessions_dir":
        cfg = inp.get("config")
        if isinstance(cfg, dict):
            cfg = AttrDouble(cfg)
        return run_flow(lambda: fn(cfg), nostate)
    if func == "profile_sessions_dir":
        launch = inp.get("launch")
        if isinstance(launch, list):
            launch = tuple(PurePosixPath(x) for x in launch)
        return run_flow(lambda: fn(launch), nostate)
    if func == "guard_corrupt_user_config":
        return run_flow(lambda: fn(), nostate)
    if func == "wait_for_pid_exit":
        return run_flow(lambda: run_coro(fn(inp.get("pid", 0), inp.get("attempts", 0),
                                            inp.get("delay", 0))), nostate)
    if func == "resolve_handoff_watch_scopes":
        rd = build_runner(inp)
        return run_flow(lambda: run_coro(fn(rd)), nostate)
    if func == "log_standalone_profiles_at_boot":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd), nostate)
    if func == "host_attach_or_none":
        return run_flow(lambda: run_coro(fn(inp.get("replace", False),
                                            force=inp.get("force", False))), nostate)
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
    print("ORACLE gateway_run_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())