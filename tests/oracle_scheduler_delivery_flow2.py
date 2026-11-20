#!/usr/bin/env python3
"""Oracle for scheduler_delivery_flow2 (31 UNCLAIMED cron/scheduler_delivery.py defs).

The harness emits {"func","in","out"}; this oracle drives the LIVE, AST-extracted
Python defs with scripted recording stubs and diffs {ret, trace, state, echoes}.
NAME-ONLY trace. Full builtin exception hierarchy. Fail-closed: 0 cases => 1.
Summary: 'ORACLE scheduler_delivery_flow2: N cases, M mismatches'.
"""
import ast, copy, errno, json, os, sys, types, math, contextlib, threading, hashlib, logging
from pathlib import PurePosixPath, Path
from datetime import datetime
from collections import OrderedDict
from dataclasses import dataclass, field
import __future__ as _fut

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_QUARRY = _HERE.parents[2] / "hermes-agent"
if not (_QUARRY / "cron" / "scheduler_delivery.py").is_file():
    _QUARRY = Path.home() / "hermes-agent"
SRC = str(_QUARRY / "cron" / "scheduler_delivery.py")
os.environ.setdefault("HOME", str(_HERE.parents[2]))
os.environ.setdefault("USERPROFILE", str(_HERE.parents[2]))
os.environ.setdefault("HERMES_HOME", str(_HERE.parents[1] / ".testbin" / "dbg_home"))

PY = {
    "_resolve_cron_surface_mode": "_resolve_cron_surface_mode",
    "_fresh_thread_overrides_origin_topic": "_fresh_thread_overrides_origin_topic",
    "_origin_thread_is": "_origin_thread_is",
    "_resolve_origin": "_resolve_origin",
    "_redact_cron_payload": "_redact_cron_payload",
    "_cron_display_name": "_cron_display_name",
    "_cron_mirror_message": "_cron_mirror_message",
    "_seed_cron_session": "_seed_cron_session",
    "_home_env_lookup": "_home_env_lookup",
    "_relay_fronted_delivery_platforms": "_relay_fronted_delivery_platforms",
    "_home_target": "_home_target",
    "_get_standalone_send_timeout": "_get_standalone_send_timeout",
    "bot_chat_message": "bot_chat_message",
    "_deliver_to_bot_chat": "_deliver_to_bot_chat",
    "_resolve_bot_chat_target": "_resolve_bot_chat_target",
    "_result_field": "_result_field",
    "_note_target_error": "_note_target_error",
    "_warn_live_lane_failure": "_warn_live_lane_failure",
    "_resolve_target_transport": "_resolve_target_transport",
    "_inchannel_surface_supported": "_inchannel_surface_supported",
    "_named_dm_topic": "_named_dm_topic",
    "_live_route_metadata": "_live_route_metadata",
    "_live_send_text": "_live_send_text",
    "_resolve_named_dm_topic": "_resolve_named_dm_topic",
    "_live_send_media": "_live_send_media",
    "_seed_live_delivery_sessions": "_seed_live_delivery_sessions",
    "_deliver_via_live_adapter": "_deliver_via_live_adapter",
    "_deliver_standalone": "_deliver_standalone",
    "_prepare_target_delivery": "_prepare_target_delivery",
    "_unresolved_delivery_outcome": "_unresolved_delivery_outcome",
    "_should_wrap_cron_response": "_should_wrap_cron_response",
}

# which echo keys each function emits (must match harness)
ECHO = {
    "_deliver_to_bot_chat": ["job"],
    "_note_target_error": ["errors"],
    "_live_send_text": ["target_errors", "delivery_errors", "unverified_targets", "t"],
    "_resolve_named_dm_topic": ["metadata", "delivery_errors", "t"],
    "_live_send_media": ["delivery_errors", "unverified_targets", "t"],
    "_deliver_via_live_adapter": ["target_errors", "delivery_errors", "unverified_targets", "t"],
    "_deliver_standalone": ["target_errors", "delivery_errors", "t"],
    "_prepare_target_delivery": ["delivery_errors"],
}

