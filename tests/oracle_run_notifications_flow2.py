#!/usr/bin/env python3
"""Oracle for the gateway/run_notifications.py UNCLAIMED-remainder FLOW cores
(src/gateway/run_notifications_flow2.c).

AST-extracts the LIVE GatewayNotificationsMixin methods (+ the nested
_CompletionClaim dataclass and module constants), seeds the real _log_suppressed
from gateway/run_shutdown.py, and replaces every live I/O dependency with
recording stubs driven by the SAME per-name scripts the C harness feeds its
universal seams (in.io). Compares the ordered seam-call trace plus the return
value and the mutated state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text} raises a REAL exception of that kind; a scripted
null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for <name>"
(recorded as ret, then raised). BaseException kinds pass through every
`except Exception` / _log_suppressed. Ported siblings run inline (real) on both
sides; claimed / foreign siblings are seams.

Input: stdin JSON lines from t_port_run_notifications_flow2.
Summary: ORACLE run_notifications_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
import typing
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
SRC = os.path.join(QUARRY, "gateway", "run_notifications.py")
SHUTDOWN_SRC = os.path.join(QUARRY, "gateway", "run_shutdown.py")

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def deepcopy(x):
    return copy.deepcopy(x)


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
    rec({"ev": name, "args": deepcopy(args), "ret": deepcopy(val)})
    return val


BUILTIN_EXC = {
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
    "KeyError": KeyError, "LookupError": LookupError, "IndexError": IndexError,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError, "RuntimeError": RuntimeError,
    "TimeoutError": TimeoutError, "ImportError": ImportError, "OverflowError": OverflowError,
    "Exception": Exception,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    cls = BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        return cls(msg)   # str(KeyError(msg)) == repr(msg); msg already repr'd
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

    def exception(self, fmt, *a, **k):
        return self._rec("logger.error", None, fmt, a, k)

    def log(self, level, fmt, *a, **k):
        return self._rec("logger.log", level, fmt, a, k)


class Platform:
    def __init__(self, value):
        if value not in PLATFORM_VALUES:
            raise ValueError("%r is not a valid Platform" % value)
        self.value = value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, Platform) and self.value == other.value

    def __repr__(self):
        return "Platform(%r)" % self.value


PLATFORM_VALUES = [
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin",
    "bluebubbles", "qqbot", "relay",
]
for _pv in PLATFORM_VALUES:
    setattr(Platform, _pv.upper(), Platform(_pv))


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)

    def __setattr__(self, name, val):
        object.__getattribute__(self, "_d")[name] = val


class AdapterDouble:
    """Adapter double: PRESENT hook keys model callable methods; data fields read."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name not in d:
            raise AttributeError("missing attribute '%s'" % name)
        if name == "send":
            async def method(*a, **k):
                return materialize(script("adapter.send", {
                    "chat_id": a[0], "message": a[1], "metadata": k.get("metadata")}))
            return method
        if name == "_ensure_store":
            def method(*a, **k):
                return materialize(script("adapter.ensure_store", {}))
            return method
        return d[name]


class SessionStoreDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))
        ents = d.get("_entries") or {}
        object.__setattr__(self, "_entries",
                           {k: (AttrDouble(v) if isinstance(v, dict) else v) for k, v in ents.items()})

    def _ensure_loaded(self):
        return materialize(script("session_store.ensure_loaded", {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class ScopeDouble:
    def __init__(self, home):
        self.home = home

    def __enter__(self):
        materialize(script("profile_scope_enter", {"home": self.home}))
        return self

    def __exit__(self, *exc):
        materialize(script("profile_scope_exit", {"home": self.home}))
        return False


def mat_adapter(x):
    return AdapterDouble(x) if isinstance(x, dict) else x


def mat(name, v):
    if v is None:
        return None
    if name in ("self._build_process_event_source", "identity_of"):
        return types.SimpleNamespace(**v) if isinstance(v, dict) else v
    if name == "self._transport_owner":
        return tuple(mat_adapter(x) for x in v) if isinstance(v, list) else v
    if name in ("self._delivery_adapter_for", "self._routed_primary_adapter",
                "self._committed_relay_adapter"):
        return mat_adapter(v)
    if name == "self._adapters_for_profile":
        return {Platform(k): mat_adapter(x) for k, x in v.items()} if isinstance(v, dict) else v
    if name == "resolve_delivery_transport":
        if isinstance(v, dict):
            d = dict(v)
            if isinstance(d.get("adapter"), dict):
                d["adapter"] = AdapterDouble(d["adapter"])
            return types.SimpleNamespace(**d)
        return v
    if name == "self._lookup_completion_owner":
        if isinstance(v, list) and len(v) >= 3:
            parent = types.SimpleNamespace(**v[2]) if isinstance(v[2], dict) else v[2]
            return [v[0], v[1], parent] + v[3:]
        return v
    return v


class SeamFn:
    def __init__(self, name, argmap, async_=False):
        self.name = name
        self.argmap = argmap
        self.async_ = async_

    def __call__(self, *a, **k):
        args = self.argmap(a, k)
        v = mat(self.name, materialize(script(self.name, args)))
        return v

    async def acall(self, *a, **k):
        return self.__call__(*a, **k)


def seam(name, argmap, async_=False):
    fn = SeamFn(name, argmap, async_)
    if async_:
        async def wrapper(*a, **k):
            return fn(*a, **k)
        return wrapper
    return fn


def evt_args(a, k):
    return {"evt": deepcopy(a[0])}


ARGMAPS = {
    "self._build_process_event_source": evt_args,
    "self._settle_durable_claim": lambda a, k: {"kind": a[0], "delegation_id": a[1], "claim_id": a[2]},
    "self._transport_owner": lambda a, k: {"source": denorm(a[0])},
    "self._delivery_adapter_for": lambda a, k: {"source": denorm(a[0])},
    "self._restored_transport_trusted": lambda a, k: {"source": denorm(a[0]), "identity": denorm(a[1])},
    "self._adapters_for_profile": lambda a, k: {"profile": a[0]},
    "self._routed_primary_adapter": lambda a, k: {"source": denorm(a[0])},
    "self._committed_relay_adapter": lambda a, k: {"source": denorm(a[0])},
    "self._lookup_completion_owner": lambda a, k: {"parent_session_id": a[0], "session_id": a[1]},
    "self._closed_local_target": lambda a, k: {"aio": denorm(a[0]), "entry": denorm(a[1]),
                                               "parent_session_id": a[2], "parent": denorm(a[3])},
    "self._resolve_compression_lineage_target": lambda a, k: {"aio": denorm(a[0]), "entry": denorm(a[1]),
                                                              "parent_session_id": a[2]},
    "identity_of": lambda a, k: {"source": denorm(a[0])},
    "detached_route_of": lambda a, k: {"source": denorm(a[0])},
    "resolve_delivery_transport": lambda a, k: {"platform": a[0].value, "config": denorm(a[1]),
                                                "adapters": denorm(a[2])},
    "_raw_process_event_session_id": evt_args,
    "_held_reason": lambda a, k: {"source": denorm(a[0]), "platform": a[1], "multiplexed": a[2]},
    "_safe_delivery_transport": lambda a, k: {"platform": a[0].value, "config": denorm(a[1]),
                                              "adapters": denorm(a[2]), "profile": k.get("profile")},
    "adapter_supports_push": lambda a, k: {"adapter": denorm(a[0])},
    "session_owned_by_profile": lambda a, k: {"config": denorm(a[0]), "profile": a[1], "raw_sid": a[2]},
    "_multiplex_profile_homes": lambda a, k: {"config": denorm(a[0])},
    "strip_ansi": lambda a, k: {"text": a[0]},
    "transform_process_output": lambda a, k: dict({"text": a[0]}, **k),
    "redact_terminal_output": lambda a, k: {"text": a[0], "command": a[1]},
    "_redact_gateway_user_facing_secrets": lambda a, k: {"text": a[0]},
    "_format_concise_process_notification": lambda a, k: dict(
        {"session_id": a[0], "command": a[1], "exit_code": a[2], "output": a[3]}, **k),
    "_shorten_command_for_display": lambda a, k: {"command": a[0]},
    "_non_conversational_metadata": lambda a, k: dict({"metadata": denorm(a[0])}, **k),
    "t": lambda a, k: dict({"key": a[0]}, **k),
    "claim_completion_delivery": lambda a, k: {"delegation_id": a[0], "claim_id": a[1]},
    "restore_undelivered_completions": lambda a, k: {"queue": a[0].id},
    "sweep_orphaned_completions": lambda a, k: {"queue": a[0].id},
    "time.time": lambda a, k: {},
    "self_id": lambda a, k: {},
    "uuid4_hex": lambda a, k: {},
    "ledger.fn": lambda a, k: {},
}
ASYNC_SEAMS = {"self._lookup_completion_owner", "self._closed_local_target",
               "self._resolve_compression_lineage_target"}

# self-method seams (instance attributes; no self recorded)
SELF_SEAMS = {
    "_build_process_event_source": "self._build_process_event_source",
    "_settle_durable_claim": "self._settle_durable_claim",
    "_transport_owner": "self._transport_owner",
    "_delivery_adapter_for": "self._delivery_adapter_for",
    "_restored_transport_trusted": "self._restored_transport_trusted",
    "_adapters_for_profile": "self._adapters_for_profile",
    "_routed_primary_adapter": "self._routed_primary_adapter",
    "_committed_relay_adapter": "self._committed_relay_adapter",
    "_lookup_completion_owner": "self._lookup_completion_owner",
    "_closed_local_target": "self._closed_local_target",
    "_resolve_compression_lineage_target": "self._resolve_compression_lineage_target",
}

METHODS = (
    "_redacted_output_tail", "_build_process_completion_event",
    "_format_process_final_message", "_format_process_running_message",
    "_format_coalesced_process_completions", "_launching_turn_active",
    "_send_watcher_message", "_each_secondary_ledger",
    "_restore_secondary_completion_ledgers", "_sweep_orphaned_completion_ledgers",
    "_classify_completion_target", "_classify_completion_owner",
    "_completion_delivery_ready", "_preflight_completion_delivery",
    "_resolve_injection_adapter", "_served_api_server_wake_profile",
    "_adapter_by_platform_value", "_home_channel_transports",
    "_served_home_channel_configs", "_served_home_channel_transports",
)
STATIC_METHODS = {"_redacted_output_tail", "_build_process_completion_event",
                  "_format_coalesced_process_completions"}
PORTED = set(METHODS)


def extract():
    src = open(SRC, encoding="utf-8").read()
    modname = "rn2_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["__file__"] = SRC
    ns["asyncio"] = asyncio
    ns["contextlib"] = contextlib
    ns["contextmanager"] = contextlib.contextmanager
    ns["dataclasses"] = dataclasses
    ns["logging"] = __import__("logging")
    ns["cast"] = typing.cast
    ns["Any"] = typing.Any
    ns["Optional"] = typing.Optional
    ns["Path"] = Path
    ns["json"] = json
    ns["logger"] = RecLogger()
    ns["Platform"] = Platform
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    # module constants (plain-Name targets only)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayNotificationsMixin":
            for sub in node.body:
                if isinstance(sub, ast.ClassDef) and sub.name == "_CompletionClaim":
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec", flags=flags), ns)
                elif isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in PORTED:
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec", flags=flags), ns)
    # real _log_suppressed from gateway/run_shutdown.py (keep its decorator)
    stree = ast.parse(open(SHUTDOWN_SRC, encoding="utf-8").read())
    for node in stree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_log_suppressed":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SHUTDOWN_SRC, "exec", flags=flags), ns)
    need = PORTED | {"_CompletionClaim", "_log_suppressed", "_WATCHER_ROUTE_FIELDS"}
    missing = need - set(ns)
    if missing:
        raise RuntimeError("extract failed to bind: %s" % sorted(missing))
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


def denorm(x):
    if isinstance(x, Platform):
        return x.value
    if isinstance(x, (AdapterDouble, AttrDouble, SessionStoreDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return denorm(dataclasses.asdict(x))
    if isinstance(x, dict):
        return {denorm(k): denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "msg": str(x)}
    return x


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


def install_globals():
    NS["logger"] = RecLogger()
    NS["Platform"] = Platform
    NS["time"] = types.SimpleNamespace(time=seam("time.time", ARGMAPS["time.time"]))
    NS["t"] = seam("t", ARGMAPS["t"])
    NS["_held_reason"] = seam("_held_reason", ARGMAPS["_held_reason"])
    NS["_raw_process_event_session_id"] = seam("_raw_process_event_session_id",
                                               ARGMAPS["_raw_process_event_session_id"])
    NS["_safe_delivery_transport"] = seam("_safe_delivery_transport", ARGMAPS["_safe_delivery_transport"])
    for pkgname in ("gateway", "tools", "agent", "persistence"):
        pkg(pkgname)
    pkg("gateway.platforms")
    stubmod("gateway.run",
            _redact_gateway_user_facing_secrets=seam("_redact_gateway_user_facing_secrets", ARGMAPS["_redact_gateway_user_facing_secrets"]),
            _format_concise_process_notification=seam("_format_concise_process_notification", ARGMAPS["_format_concise_process_notification"]),
            _shorten_command_for_display=seam("_shorten_command_for_display", ARGMAPS["_shorten_command_for_display"]),
            _non_conversational_metadata=seam("_non_conversational_metadata", ARGMAPS["_non_conversational_metadata"]),
            _multiplex_profile_homes=seam("_multiplex_profile_homes", ARGMAPS["_multiplex_profile_homes"]),
            _profile_runtime_scope=lambda path, ctx: ScopeDouble(path.as_posix()),
            _USER_BOUNDARY_END_REASONS=("session_reset", "user_exit", "session_switch", "new_session"))
    stubmod("gateway.delivery", resolve_delivery_transport=seam("resolve_delivery_transport", ARGMAPS["resolve_delivery_transport"]))
    stubmod("gateway.session_detached", detached_route_of=seam("detached_route_of", ARGMAPS["detached_route_of"]))
    stubmod("gateway.session_identity", identity_of=seam("identity_of", ARGMAPS["identity_of"]))
    stubmod("gateway.wake",
            adapter_supports_push=seam("adapter_supports_push", ARGMAPS["adapter_supports_push"]),
            session_owned_by_profile=seam("session_owned_by_profile", ARGMAPS["session_owned_by_profile"]))
    stubmod("tools.ansi_strip", strip_ansi=seam("strip_ansi", ARGMAPS["strip_ansi"]))
    stubmod("tools.process_registry",
            transform_process_output=seam("transform_process_output", ARGMAPS["transform_process_output"]),
            process_registry=types.SimpleNamespace(completion_queue=types.SimpleNamespace(id="completion_queue")))
    stubmod("agent.redact", redact_terminal_output=seam("redact_terminal_output", ARGMAPS["redact_terminal_output"]))
    stubmod("tools.async_delegation",
            claim_completion_delivery=seam("claim_completion_delivery", ARGMAPS["claim_completion_delivery"]),
            restore_undelivered_completions=seam("restore_undelivered_completions", ARGMAPS["restore_undelivered_completions"]),
            sweep_orphaned_completions=seam("sweep_orphaned_completions", ARGMAPS["sweep_orphaned_completions"]))
    stubmod("agent.i18n", t=NS["t"])
    stubmod("uuid", uuid4=lambda: types.SimpleNamespace(hex=materialize(script("uuid4_hex", {}))))


install_globals()


def mat_pcfg(pc):
    if not isinstance(pc, dict):
        return pc
    d = dict(pc)
    if isinstance(d.get("home_channel"), dict):
        d["home_channel"] = AttrDouble(d["home_channel"])
    return AttrDouble(d)


def mat_profile_cfg(pcfg):
    if not isinstance(pcfg, dict):
        return pcfg
    d = dict(pcfg)
    if isinstance(d.get("platforms"), dict):
        d["platforms"] = {Platform(pv): mat_pcfg(pc) for pv, pc in d["platforms"].items()}
    return AttrDouble(d)


def build_self(inp):
    sd = types.SimpleNamespace()
    d = inp.get("self") or {}
    for k, v in d.items():
        if k == "adapters":
            setattr(sd, k, {Platform(pv): mat_adapter(av) for pv, av in v.items()})
        elif k == "config":
            cfg = dict(v)
            if isinstance(cfg.get("platforms"), dict):
                cfg["platforms"] = {Platform(pv): mat_pcfg(pc) for pv, pc in cfg["platforms"].items()}
            setattr(sd, k, AttrDouble(cfg))
        elif k == "_profile_configs":
            setattr(sd, k, {prof: mat_profile_cfg(pc) for prof, pc in v.items()} if isinstance(v, dict) else v)
        elif k == "_profile_adapters":
            setattr(sd, k, {prof: {Platform(pv): mat_adapter(av) for pv, av in av_map.items()}
                            for prof, av_map in v.items()} if isinstance(v, dict) else v)
        elif k == "persistence":
            setattr(sd, k, AttrDouble(v) if isinstance(v, dict) else v)
        elif k == "session_store":
            setattr(sd, k, SessionStoreDouble(v) if isinstance(v, dict) else v)
        else:
            setattr(sd, k, deepcopy(v))
    # ported siblings run inline (real)
    for nm in METHODS:
        if nm in STATIC_METHODS:
            setattr(sd, nm, REAL[nm])
        else:
            setattr(sd, nm, types.MethodType(REAL[nm], sd))
    # the nested claim dataclass is a class attribute accessed via self
    setattr(sd, "_CompletionClaim", NS["_CompletionClaim"])
    # claimed / foreign self-method seams (instance attributes; no self)
    for live, seamname in SELF_SEAMS.items():
        setattr(sd, live, seam(seamname, ARGMAPS[seamname], async_=seamname in ASYNC_SEAMS))
    return sd


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": state_fn()}


def drive(func, inp):
    live = "_" + func
    sd = build_self(inp)
    nostate = lambda: None
    g = inp.get

    if func == "redacted_output_tail":
        sess = types.SimpleNamespace(**(g("session") or {}))
        return run_flow(lambda: REAL[live](sess, g("limit", 1000)), nostate)
    if func == "build_process_completion_event":
        sess = types.SimpleNamespace(**(g("session") or {}))
        return run_flow(lambda: REAL[live](g("watcher") or {}, sess, g("session_id", "")), nostate)
    if func == "format_coalesced_process_completions":
        return run_flow(lambda: REAL[live](g("entries") or []), nostate)
    if func == "format_process_final_message":
        sess = types.SimpleNamespace(**(g("session") or {}))
        return run_flow(lambda: getattr(sd, live)(g("session_id", ""), sess, g("notify_mode", "")), nostate)
    if func == "format_process_running_message":
        sess = types.SimpleNamespace(**(g("session") or {}))
        return run_flow(lambda: getattr(sd, live)(sess), nostate)
    if func == "launching_turn_active":
        return run_flow(lambda: asyncio.run(getattr(sd, live)(g("platform_name", ""), g("watcher") or {})), nostate)
    if func == "send_watcher_message":
        return run_flow(lambda: asyncio.run(getattr(sd, live)(
            g("platform_name", ""), g("chat_id"), g("thread_id"), g("message_text", ""), g("watcher") or {})), nostate)
    if func == "each_secondary_ledger":
        fn = lambda: materialize(script("ledger.fn", {}))
        return run_flow(lambda: getattr(sd, live)(g("profile_homes") or [], fn, g("verb", "")), nostate)
    if func == "restore_secondary_completion_ledgers":
        return run_flow(lambda: getattr(sd, live)(g("profile_homes") or []), nostate)
    if func == "sweep_orphaned_completion_ledgers":
        return run_flow(lambda: getattr(sd, live)(), nostate)
    if func == "classify_completion_target":
        return run_flow(lambda: asyncio.run(getattr(sd, live)(g("parent_session_id", ""), g("session_key", ""))), nostate)
    if func == "classify_completion_owner":
        return run_flow(lambda: asyncio.run(getattr(sd, live)(g("parent_session_id", ""), g("session_key", ""))), nostate)
    if func == "resolve_injection_adapter":
        src = g("source")
        source = types.SimpleNamespace(**src) if isinstance(src, dict) else src
        return run_flow(lambda: getattr(sd, live)(g("platform_name", ""), source), nostate)
    if func == "served_api_server_wake_profile":
        return run_flow(lambda: getattr(sd, live)(g("evt") or {}, g("raw_sid", "")), nostate)
    if func == "adapter_by_platform_value":
        return run_flow(lambda: getattr(sd, live)(g("platform_name", "")), nostate)
    if func == "home_channel_transports":
        return run_flow(lambda: list(getattr(sd, live)()), nostate)
    if func == "served_home_channel_configs":
        return run_flow(lambda: list(getattr(sd, live)()), nostate)
    if func == "served_home_channel_transports":
        return run_flow(lambda: list(getattr(sd, live)()), nostate)
    if func in ("completion_delivery_ready", "preflight_completion_delivery"):
        evt = deepcopy(g("evt") or {})

        def state_fn():
            st = {"evt": denorm(evt)}
            if "_completion_no_transport_warned" in sd.__dict__:
                w = sd.__dict__["_completion_no_transport_warned"]
                st["warned"] = sorted([list(p) for p in w],
                                      key=lambda p: (p[0] is not None, p[0] or "", p[1]))
            return st

        if func == "completion_delivery_ready":
            return run_flow(lambda: asyncio.run(getattr(sd, live)(evt)), state_fn)
        # preflight: id(self) is a scripted seam
        real_id = builtins.id

        def fake_id(obj):
            if obj is sd:
                return materialize(script("self_id", {}))
            return real_id(obj)

        def body():
            builtins.id = fake_id
            try:
                return asyncio.run(getattr(sd, live)(evt))
            finally:
                builtins.id = real_id
        return run_flow(body, state_fn)
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
    print("ORACLE run_notifications_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())