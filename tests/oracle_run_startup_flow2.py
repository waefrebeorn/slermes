#!/usr/bin/env python3
"""Oracle for the gateway/run_startup.py UNCLAIMED remainder
(src/gateway/run_startup_flow2.c).

AST-extracts the 24 target functions (module function _own_policy_envs +
GatewayStartupMixin methods, decorators stripped) and the class/module
constants, seeds faithful seam recorders for every EFFECTFUL collaborator
(logging, config/registry/env reads, relay/hooks/plugins/profiles imports,
ledger leaves, faulthandler/signal/os/open, Path.resolve, the claimed-sibling
CMs _log_suppressed / _profile_runtime_scope and the claimed self-methods),
and binds the targets onto an AttrDouble runner. Ported targets run INLINE and
compose with each other REAL; claimed siblings are seamed. State is injected
as a JSON mirror; the oracle materializes the runner double, lets live mutate
it, then norms it back to JSON. Compares ordered seam trace + return + final
state. BaseException is caught per case; scripted raises round-trip bare
(identity-matched to the Raise instance).

Input: stdin JSON lines from t_port_run_startup_flow2.
Summary: ORACLE run_startup_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
import os
import posixpath
import sys
import types
from pathlib import PurePosixPath
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path as _RealPath
_REPO = _RealPath(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "rsu2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_startup.py")

# 24 UNCLAIMED targets.
MODULE_TARGETS = ["_own_policy_envs"]
STATIC_TARGETS = ["_log_agent_budget", "_release_runtime_claim_quiet",
                  "_register_config_hooks", "_register_launch_profile_config_hooks",
                  "_start_register_plugins_relay_hooks", "_start_flush_runtime_status",
                  "_handoff_home_chat_type"]
METHOD_TARGETS = ["_startup_retry_entry", "_startup_queue_transient_failure",
                  "_startup_teardown_adapter", "_resume_pending_candidates",
                  "_obligation_adapter", "_on_relay_provisioned",
                  "_start_relay_provision_retry",
                  "_recover_secondary_process_checkpoints",
                  "_start_log_retired_session_reset",
                  "_start_log_systemd_timing_alignment", "_open_faulthandler_log",
                  "_start_install_faulthandler", "_start_check_access_policy",
                  "_handoff_resolve_scope", "_start_aggregate_connect_results",
                  "_start_secondary_profiles"]
TARGETS = MODULE_TARGETS + STATIC_TARGETS + METHOD_TARGETS
ASYNC_TARGETS = {"_startup_teardown_adapter", "_obligation_adapter",
                 "_release_runtime_claim_quiet", "_start_flush_runtime_status",
                 "_handoff_home_chat_type", "_start_aggregate_connect_results",
                 "_start_secondary_profiles"}
# Sync target that calls asyncio.create_task -> drive inside a running loop.
LOOP_SYNC_TARGETS = {"_start_relay_provision_retry"}

STATE_DEFAULTS = {
    "_startup_parked_platforms": False,
    "_startup_ownership_conflict": False,
    "_platform_lock_takeover_on_start": True,
}
STATE_OBJ_DEFAULTS = ["config", "adapters", "_profile_adapters", "_failed_platforms"]
STATE_ARR_DEFAULTS = ["startup_retryable_errors", "startup_nonretryable_errors",
                      "multiplex_skipped_platforms"]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [],
       "self": None, "ns": None, "state": None}


def rec(ev):
    CUR["trace"].append(ev)


# ── exception construction (REAL builtins; BaseException trio passes) ────────
MultiplexConfigError = type("MultiplexConfigError", (Exception,), {})


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "MultiplexConfigError":
        return MultiplexConfigError(msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── script access (mirrors the harness stub semantics, fail-closed) ──────────
def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError",
                          "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam(name, args):
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


def recorder(name, argfn):
    def f(*a, **kw):
        return seam(name, argfn(a, kw))
    return f


def arecorder(name, argfn):
    async def f(*a, **kw):
        return seam(name, argfn(a, kw))
    return f


# ── Platform enum (builtin members; plugin pseudo-members out of domain) ─────
Platform = enum.Enum("Platform", {
    "LOCAL": "local", "TELEGRAM": "telegram", "DISCORD": "discord",
    "WHATSAPP": "whatsapp", "WHATSAPP_CLOUD": "whatsapp_cloud", "SLACK": "slack",
    "SIGNAL": "signal", "MATTERMOST": "mattermost", "MATRIX": "matrix",
    "EMAIL": "email", "SMS": "sms", "DINGTALK": "dingtalk",
    "API_SERVER": "api_server", "WEBHOOK": "webhook",
    "MSGRAPH_WEBHOOK": "msgraph_webhook", "FEISHU": "feishu", "WECOM": "wecom",
    "WECOM_CALLBACK": "wecom_callback", "WEIXIN": "weixin",
    "BLUEBUBBLES": "bluebubbles", "QQBOT": "qqbot", "RELAY": "relay",
})


# ── doubles ──────────────────────────────────────────────────────────────────
class AttrDouble:
    """Attribute reads on absent keys raise the C-port's AttrDouble message."""
    def __init__(self, d):
        object.__setattr__(self, "_d", d)   # shared reference (mutation flows)

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class PathDouble(PurePosixPath):
    def resolve(self):
        return seam("Path.resolve", {"path": str(self)})