# ── full builtin exception hierarchy ─────────────────────────────────────────
_EXC = {
    "BaseException": BaseException, "Exception": Exception,
    "GeneratorExit": GeneratorExit, "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
    "ArithmeticError": ArithmeticError, "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "BytesWarning": BytesWarning, "DeprecationWarning": DeprecationWarning,
    "EOFError": EOFError, "EnvironmentError": OSError, "FloatingPointError": FloatingPointError,
    "FutureWarning": FutureWarning, "ImportError": ImportError, "ImportWarning": ImportWarning,
    "IndentationError": IndentationError, "IndexError": IndexError, "IOError": OSError,
    "KeyError": KeyError, "LookupError": LookupError, "MemoryError": MemoryError,
    "NameError": NameError, "NotImplementedError": NotImplementedError, "OSError": OSError,
    "OverflowError": OverflowError, "PendingDeprecationWarning": PendingDeprecationWarning,
    "ReferenceError": ReferenceError, "ResourceWarning": ResourceWarning, "RuntimeError": RuntimeError,
    "RuntimeWarning": RuntimeWarning, "StopIteration": StopIteration, "SyntaxError": SyntaxError,
    "SyntaxWarning": SyntaxWarning, "SystemError": SystemError, "TabError": TabError,
    "TimeoutError": TimeoutError, "TypeError": TypeError, "UnboundLocalError": UnboundLocalError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "UnicodeWarning": UnicodeWarning, "UserWarning": UserWarning, "ValueError": ValueError,
    "Warning": Warning, "ZeroDivisionError": ZeroDivisionError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "ModuleNotFoundError": ModuleNotFoundError, "RecursionError": RecursionError,
    "StopAsyncIteration": StopAsyncIteration, "EncodingWarning": EncodingWarning,
}
class PartialDeliveryError(Exception):
    pass
_EXC["PartialDeliveryError"] = PartialDeliveryError
for _n in dir(builtins := __import__("builtins")):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException) and _n not in _EXC:
        _EXC[_n] = _o

def make_exc(spec):
    t = spec.get("type", "RuntimeError"); msg = spec.get("message", "")
    cls = _EXC.get(t) or type(t, (Exception,), {})
    try: e = cls(msg) if msg else cls()
    except Exception: e = cls(msg)
    en = spec.get("errno")
    if en and isinstance(e, OSError):
        try: e.errno = getattr(errno, en, None)
        except Exception: pass
    # extra attributes consumed by Except bodies (identity-safe, same class object)
    for k in ("result", "send_retryable", "started"):
        if k in spec:
            try: setattr(e, k, spec[k])
            except Exception: pass
    return e

def raise_spec(e):
    d = {"__raise__": type(e).__name__, "message": str(e)}
    if isinstance(e, OSError) and e.errno is not None:
        d["errno"] = e.errno; d["message"] = e.strerror if e.strerror is not None else str(e)
    for k in ("send_retryable", "started", "result"):
        if hasattr(e, k):
            v = getattr(e, k)
            if v is not None: d[k] = canon(v)
    return d

# ── attribute double (supports attr + dict access; projects as a dict) ───────
class AttrDict(dict):
    def __getattr__(self, k):
        if k in self: return self[k]
        raise AttributeError(k)
    def __setattr__(self, k, v): self[k] = v
    def __delattr__(self, k):
        try: del self[k]
        except KeyError: raise AttributeError(k)

def mat(v):
    if isinstance(v, dict): return AttrDict((k, mat(x)) for k, x in v.items())
    if isinstance(v, list): return [mat(x) for x in v]
    return v

class PlatformVal:
    def __init__(self, v): self.value = v
    def __eq__(self, o): return self.value == (o.value if isinstance(o, PlatformVal) else o)
    def __hash__(self): return hash(self.value)
    def __str__(self): return self.value
    def __repr__(self): return "Platform." + self.value
def platform_ctor(name):
    r = CURRENT["ctx"].rec("Platform")(name)
    return PlatformVal(str(r)) if r is not None else None
platform_ctor.TELEGRAM = PlatformVal("telegram")
platform_ctor.DISCORD = PlatformVal("discord")
platform_ctor.SLACK = PlatformVal("slack")
Platform = platform_ctor

class RecLogger:
    def __init__(self, ctx): self.ctx = ctx
    def __getattr__(self, level):
        def log(msg, *a, exc_info=None, **kw):
            pa = [str(x) if isinstance(x, BaseException) else x for x in a]
            d = {"msg": msg, "args": pa}
            if exc_info is not None: d["exc_info"] = exc_info
            self.ctx.rec("logger." + level)(**d)
        return log

class CaseCtx:
    def __init__(self, inp):
        self.inp = inp; self.io = inp.get("io") or {}; self.cursors = {}
        self.trace = []; self.last = {}; self.state = copy.deepcopy(inp.get("state") or {})
    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None: return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry): return None
            self.cursors[name] = idx + 1; entry = entry[idx]
        return entry
    def rec(self, name):
        ctx = self
        def call(*a, **kw):
            ctx.trace.append(name)
            raw = ctx.script_next(name)
            ctx.last[name] = raw
            if isinstance(raw, dict):
                eff = raw.get("state")
                if isinstance(eff, dict): ctx.state.update(copy.deepcopy(eff))
                rs = raw.get("raise")
                if isinstance(rs, dict): raise make_exc(rs)
                return mat(raw.get("ret"))
            return mat(raw)
        return call

CURRENT = {"ctx": None}
def rec(name):
    def f(*a, **kw): return CURRENT["ctx"].rec(name)(*a, **kw)
    return f

def _pkg(name):
    m = types.ModuleType(name); m.__path__ = []; return m

def canon(v):
    if isinstance(v, BaseException): return raise_spec(v)
    if isinstance(v, PlatformVal): return v.value
    if isinstance(v, AttrDict): return {canon(k): canon(x) for k, x in v.items()}
    if isinstance(v, dict): return {canon(k): canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple, set, frozenset)): return [canon(x) for x in v]
    if isinstance(v, PurePosixPath): return str(v)
    if isinstance(v, float):
        if math.isnan(v) or math.isinf(v): return str(v)
        return int(v) if v == int(v) and abs(v) < 1e16 else v
    return v


# ── AST extraction of the live defs (no heavy quarry import) ──────────────────
_ANN = _fut.annotations.compiler_flag

def extract():
    mod = types.ModuleType("cron.scheduler_delivery")
    sys.modules["cron.scheduler_delivery"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "cron.scheduler_delivery",
        "contextlib": contextlib, "os": os, "sys": sys, "logging": logging,
        "threading": threading, "hashlib": hashlib, "json": json, "errno": errno,
        "PurePosixPath": PurePosixPath, "Path": Path,
        "dataclass": dataclass, "field": field, "Any": object, "Optional": object,
        "List": list, "Set": set, "Tuple": tuple, "Dict": dict,
        "REQUIRED_FIELDS": [],
        "_NON_PUSH_ORIGIN_PLATFORMS": frozenset({"api_server"}),
        "_KNOWN_DELIVERY_PLATFORMS": frozenset({"webhook", "wecom_callback"}),
        "_LEGACY_HOME_TARGET_ENV_VARS": {"QQBOT_HOME_CHANNEL": "QQ_HOME_CHANNEL"},
        "_HOME_TARGET_PRIORITY": ("matrix","telegram","discord","slack","signal","mattermost",
            "sms","email","dingtalk","feishu","wecom","weixin","bluebubbles","qqbot",
            "whatsapp","whatsapp_cloud"),
        "BOT_CHAT_PLATFORM": "bot-chat", "ROOM_PLATFORM": "room",
        "_LIVE_SEND_CONFIRM_TIMEOUT_SECS": 60,
        "NONE": "none", "PARTIAL": "partial",
        "logger": RecLogger(CaseCtx({})),
        # module-level late-bound collaborators: replaced per case in seed_case
        "_sched": types.SimpleNamespace(load_config=lambda: {}),
        "_origin": types.SimpleNamespace(stamp_origin_discriminators=lambda *a: None),
        "_preflight": types.SimpleNamespace(SharedRouteAdapters=type("SharedRouteAdapters", (), {})),
        "record_cron_delivery": lambda *a, **k: None,
        "recording_cron_delivery": lambda *a, **k: None,
        "_send_media_via_adapter": lambda *a, **k: [],
        "_standalone_send": lambda *a, **k: (None, None),
        "_queue_for_live_reconnect": lambda *a, **k: None,
        "Platform": Platform, "PlatformConfig": type("PlatformConfig", (), {}),
        "SharedRouteAdapters": type("SharedRouteAdapters", (), {}),
        "DeliveryTransport": type("DeliveryTransport", (), {}),
        "PartialDeliveryError": PartialDeliveryError,
        "delivery_state": lambda r: "none",
        "NONE": "none", "PARTIAL": "partial",
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # constants first
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if not isinstance(tgt, ast.Name): continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=_ANN), ns)
            except Exception: pass
    # _TargetDelivery class (keeps @dataclass decorator)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_TargetDelivery":
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=_ANN), ns)
            except Exception: pass
    # the 31 functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in set(PY.values()):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=_ANN), ns)
            except Exception: pass
    missing = [v for v in PY.values() if v not in ns or not callable(ns[v])]
    if missing:
        print("FATAL scheduler_delivery_flow2: extraction missing:", sorted(missing)); sys.exit(2)
    return ns

NS = extract()
LIVE = {v: NS[v] for v in PY.values()}  # originals; SIB overrides are recorders