class SignalDouble:
    def __getattr__(self, name):
        if name == "SIGUSR2":
            return seam("signal.SIGUSR2", {})
        raise AttributeError("missing attribute '%s'" % name)


class _AbstractCM:
    def __enter__(self):
        return None

    def __exit__(self, *a):
        return False


class _LogSuppressedCM:
    """Claimed-sibling CM: factory seamed; except-Exception semantics twin."""
    def __init__(self, level, msg, args, exc_info):
        self.level, self.msg, self.args, self.exc_info = level, msg, args, exc_info

    def __enter__(self):
        return None

    def __exit__(self, et, ev, tb):
        if et is None:
            return False
        if not issubclass(et, Exception):
            return False   # BaseException passes unlogged
        a = {"fmt": self.msg, "level": self.level}
        args = [proj(x) for x in self.args]
        if not self.exc_info:
            args = args + [{"__raise__": et.__name__, "message": str(ev)}]
        if args:
            a["args"] = args
        if self.exc_info:
            a["exc_info"] = True
        seam("logger.log", a)
        return True


def _log_suppressed_factory(level, msg, *args, exc_info=False):
    a = {"level": level, "fmt": msg}
    if args:
        a["args"] = [proj(x) for x in args]
    if exc_info:
        a["exc_info"] = True
    seam("log_suppressed", a)
    return _LogSuppressedCM(level, msg, args, exc_info)


def _scope_factory(*a):
    args = {"home": proj(a[0])}
    if len(a) > 1:
        args["env"] = proj(a[1])
    seam("run._profile_runtime_scope", args)
    return _AbstractCM()


def proj_key(k):
    return k.value if isinstance(k, Platform) else k