def install_import_stubs(ctx):
    """Stub every in-function import module; attributes bind case recorders."""
    recs = {
        "agent.redact": {"redact_sensitive_text": "redact_sensitive_text"},
        "agent.secret_scope": {"get_secret": "get_secret"},
        "agent.async_utils": {"safe_schedule_threadsafe": "safe_schedule_threadsafe",
                              "_ensure_named_dm_topic": "_ensure_named_dm_topic"},
        "gateway.config": {"Platform": "__PLATFORM__", "PlatformConfig": "__PLATFORMCFG__"},
        "gateway.session": {"SessionSource": "SessionSource"},
        "gateway.mirror": {"mirror_to_session": "mirror_to_session"},
        "gateway.delivery": {"DeliveryTransport": "__DELIVERYTRANSPORT__",
            "resolve_delivery_transport": "resolve_delivery_transport",
            "DeliveryRouter": "DeliveryRouter", "DeliveryTarget": "DeliveryTarget",
            "PartialDeliveryError": "__PARTIALDELIVERYERROR__",
            "_looks_like_int": "_looks_like_int",
            "looks_like_telegram_private_chat_id": "looks_like_telegram_private_chat_id",
            "_ensure_named_dm_topic": "_ensure_named_dm_topic"},
        "gateway.relay": {"relay_fronted_platforms": "relay_fronted_platforms"},
        "gateway.platforms._shared": {"NONE": "none", "PARTIAL": "partial",
                                      "delivery_state": "delivery_state"},
        "gateway.warning_notifications": {"warning_notifications_enabled": "warning_notifications_enabled"},
        "hermes_cli.config_effective": {"load_user_config_effective": "load_user_config_effective"},
        "hermes_cli.profiles": {"get_profile_dir": "get_profile_dir",
            "normalize_profile_name": "normalize_profile_name", "profile_exists": "profile_exists"},
        "hermes_constants": {"get_hermes_home": "get_hermes_home"},
        "tools.bot_live_delivery": {"deliver_to_live_owner": "deliver_to_live_owner",
            "find_canonical_live_owner": "find_canonical_live_owner",
            "read_delivery_result": "read_delivery_result"},
        "cron.bot_chat_legacy": {"defer": "defer"},
        "cron.scheduler_delivery_standalone": {"_standalone_send": "standalone_send",
            "_queue_for_live_reconnect": "queue_for_live_reconnect"},
        "cron.scheduler_delivery_media": {"_send_media_via_adapter": "send_media_via_adapter"},
        "cron.scheduler_delivery_origin": {},
        "cron.scheduler_preflight": {},
    }
    sentinel = {"__PLATFORM__": platform_ctor, "__PARTIALDELIVERYERROR__": PartialDeliveryError, "__PLATFORMCFG__": PlatformConfigStub,
                "__DELIVERYTRANSPORT__": DeliveryTransportStub}
    parts = ("agent", "gateway", "hermes_cli", "tools", "cron")
    for p in parts: sys.modules.setdefault(p, _pkg(p))
    sys.modules.setdefault("gateway.platforms", _pkg("gateway.platforms"))
    for name, attrs in recs.items():
        m = types.ModuleType(name); m.__path__ = []
        for a, sn in attrs.items():
            if sn in sentinel: setattr(m, a, sentinel[sn])
            elif sn in ("none", "partial"): setattr(m, a, sn)
            else: setattr(m, a, rec(sn))
        sys.modules[name] = m
        # link parent attribute
        if "." in name:
            par, leaf = name.rsplit(".", 1)
            try: setattr(sys.modules[par], leaf, m)
            except Exception: pass

class PlatformConfigStub:
    def __init__(self, enabled=True): self.enabled = enabled
class DeliveryTransportStub:
    def __new__(cls, adapter, config, platform):
        return CURRENT["ctx"].rec("DeliveryTransport")(adapter, config, platform)


# live function name -> C seam name (unclaimed siblings are SEAMED, not composed)
SIB = {
    "_resolve_origin": "resolve_origin",
    "_redact_cron_payload": "redact_cron_payload",
    "_cron_display_name": "cron_display_name",
    "_cron_mirror_message": "cron_mirror_message",
    "bot_chat_message": "bot_chat_message",
    "_get_home_target_thread_id": "get_home_target_thread_id",
    "_sched": None,
    "_delivery_lane_value": "delivery_lane_value",
    "_normalize_deliver_value": "normalize_deliver_value",
    "_target_matches_origin": "target_matches_origin",
    "_target_mirror_eligible": "target_mirror_eligible",
    "_inchannel_seed_allowed": "inchannel_seed_allowed",
    "_note_target_error": "note_target_error",
    "_warn_live_lane_failure": "warn_live_lane_failure",
    "_result_field": "result_field",
    "_confirm_adapter_delivery": "confirm_adapter_delivery",
    "_is_channel_dm_topic": "is_channel_dm_topic",
    "_named_dm_topic": "named_dm_topic",
    "_live_route_metadata": "live_route_metadata",
    "_live_send_text": "live_send_text",
    "_resolve_named_dm_topic": "resolve_named_dm_topic",
    "_live_send_media": "live_send_media",
    "_seed_cron_thread_session": "seed_cron_thread_session",
    "_seed_cron_channel_session": "seed_cron_channel_session",
    "_maybe_mirror_cron_delivery": "maybe_mirror_cron_delivery",
    "_seed_live_delivery_sessions": "seed_live_delivery_sessions",
    "_origin_thread_is": "origin_thread_is",
    "_fresh_thread_overrides_origin_topic": "fresh_thread_overrides_origin_topic",
    "_open_continuable_cron_thread": "open_continuable_cron_thread",
    "_inchannel_surface_supported": "inchannel_surface_supported",
    "_resolve_cron_surface_mode": "resolve_cron_surface_mode",
    "_resolve_target_transport": "resolve_target_transport",
    "_origin": None,
    "_log_": None,
}

# module-level bare globals refreshed per case (cheap; all name recorders)
BARE = ["redact_sensitive_text", "get_secret", "get_hermes_home", "get_profile_dir",
    "warning_notifications_enabled", "load_user_config_effective", "normalize_profile_name",
    "profile_exists", "deliver_to_live_owner", "find_canonical_live_owner", "read_delivery_result",
    "defer", "resolve_delivery_transport", "relay_fronted_platforms", "DeliveryTransport",
    "DeliveryRouter", "DeliveryTarget", "PartialDeliveryError", "_looks_like_int",
    "looks_like_telegram_private_chat_id", "_ensure_named_dm_topic", "safe_schedule_threadsafe",
    "SessionSource", "mirror_to_session", "PlatformConfig", "delivery_state",
    "record_cron_delivery", "recording_cron_delivery", "_send_media_via_adapter",
    "_standalone_send", "_queue_for_live_reconnect",
    # claimed siblings / unclaimed siblings used by these functions -> seams
    "resolve_origin", "redact_cron_payload", "cron_display_name", "cron_mirror_message",
    "bot_chat_message", "get_home_target_thread_id", "load_config",
    "delivery_lane_value", "normalize_deliver_value", "target_matches_origin",
    "target_mirror_eligible", "inchannel_seed_allowed", "note_target_error",
    "warn_live_lane_failure", "result_field", "confirm_adapter_delivery",
    "is_channel_dm_topic", "named_dm_topic", "live_route_metadata", "live_send_text",
    "resolve_named_dm_topic", "live_send_media", "seed_cron_thread_session",
    "seed_cron_channel_session", "maybe_mirror_cron_delivery", "seed_live_delivery_sessions",
    "origin_thread_is", "fresh_thread_overrides_origin_topic", "open_continuable_cron_thread",
    "inchannel_surface_supported", "resolve_cron_surface_mode", "resolve_target_transport",
]


class SharedAdapters(AttrDict):
    def get(self, k, d=None): return CURRENT["ctx"].rec("shared_get")(k, d)

class FutureDouble:
    def result(self, timeout=None): return CURRENT["ctx"].rec("future.result")(timeout=timeout)
    def cancel(self): return None

def seed_case(ctx, func):
    CURRENT["ctx"] = ctx
    install_import_stubs(ctx)
    NS["logger"] = RecLogger(ctx)
    for name in BARE:
        NS[name] = rec(name)
    for live, seam in SIB.items():
        if seam: NS[live] = rec(seam)
    # special class/constant bindings (identity matters for except / isinstance)
    NS["Platform"] = Platform
    NS["PlatformConfig"] = PlatformConfigStub
    NS["SharedRouteAdapters"] = SharedAdapters
    NS["DeliveryTransport"] = DeliveryTransportStub
    NS["PartialDeliveryError"] = PartialDeliveryError
    NS["NONE"] = "none"; NS["PARTIAL"] = "partial"
    NS["_preflight"] = types.SimpleNamespace(SharedRouteAdapters=SharedAdapters)
    NS["_sched"] = types.SimpleNamespace(load_config=rec("load_config"))
    def stamp_stub(t, rm, mm):
        r = rec("stamp_origin_discriminators")(t, rm, mm)
        if isinstance(r, dict):
            if isinstance(r.get("route_metadata"), dict): rm.update(r["route_metadata"])
            if isinstance(r.get("media_metadata"), dict): mm.update(r["media_metadata"])
        return r
    NS["_origin"] = types.SimpleNamespace(stamp_origin_discriminators=stamp_stub)
    def home_stub():
        r = ctx.rec("get_hermes_home")()
        return HomePath(r) if r is not None else HomePath("")
    def profdir_stub(p):
        r = ctx.rec("get_profile_dir")(p)
        return HomePath(r) if r is not None else HomePath("")
    NS["get_hermes_home"] = home_stub
    NS["get_profile_dir"] = profdir_stub
    def conf_stub(result, job_id, gap):
        r = rec("confirm_adapter_delivery")(result, job_id, gap)
        if isinstance(r, dict):
            if isinstance(r.get("evidence"), list): gap.extend(r["evidence"])
            return r.get("ok")
        return r
    NS["_confirm_adapter_delivery"] = conf_stub
    NS["confirm_adapter_delivery"] = conf_stub
    def note_stub(job, msg, errors, *a, **k):
        r = ctx.rec("note_target_error")(job, msg, errors, *a, **k)
        if isinstance(errors, list): errors.append(msg)
        return r
    NS["_note_target_error"] = note_stub
    def rt_stub(*a, **k):
        r = ctx.rec("resolve_target_transport")(*a, **k)
        if isinstance(r, dict):
            if not r.get("ok"): return (None, canon(r.get("error")))
            return ((mat(r.get("transport")), mat(r.get("pconfig")),
                     mat(r.get("runtime_adapter")), mat(r.get("target_adapters"))), None)
        return (r, None)
    NS["_resolve_target_transport"] = rt_stub
    def safe_sched(coro, loop):
        if hasattr(coro, "close"):
            try: coro.close()
            except Exception: pass
        r = rec("safe_schedule_threadsafe")(coro, loop)
        return None if r is None else FutureDouble()
    NS["safe_schedule_threadsafe"] = safe_sched
    # module-level names used directly by some defs
    NS["record_cron_delivery"] = rec("record_cron_delivery")
    NS["recording_cron_delivery"] = rec("recording_cron_delivery")
    NS["_send_media_via_adapter"] = rec("send_media_via_adapter")
    def standalone_stub(*a, **k):
        r = ctx.rec("standalone_send")(*a, **k)
        if isinstance(r, dict): return (mat(r.get("result")), mat(r.get("err")))
        return r
    NS["_standalone_send"] = standalone_stub
    NS["_queue_for_live_reconnect"] = rec("queue_for_live_reconnect")
    NS["delivery_state"] = rec("delivery_state")
    au = sys.modules.get("agent.async_utils")
    if au is not None: au.safe_schedule_threadsafe = NS["safe_schedule_threadsafe"]