def proj(x):
    if isinstance(x, Platform):
        return x.value
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "message": str(x)}
    if isinstance(x, types.MethodType):
        return {"__callable__": x.__func__.__name__}
    if isinstance(x, PathDouble):
        return str(x)
    if isinstance(x, AttrDouble):
        d = object.__getattribute__(x, "_d")
        return {proj_key(k): proj(v) for k, v in d.items()}
    if isinstance(x, SimpleNamespace):
        return {k: proj(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {proj_key(k): proj(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [proj(v) for v in x]
    if isinstance(x, type):
        return x.__name__
    return x


class RecLogger:
    def _rec(self, name, level, fmt, a, k):
        args = {"fmt": fmt}
        if level is not None:
            args["level"] = level
        if a:
            args["args"] = [proj(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return seam(name, args)

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


# ── module stubs for in-function imports ─────────────────────────────────────
def _stub_pkg(parent, child, mod):
    if parent not in sys.modules or not isinstance(sys.modules[parent], types.ModuleType):
        p = types.ModuleType(parent)
        p.__path__ = []
        sys.modules[parent] = p
    sys.modules[parent + "." + child] = mod
    setattr(sys.modules[parent], child, mod)


def _install_module_stubs(ns):
    cfgmod = types.ModuleType("hermes_cli.config")
    cfgmod.TURN_LIMIT_UNLIMITED = sys.maxsize
    cfgmod.resolve_turn_limit = recorder("config.resolve_turn_limit",
                                         lambda a, k: {"value": proj(a[0])})
    cfgmod.load_config = recorder("config.load_config", lambda a, k: {})
    _stub_pkg("hermes_cli", "config", cfgmod)

    envmod = types.ModuleType("gateway.platform_env")
    envmod.authz_field = recorder("platform_env.authz_field",
        lambda a, k: {"value": a[0], "field": a[1], "entry": proj(a[2])})
    _stub_pkg("gateway", "platform_env", envmod)

    regmod = types.ModuleType("gateway.platform_registry")
    regmod.platform_registry = SimpleNamespace(
        get=recorder("platform_registry.get", lambda a, k: {"value": a[0]}),
        loaded_entries=recorder("platform_registry.loaded_entries", lambda a, k: {}))
    _stub_pkg("gateway", "platform_registry", regmod)

    runmod = types.ModuleType("gateway.run")
    runmod._own_policy_open_startup_violation = recorder(
        "run._own_policy_open_startup_violation", lambda a, k: {"config": proj(a[0])})
    runmod._write_runtime_status_quiet = recorder("run._write_runtime_status_quiet",
        lambda a, k: {"gateway_state": k.get("gateway_state"),
                      "exit_reason": k.get("exit_reason")})
    runmod._profile_runtime_scope = _scope_factory
    runmod.get_hermes_home = recorder("run.get_hermes_home", lambda a, k: {})
    runmod._multiplex_profile_homes = recorder("run._multiplex_profile_homes",
                                               lambda a, k: {})
    runmod.load_gateway_config = recorder("run.load_gateway_config", lambda a, k: {})
    runmod.MultiplexConfigError = MultiplexConfigError
    _stub_pkg("gateway", "run", runmod)

    rdelmod = types.ModuleType("gateway.run_restored_delivery")
    rdelmod.resume_pending_candidates = recorder(
        "run_restored_delivery.resume_pending_candidates",
        lambda a, k: {"platform": proj(a[1])})
    _stub_pkg("gateway", "run_restored_delivery", rdelmod)

    ledmod = types.ModuleType("gateway.delivery_ledger")
    ledmod.release_runtime_claim = recorder("delivery_ledger.release_runtime_claim",
        lambda a, k: {"obligation_id": proj(a[0]), "error": proj(a[1])})
    ledmod.is_flood_error = recorder("delivery_ledger.is_flood_error",
        lambda a, k: {"last_error": proj(a[0])})
    _stub_pkg("gateway", "delivery_ledger", ledmod)

    relaymod = types.ModuleType("gateway.relay")
    relaymod.relay_provision_pending = recorder("relay.relay_provision_pending",
                                                lambda a, k: {})
    relaymod.retry_self_provision = recorder("relay.retry_self_provision",
        lambda a, k: {"callback": proj(a[0])})
    relaymod.register_relay_adapter = recorder("relay.register_relay_adapter",
                                               lambda a, k: {})
    relaymod.relay_url = recorder("relay.relay_url", lambda a, k: {})
    relaymod.self_provision_relay = recorder("relay.self_provision_relay",
                                             lambda a, k: {})
    relaymod.send_relay_policy = recorder("relay.send_relay_policy", lambda a, k: {})
    _stub_pkg("gateway", "relay", relaymod)

    plmod = types.ModuleType("hermes_cli.plugins")
    plmod.discover_plugins = recorder("plugins.discover_plugins", lambda a, k: {})
    _stub_pkg("hermes_cli", "plugins", plmod)
    plrem = types.ModuleType("hermes_cli.plugins_cmd_remove")
    plrem.sweep_pending_plugin_deletes = recorder("plugins.sweep_pending_plugin_deletes",
                                                  lambda a, k: {})
    sys.modules["hermes_cli.plugins_cmd_remove"] = plrem
    sys.modules["hermes_cli"].plugins_cmd_remove = plrem

    secmod = types.ModuleType("agent.secret_scope")
    secmod.is_multiplex_active = recorder("secret_scope.is_multiplex_active",
                                          lambda a, k: {})
    _stub_pkg("agent", "secret_scope", secmod)

    hcmod = types.ModuleType("hermes_constants")
    hcmod.get_process_hermes_home = recorder("hermes_constants.get_process_hermes_home",
                                             lambda a, k: {})

    def _ghh():
        r = seam("hermes_constants.get_hermes_home", {})
        return PathDouble(r) if isinstance(r, str) else r
    hcmod.get_hermes_home = _ghh
    sys.modules["hermes_constants"] = hcmod

    shmod = types.ModuleType("agent.shell_hooks")
    shmod.register_from_config = recorder("shell_hooks.register_from_config",
        lambda a, k: {"config": proj(a[0]), "accept_hooks": k.get("accept_hooks")})
    _stub_pkg("agent", "shell_hooks", shmod)
    owmod = types.ModuleType("agent.outbound_webhooks")
    owmod.register_from_config = recorder("outbound_webhooks.register_from_config",
        lambda a, k: {"config": proj(a[0])})
    sys.modules["agent.outbound_webhooks"] = owmod
    sys.modules["agent"].outbound_webhooks = owmod

    clmod = types.ModuleType("gateway.config_loader")
    clmod.read_yaml_layers = recorder("config_loader.read_yaml_layers",
        lambda a, k: {"home": proj(a[0])})
    _stub_pkg("gateway", "config_loader", clmod)

    prmod = types.ModuleType("hermes_cli.profiles")
    prmod.profiles_to_serve = recorder("profiles.profiles_to_serve",
        lambda a, k: {"multiplex": a[0]})
    sys.modules["hermes_cli.profiles"] = prmod
    sys.modules["hermes_cli"].profiles = prmod

    srmod = types.ModuleType("hermes_cli.session_reset_retirement")
    srmod.format_notice = recorder("session_reset_retirement.format_notice",
        lambda a, k: {"path": proj(a[0]), "mode": proj(a[1])})
    srmod.reset_plugin_enabled = recorder("session_reset_retirement.reset_plugin_enabled",
                                          lambda a, k: {})
    srmod.retired_reset_policy = recorder("session_reset_retirement.retired_reset_policy",
        lambda a, k: {"layers": proj(a[0])})
    sys.modules["hermes_cli.session_reset_retirement"] = srmod
    sys.modules["hermes_cli"].session_reset_retirement = srmod

    sfmod = types.ModuleType("gateway.shutdown_forensics")
    sfmod.check_systemd_timing_alignment = recorder(
        "shutdown_forensics.check_systemd_timing_alignment",
        lambda a, k: {"drain_timeout": proj(a[0]), "cron_drain_timeout": proj(a[1])})
    _stub_pkg("gateway", "shutdown_forensics", sfmod)

    stmod = types.ModuleType("gateway.status")
    stmod.flush_runtime_status_async = arecorder("status.flush_runtime_status_async",
        lambda a, k: {"timeout": k.get("timeout")})
    _stub_pkg("gateway", "status", stmod)

    prgmod = types.ModuleType("tools.process_registry")
    prgmod.process_registry = SimpleNamespace(
        recover_from_checkpoint=recorder("process_registry.recover_from_checkpoint",
                                         lambda a, k: {}),
        restore_completions=recorder("process_registry.restore_completions",
                                     lambda a, k: {}))
    _stub_pkg("tools", "process_registry", prgmod)


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("run_startup_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    logging_double = SimpleNamespace(DEBUG=10, INFO=20, WARNING=30, ERROR=40,
                                     getLogger=lambda name: RecLogger())
    asyncio_double = SimpleNamespace(
        create_task=recorder("asyncio.create_task",
            lambda a, k: {"coro": proj(a[0]), "name": k.get("name")}),
        CancelledError=asyncio.CancelledError)

    async def _to_thread(fn, *a, **k):
        return fn(*a, **k)
    asyncio_double.to_thread = _to_thread

    faulthandler_double = SimpleNamespace(
        enable=recorder("faulthandler.enable",
            lambda a, k: {**({"file": proj(k["file"])} if "file" in k else {}),
                          **({"all_threads": k["all_threads"]} if "all_threads" in k else {})}),
        register=recorder("faulthandler.register",
            lambda a, k: {"sigusr2": proj(a[0]), "file": proj(k.get("file")),
                          "all_threads": k.get("all_threads"), "chain": k.get("chain")}))
    ns.update({
        "json": json, "contextlib": contextlib, "copy": copy,
        "suppress": contextlib.suppress, "nullcontext": contextlib.nullcontext,
        "asyncio": asyncio_double, "dataclasses": __import__("dataclasses"),
        "faulthandler": faulthandler_double, "logging": logging_double,
        "os": SimpleNamespace(path=posixpath,
                              makedirs=recorder("os.makedirs",
                                  lambda a, k: {"path": proj(a[0]),
                                                "exist_ok": k.get("exist_ok")}),
                              getenv=recorder("os.getenv",
                                  lambda a, k: {"name": a[0],
                                                **({"default": a[1]} if len(a) > 1 else {})})),
        "signal": SignalDouble(), "time": __import__("time"),
        "open": recorder("open", lambda a, k: {"path": proj(a[0]), "mode": a[1],
                                               "encoding": k.get("encoding")}),
        "Path": PathDouble, "Platform": Platform,
        "Optional": object, "Any": object, "Dict": object, "Tuple": object,
        "DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT": 30.0,
        "GATEWAY_FATAL_CONFIG_EXIT_CODE": 78,
        "is_global_startup_conflict": recorder("restart.is_global_startup_conflict",
            lambda a, k: {"code": proj(a[0])}),
        "_log_suppressed": _log_suppressed_factory,
        "_send_error": recorder("_send_error", lambda a, k: {"result": proj(a[0])}),
        "looks_like_telegram_private_chat_id": recorder(
            "looks_like_telegram_private_chat_id", lambda a, k: {"chat_id": proj(a[0])}),
        "prime_revived_egress": recorder("prime_revived_egress", lambda a, k: {}),
        "BasePlatformAdapter": type("BasePlatformAdapter", (), {}),
        "MessageEvent": type("MessageEvent", (), {}),
        "MessageType": SimpleNamespace(TEXT="text"),
        "SessionSource": type("SessionSource", (), {}),
        "build_session_key": recorder("build_session_key", lambda a, k: {}),
        "t": recorder("t", lambda a, k: {}),
        "note_running_chat_owner": recorder("note_running_chat_owner", lambda a, k: {}),
        "DEFAULT_HEARTBEAT_INTERVAL_S": 30.0,
        "loop_heartbeat_forever": recorder("loop_heartbeat_forever", lambda a, k: {}),
        "copy_context": __import__("contextvars").copy_context,
        "logger": RecLogger(),
    })
    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag
    # module-level plain-Name assigns (logger, _HANDOFF_SHARED_CHAT_TYPES)
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # module function targets
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in MODULE_TARGETS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    # class constants + method targets (decorators stripped)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayStartupMixin":
            for m in node.body:
                if isinstance(m, ast.Assign) and all(isinstance(t, ast.Name) for t in m.targets):
                    exec(compile(ast.Module(body=[m], type_ignores=[]), SRC, "exec"), ns)
                elif isinstance(m, ast.AnnAssign) and m.value and isinstance(m.target, ast.Name):
                    exec(compile(ast.Module(body=[m], type_ignores=[]), SRC, "exec"), ns)
                elif isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                        m.name in STATIC_TARGETS + METHOD_TARGETS:
                    m2 = copy.copy(m)
                    m2.decorator_list = []
                    exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec",
                                 flags=flags), ns)
    for name in TARGETS:
        assert name in ns, "extraction missed %s" % name
    for name in ("_ALLOWLIST_ENV_PLATFORMS", "_BUILTIN_ALLOWED_USERS_VARS",
                 "_BUILTIN_ALLOW_ALL_VARS", "_HANDOFF_SHARED_CHAT_TYPES"):
        assert name in ns, "constant missed %s" % name
    _install_module_stubs(ns)
    ns["logger"] = RecLogger()
    # synthetic class handle for GatewayStartupMixin.<static> compositions
    gsm = type("GatewayStartupMixin", (), {})
    for nm in STATIC_TARGETS:
        setattr(gsm, nm, staticmethod(ns[nm]))
    ns["GatewayStartupMixin"] = gsm
    CUR["ns"] = ns
    return ns


def _mat_adapter(d):
    if not isinstance(d, dict):
        return d
    return AttrDouble(d)


def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    state = copy.deepcopy(inp.get("state") or {})
    for k, v in STATE_DEFAULTS.items():
        state.setdefault(k, v)
    for k in STATE_OBJ_DEFAULTS:
        if not isinstance(state.get(k), dict):
            state[k] = {}
    for k in STATE_ARR_DEFAULTS:
        if not isinstance(state.get(k), list):
            state[k] = []
    CUR["state"] = state

    sd = AttrDouble(state)
    # config double (platforms keyed by Platform, cfg attribute doubles)
    cfg = state["config"]
    platforms = cfg.get("platforms")
    if isinstance(platforms, dict):
        cfg["platforms"] = {Platform(k): _mat_adapter(v) for k, v in platforms.items()}
    object.__setattr__(sd, "config", AttrDouble(cfg))
    object.__setattr__(sd, "adapters",
                       {Platform(k): _mat_adapter(v) for k, v in state["adapters"].items()})
    object.__setattr__(sd, "_profile_adapters",
                       {n: {Platform(k): _mat_adapter(v) for k, v in m.items()}
                        for n, m in state["_profile_adapters"].items()})
    object.__setattr__(sd, "_failed_platforms",
                       {Platform(k): v for k, v in state["_failed_platforms"].items()})
    # bind ported targets
    for nm in METHOD_TARGETS:
        object.__setattr__(sd, nm, types.MethodType(ns[nm], sd))
    for nm in STATIC_TARGETS:
        object.__setattr__(sd, nm, ns[nm])
    # class-level constants read through self
    for nm in ("_ALLOWLIST_ENV_PLATFORMS", "_BUILTIN_ALLOWED_USERS_VARS",
               "_BUILTIN_ALLOW_ALL_VARS"):
        object.__setattr__(sd, nm, ns[nm])
    # claimed self-method siblings -> seams
    object.__setattr__(sd, "_safe_adapter_disconnect", arecorder(
        "self._safe_adapter_disconnect",
        lambda a, k: {"adapter": proj(a[0]), "platform": proj(a[1])}))
    object.__setattr__(sd, "_authorization_adapter", recorder(
        "self._authorization_adapter",
        lambda a, k: {"platform": proj(a[0]), "profile": proj(a[1])}))
    object.__setattr__(sd, "_reconnect_queue_entry", recorder(
        "self._reconnect_queue_entry",
        lambda a, k: {"platform": proj(a[0]), "adapter": proj(a[1]),
                      "platform_config": proj(a[2]), "attempts": k.get("attempts"),
                      "delay": k.get("delay"), "queued": k.get("queued")}))
    object.__setattr__(sd, "_update_platform_runtime_status", recorder(
        "self._update_platform_runtime_status",
        lambda a, k: {"value": a[0], "platform_state": k.get("platform_state"),
                      "error_code": k.get("error_code"),
                      "error_message": k.get("error_message")}))
    object.__setattr__(sd, "_request_clean_exit", recorder(
        "self._request_clean_exit", lambda a, k: {"reason": proj(a[0])}))
    object.__setattr__(sd, "_retain_background_task", recorder(
        "self._retain_background_task", lambda a, k: {"task": proj(a[0])}))
    object.__setattr__(sd, "_publish_primary_adapter", recorder(
        "self._publish_primary_adapter",
        lambda a, k: {"platform": proj(a[0]), "adapter": proj(a[1])}))
    object.__setattr__(sd, "_startup_fail_fatal_config", recorder(
        "self._startup_fail_fatal_config", lambda a, k: {"reason": proj(a[0])}))
    object.__setattr__(sd, "_start_secondary_profile_adapters", arecorder(
        "self._start_secondary_profile_adapters", lambda a, k: {}))
    object.__setattr__(sd, "_unserved_shared_ingress_warnings", recorder(
        "self._unserved_shared_ingress_warnings", lambda a, k: {}))
    # faulthandler register presence flag (no trace event on either side)
    fh = ns["faulthandler"]
    if inp.get("faulthandler_has_register", True):
        if not hasattr(fh, "register"):
            fh.register = recorder("faulthandler.register",
                lambda a, k: {"sigusr2": proj(a[0]), "file": proj(k.get("file")),
                              "all_threads": k.get("all_threads"), "chain": k.get("chain")})
    else:
        if hasattr(fh, "register"):
            del fh.register
    CUR["self"] = sd


def norm_state():
    sd = CUR["self"]
    state = CUR["state"]
    out = {
        "config": proj(sd.config),
        "adapters": {k.value: proj(v) for k, v in sd.adapters.items()},
        "_profile_adapters": {n: {k.value: proj(v) for k, v in m.items()}
                              for n, m in sd._profile_adapters.items()},
        "_failed_platforms": {k.value: proj(v) for k, v in sd._failed_platforms.items()},
        "_startup_parked_platforms": sd._startup_parked_platforms,
        "_startup_ownership_conflict": sd._startup_ownership_conflict,
        "_platform_lock_takeover_on_start": sd._platform_lock_takeover_on_start,
        "startup_retryable_errors": state["startup_retryable_errors"],
        "startup_nonretryable_errors": state["startup_nonretryable_errors"],
        "multiplex_skipped_platforms": state["multiplex_skipped_platforms"],
    }
    for k in ("_restart_drain_timeout", "_cron_drain_timeout"):
        if k in state:
            out[k] = state[k]
    return out


def _mk_adapter(inp):
    d = inp.get("adapter")
    ad = _mat_adapter(d) if isinstance(d, dict) else d
    return ad


def _mk_teardown_adapter(inp):
    ad = _mk_adapter(inp)
    if isinstance(ad, AttrDouble):
        object.__setattr__(ad, "cancel_background_tasks", arecorder(
            "adapter.cancel_background_tasks", lambda a, k: {}))
    return ad


def _mk_chat_adapter(inp):
    ad = _mk_adapter(inp)
    if isinstance(ad, AttrDouble):
        object.__setattr__(ad, "get_chat_info", arecorder(
            "adapter.get_chat_info", lambda a, k: {"home_chat_id": a[0]}))
    return ad


def _mk_raw(raw):
    out = []
    for item in raw:
        if isinstance(item, dict) and "__raise__" in item:
            out.append(make_exc(item))
        elif isinstance(item, list):
            p, ad, pc, outcome, exc = item
            ad2 = _mat_adapter(ad) if isinstance(ad, dict) else ad
            pc2 = _mat_adapter(pc) if isinstance(pc, dict) else pc
            exc2 = make_exc(exc) if isinstance(exc, dict) and "__raise__" in exc else exc
            out.append([Platform(p), ad2, pc2, outcome, exc2])
        else:
            out.append(item)
    return out


def drive(ns, func, inp):
    sd = CUR["self"]
    g = inp.get
    if func == "_own_policy_envs":
        return ns["_own_policy_envs"](Platform(g("platform")))
    if func == "_log_agent_budget":
        return ns["_log_agent_budget"]()
    if func == "_startup_retry_entry":
        return sd._startup_retry_entry(Platform(g("platform")),
                                       _mk_adapter(inp), _mat_adapter(g("platform_config")),
                                       queued=g("queued", True))
    if func == "_startup_queue_transient_failure":
        return sd._startup_queue_transient_failure(
            Platform(g("platform")), _mk_adapter(inp), _mat_adapter(g("platform_config")),
            g("message", ""), CUR["state"]["startup_retryable_errors"])
    if func == "_startup_teardown_adapter":
        return asyncio.run(sd._startup_teardown_adapter(_mk_teardown_adapter(inp),
                                                        Platform(g("platform"))))
    if func == "_resume_pending_candidates":
        p = g("platform")
        return sd._resume_pending_candidates(Platform(p) if p is not None else None)
    if func == "_release_runtime_claim_quiet":
        return asyncio.run(ns["_release_runtime_claim_quiet"](
            g("obligation_id"), g("log_fmt", ""), error=g("error", "send_path_degraded")))
    if func == "_obligation_adapter":
        return asyncio.run(sd._obligation_adapter(g("row")))
    if func == "_on_relay_provisioned":
        return sd._on_relay_provisioned()
    if func == "_start_relay_provision_retry":
        async def _in_loop():
            return sd._start_relay_provision_retry()
        return asyncio.run(_in_loop())
    if func == "_register_config_hooks":
        return ns["_register_config_hooks"](g("fail_fmt", ""), *g("fail_args", []),
                                            level=g("level", 10))
    if func == "_register_launch_profile_config_hooks":
        return ns["_register_launch_profile_config_hooks"]()
    if func == "_start_register_plugins_relay_hooks":
        return ns["_start_register_plugins_relay_hooks"]()
    if func == "_recover_secondary_process_checkpoints":
        pr = SimpleNamespace(recover_from_checkpoint=recorder(
            "process_registry.recover_from_checkpoint", lambda a, k: {}))
        return sd._recover_secondary_process_checkpoints(pr)
    if func == "_start_log_retired_session_reset":
        return sd._start_log_retired_session_reset()
    if func == "_start_log_systemd_timing_alignment":
        return sd._start_log_systemd_timing_alignment()
    if func == "_open_faulthandler_log":
        return sd._open_faulthandler_log()
    if func == "_start_install_faulthandler":
        return sd._start_install_faulthandler()
    if func == "_start_flush_runtime_status":
        return asyncio.run(ns["_start_flush_runtime_status"]())
    if func == "_start_check_access_policy":
        return sd._start_check_access_policy()
    if func == "_handoff_resolve_scope":
        return sd._handoff_resolve_scope(g("profile_name"))
    if func == "_handoff_home_chat_type":
        return asyncio.run(ns["_handoff_home_chat_type"](
            _mk_chat_adapter(inp), g("home_chat_id", ""), Platform(g("platform"))))
    if func == "_start_aggregate_connect_results":
        return asyncio.run(sd._start_aggregate_connect_results(
            _mk_raw(g("raw") or []), CUR["state"]["startup_retryable_errors"],
            CUR["state"]["startup_nonretryable_errors"]))
    if func == "_start_secondary_profiles":
        skipped = [Platform(s) for s in CUR["state"]["multiplex_skipped_platforms"]]
        return asyncio.run(sd._start_secondary_profiles(g("connected_count", 0), skipped))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    setup_case(ns, func, inp)
    try:
        ret = drive(ns, func, inp)
        want_ret = proj(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state()}


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
    ns = extract()
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
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_startup_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())