def mat_pconfig(spec):
    if spec is None: return None
    if isinstance(spec, dict): return SimpleNamespaceExtra(**{k: mat(v) for k, v in spec.items()})
    return spec

class SimpleNamespaceExtra(types.SimpleNamespace):
    pass

def mat_adapter(spec):
    """runtime_adapter descriptor -> object with optional seamed callable."""
    if spec is None: return None
    d = spec if isinstance(spec, dict) else {}
    o = types.SimpleNamespace()
    if d.get("per_platform"):
        setattr(o, "supports_inchannel_continuable_for_platform", rec("supports_inchannel_continuable_for_platform"))
    if "supports" in d:
        setattr(o, "supports_inchannel_continuable", d["supports"])
    if d.get("_session_store"):
        setattr(o, "_session_store", types.SimpleNamespace(
            get_or_create_session=rec("get_or_create_session")))
    for k, v in d.items():
        if not hasattr(o, k): setattr(o, k, mat(v))
    return o

def mat_t(spec):
    """Build a real _TargetDelivery from fixture spec."""
    TD = NS["_TargetDelivery"]
    kw = dict(spec or {})
    pl = kw.get("platform")
    kw["platform"] = PlatformVal(pl) if pl is not None else PlatformVal("")
    tr = kw.get("transport")
    kw["transport"] = mat(tr) if tr is not None else None
    cfg = kw.get("config")
    if isinstance(cfg, dict):
        cfg = mat(cfg)
        setattr(cfg, "get_home_channel", rec("get_home_channel"))
        kw["config"] = cfg
    loop = kw.get("loop")
    if isinstance(loop, dict) and "is_running" in loop:
        lr = loop["is_running"]
        kw["loop"] = types.SimpleNamespace(is_running=lambda: CURRENT["ctx"].rec("loop_is_running")())
    # fill any missing required dataclass fields with neutral defaults
    for f in TD.__dataclass_fields__.values():
        if f.name not in kw:
            if f.default is not dataclasses.MISSING: kw[f.name] = f.default
            elif f.default_factory is not dataclasses.MISSING: kw[f.name] = f.default_factory()
            else: kw[f.name] = None
    return TD(**kw)

import dataclasses

def t_echo(spec, t):
    e = copy.deepcopy(spec or {})
    e["thread_id"] = canon(t.thread_id)
    e["live_error"] = canon(t.live_error)
    e["live_retryable"] = canon(t.live_retryable)
    e["standalone_in_flight"] = bool(t.standalone_in_flight)
    return e

def proj_td(td):
    """Project a live _TargetDelivery into the C harness's ret dict shape."""
    d = {}
    for f in td.__dataclass_fields__:
        v = getattr(td, f)
        d[f] = canon(v)
    return d

def norm_ret(r):
    if isinstance(r, BaseException): return raise_spec(r)
    if isinstance(r, tuple): return list(r)
    return canon(r)

def g(inp, k, default=None):
    return inp.get(k, default)


class HomePath:
    def __init__(self, s): self.s = str(s)
    def resolve(self): return self
    def __truediv__(self, other): return HomePath(self.s.rstrip("/") + "/" + str(other))
    def __str__(self): return self.s

class ShaResult:
    def __init__(self, v): self.v = v
    def hexdigest(self): return str(self.v)

class PlatformsDict(dict):
    def get(self, k, d=None): return CURRENT["ctx"].rec("platforms_get")(k, d)

def mat_send_result(spec):
    if isinstance(spec, dict) and "__attr__" in spec:
        return types.SimpleNamespace(**{k: mat(v) for k, v in spec["__attr__"].items()})
    return mat(spec)

def seed_extra(ctx):
    hm = types.ModuleType("hashlib")
    hm.sha256 = lambda b: ShaResult(ctx.rec("sha256")(b))
    sys.modules["hashlib"] = hm
    um = types.ModuleType("uuid")
    um.uuid4 = lambda: types.SimpleNamespace(hex=ctx.rec("uuid4")())
    sys.modules["uuid"] = um
    dm = types.ModuleType("dataclasses")
    dm.replace = ctx.rec("replace"); dm.dataclass = dataclass; dm.field = field
    sys.modules["dataclasses"] = dm
    # in-function-import module attributes that need wrapped stubs
    hc = sys.modules.get("hermes_constants")
    if hc is not None: hc.get_hermes_home = NS["get_hermes_home"]
    hp = sys.modules.get("hermes_cli.profiles")
    if hp is not None: hp.get_profile_dir = NS["get_profile_dir"]

def make_adapters(spec):
    if isinstance(spec, dict) and spec.get("__shared__"):
        return SharedAdapters(spec)
    return mat(spec)

def make_config(spec):
    c = mat(spec or {})
    if not hasattr(c, "platforms"):
        c.platforms = PlatformsDict()
    else:
        c.platforms = PlatformsDict(c.platforms or {})
    return c

def make_loop(spec):
    if spec is None: return None
    return types.SimpleNamespace(is_running=lambda: CURRENT["ctx"].rec("loop_is_running")())

def lst(out, k):
    v = out.get(k)
    if not isinstance(v, list):
        v = out[k] = []
    return v

def run_case(func, inp):
    ctx = CaseCtx(inp)
    t = None
    seed_case(ctx, func)
    seed_extra(ctx)
    out = {"ret": None, "trace": ctx.trace, "state": ctx.state}
    for k in ECHO.get(func, []):
        out[k] = copy.deepcopy(inp.get(k))
    py = PY[func]
    try:
        if func == "_resolve_cron_surface_mode":
            out["ret"] = LIVE[py](mat_pconfig(inp.get("pconfig")), g(inp,"logical_name"))
        elif func == "_fresh_thread_overrides_origin_topic":
            out["ret"] = LIVE[py](mat_pconfig(inp.get("pconfig")))
        elif func == "_origin_thread_is":
            out["ret"] = LIVE[py](g(inp,"job"), g(inp,"thread_id"))
        elif func == "_resolve_origin":
            out["ret"] = LIVE[py](g(inp,"job"))
        elif func == "_redact_cron_payload":
            out["ret"] = LIVE[py](g(inp,"text"), g(inp,"what"))
        elif func == "_cron_display_name":
            out["ret"] = LIVE[py](g(inp,"job"))
        elif func == "_cron_mirror_message":
            out["ret"] = LIVE[py](g(inp,"job"), g(inp,"text"))
        elif func == "_seed_cron_session":
            out["ret"] = LIVE[py](g(inp,"job"), mat_adapter(inp.get("adapter")),
                g(inp,"platform_name"), g(inp,"chat_id"), g(inp,"text"),
                thread_id=g(inp,"thread_id"), chat_type=g(inp,"chat_type"),
                user_id=g(inp,"user_id"), user_name=g(inp,"user_name"),
                chat_name=g(inp,"chat_name"), scope_id=g(inp,"scope_id"),
                discord_keys_on_thread=bool(g(inp,"discord_keys_on_thread",False)))
        elif func == "_home_env_lookup":
            out["ret"] = LIVE[py](g(inp,"env_var"), g(inp,"suffix", ""), strip=bool(g(inp,"strip",False)))
        elif func == "_relay_fronted_delivery_platforms":
            r = LIVE[py](set(g(inp,"connected") or []))
            out["ret"] = sorted(canon(r)) if isinstance(r, (set, frozenset)) else canon(r)
        elif func == "_home_target":
            out["ret"] = LIVE[py](g(inp,"platform_name"), g(inp,"chat_id"), g(inp,"resolved_from"))
        elif func == "_get_standalone_send_timeout":
            out["ret"] = LIVE[py]()
        elif func == "bot_chat_message":
            out["ret"] = LIVE[py](g(inp,"job"), g(inp,"content"))
        elif func == "_deliver_to_bot_chat":
            job = out["job"]
            out["ret"] = LIVE[py](job, g(inp,"content"), g(inp,"profile"),
                                for_failure=bool(g(inp,"for_failure",False)))
        elif func == "_resolve_bot_chat_target":
            out["ret"] = LIVE[py](g(inp,"job"), g(inp,"profile_arg"))
        elif func == "_result_field":
            out["ret"] = LIVE[py](mat_send_result(inp.get("send_result")), g(inp,"key"), g(inp,"default"))
        elif func == "_note_target_error":
            errs = out["errors"]
            if not isinstance(errs, list): errs = out["errors"] = []
            LIVE[py](g(inp,"job"), g(inp,"msg"), errs)
            out["ret"] = None
        elif func == "_warn_live_lane_failure":
            LIVE[py](g(inp,"job"), g(inp,"msg"), bool(g(inp,"is_relay",False)))
            out["ret"] = None
        elif func == "_resolve_target_transport":
            res, err = LIVE[py](g(inp,"job"), PlatformVal(g(inp,"platform")),
                g(inp,"platform_name"), g(inp,"target"), make_adapters(inp.get("adapters")),
                make_config(inp.get("config")))
            if res is None:
                out["ret"] = {"ok": False, "error": canon(err)}
            else:
                tr, pc, ra, ta = res
                out["ret"] = {"ok": True, "error": None, "transport": canon(tr),
                              "pconfig": canon(pc), "runtime_adapter": canon(ra),
                              "target_adapters": canon(ta)}
        elif func == "_inchannel_surface_supported":
            out["ret"] = LIVE[py](mat_adapter(inp.get("adapter")), g(inp,"platform_name"))
        elif func == "_named_dm_topic":
            out["ret"] = LIVE[py](mat_t(inp.get("t")))
        elif func == "_live_route_metadata":
            rt, rm, mm = LIVE[py](mat_t(inp.get("t")))
            out["ret"] = {"route_thread_id": canon(rt), "route_metadata": canon(rm),
                          "media_metadata": canon(mm)}
        elif func == "_live_send_text":
            t = mat_t(inp.get("t"))
            te = lst(out, "target_errors")
            de = lst(out, "delivery_errors")
            uv = lst(out, "unverified_targets")
            out["ret"] = LIVE[py](t, g(inp,"text"), g(inp,"route_thread_id"), g(inp,"route_metadata"),
                target_errors=te, delivery_errors=de, unverified_targets=uv)
            out["t"] = t_echo(inp.get("t"), t)
        elif func == "_resolve_named_dm_topic":
            t = mat_t(inp.get("t"))
            md = out["metadata"]
            de = lst(out, "delivery_errors")
            out["ret"] = LIVE[py](t, g(inp,"topic_name"), md, de, g(inp,"n_media",0),
                                refresh=bool(g(inp,"refresh",False)))
            out["t"] = t_echo(inp.get("t"), t)
        elif func == "_live_send_media":
            t = mat_t(inp.get("t"))
            de = lst(out, "delivery_errors")
            uv = lst(out, "unverified_targets")
            LIVE[py](t, g(inp,"media_metadata"), g(inp,"media_files"), de, uv, g(inp,"topic_name"))
            out["ret"] = None
            out["t"] = t_echo(inp.get("t"), t)
        elif func == "_seed_live_delivery_sessions":
            t = mat_t(inp.get("t"))
            LIVE[py](t, g(inp,"delivered_message_id"))
            out["ret"] = None
        elif func == "_deliver_via_live_adapter":
            t = mat_t(inp.get("t"))
            te = lst(out, "target_errors")
            de = lst(out, "delivery_errors")
            uv = lst(out, "unverified_targets")
            out["ret"] = LIVE[py](t, g(inp,"cleaned_text"), g(inp,"media_files"), target_errors=te,
                                delivery_errors=de, unverified_targets=uv)
            out["t"] = t_echo(inp.get("t"), t)
        elif func == "_deliver_standalone":
            t = mat_t(inp.get("t"))
            te = lst(out, "target_errors")
            de = lst(out, "delivery_errors")
            out["ret"] = LIVE[py](t, g(inp,"content"), g(inp,"media_files"), te, de)
            out["t"] = t_echo(inp.get("t"), t)
        elif func == "_prepare_target_delivery":
            de = lst(out, "delivery_errors")
            td = LIVE[py](g(inp,"job"), g(inp,"target"),
                adapters=make_adapters(inp.get("adapters")),
                loop=make_loop(inp.get("loop")), config=make_config(inp.get("config")),
                notify_delivery=bool(g(inp,"notify_delivery",False)),
                mirror_enabled=bool(g(inp,"mirror_enabled",False)),
                mirror_text=g(inp,"mirror_text"), delivery_errors=de)
            out["ret"] = proj_td(td) if td is not None else None
            if out["ret"] is not None:
                out["ret"]["loop"] = canon(inp.get("loop"))
        elif func == "_unresolved_delivery_outcome":
            out["ret"] = LIVE[py](g(inp,"job"), bool(g(inp,"for_failure",False)))
        elif func == "_should_wrap_cron_response":
            out["ret"] = LIVE[py](g(inp,"job"), g(inp,"user_cfg"))
        else:
            raise KeyError(func)
    except BaseException as e:
        out["ret"] = raise_spec(e)
        if t is not None and "t" in ECHO.get(func, []):
            out["t"] = t_echo(inp.get("t"), t)
    out["ret"] = norm_ret(out["ret"])
    for k in list(out):
        if k not in ("ret", "trace", "state"):
            out[k] = canon(out[k])
    return out

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try: c = json.loads(line)
        except json.JSONDecodeError: continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1; print(f"PYERR {func}: unknown func"); continue
        try: want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1; print(f"PYERR {func}: {type(e).__name__}: {e}"); continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace") and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        for k in ECHO.get(func, []):
            if got.get(k) != want.get(k): ok = False
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE scheduler_delivery_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1

if __name__ == "__main__":
    sys.exit(main())
