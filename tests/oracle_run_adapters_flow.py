#!/usr/bin/env python3
"""Oracle for the gateway/run_adapters.py adapter-lifecycle FLOW cores
(src/gateway/run_adapters_flow.c).

AST-extracts the LIVE GatewayAdapterLifecycleMixin methods (no heavy gateway
import), seeds the module constants (_UNSET, _SYSTEMD_BOOT_STEP_MARGIN_SECS,
ON_ALL_ADAPTERS_DOWN_POLICIES) and _log_suppressed, and replaces every live
I/O dependency with recording stubs driven by the SAME per-name scripts the C
harness feeds its universal seams (in.io). Compares the ordered seam-call trace
plus the return value and the mutated runner/info state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion); {"__raise__": kind,
"msg": text} raises a real exception of that kind; absent -> None.

Modeling: Platform doubles keyed by .value; adapter doubles carry an id plus
live fields (platform value string, fatal_error_*, has_fatal_error,
_sidecar_bind/_sidecar_port); seam args reference adapters by id; claims are
tuples (denorm -> lists). asyncio is modeled by ids (ensure_future returns a
TaskDouble; `await task` reuses the connect value silently). Sets normalize to
sorted id lists. Real internal calls (ported siblings) run inline on both
sides. BaseException kinds pass through every `except Exception`.

Input: stdin JSON lines from t_port_run_adapters_flow.
Summary: ORACLE run_adapters_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import contextlib
import json
import os
import sys
import types
import __future__
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
UTC = timezone.utc

CUR = {"func": None, "in": {}, "trace": [], "tasks": {}}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
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
        "OverflowError": OverflowError,
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


class Plat:
    def __init__(self, value):
        self.value = value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, Plat) and self.value == other.value

    def __repr__(self):
        return "Plat(%r)" % self.value


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class RunnerDouble:
    """Mutable runner state double; data lives in _d, methods bound separately."""
    def __init__(self):
        object.__setattr__(self, "_d", {})

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)

    def __setattr__(self, name, val):
        object.__getattribute__(self, "_d")[name] = val


def cbname(cb):
    nm = getattr(cb, "__name__", "")
    if nm == "consume_detached_task_result":
        return "consume_detached_task_result"
    if nm == "discard":
        return "set.discard"
    return nm or "callback"


def consume_detached_task_result(task):
    pass


class TaskDouble:
    def __init__(self, tid, value=None):
        self.id = tid
        self.value = value

    def cancel(self):
        return materialize(script("task.cancel", {"id": self.id}))

    def add_done_callback(self, cb):
        return materialize(script("task.add_done_callback", {"id": self.id, "cb": cbname(cb)}))

    def __await__(self):
        if False:
            yield
        return self.value


def register_task(tid, value=None):
    t = CUR["tasks"].get(tid)
    if t is None:
        t = TaskDouble(tid, value)
        CUR["tasks"][tid] = t
    return t


class _Claim:
    def __init__(self, result=None, exc=None):
        self.result = result
        self.exc = exc

    def __await__(self):
        if self.exc is not None:
            raise self.exc
        if False:
            yield
        return self.result


class AdapterDouble:
    def __init__(self, d):
        dd = dict(d)
        if isinstance(dd.get("platform"), str):
            dd["platform"] = Plat(dd["platform"])
        object.__setattr__(self, "_d", dd)

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)

    def connect(self, is_reconnect=False):
        v = materialize(script("adapter.connect", {"is_reconnect": is_reconnect}))
        return _Claim(v)


def mk_adapter(d):
    return AdapterDouble(d) if isinstance(d, dict) else d


def mk_info(d):
    out = dict(d)
    if isinstance(out.get("predecessor"), dict):
        out["predecessor"] = mk_adapter(out["predecessor"])
    return out


class HooksDouble:
    def emit(self, event_name, ctx):
        v = materialize(script("hooks.emit", {"event_name": event_name, "ctx": denorm(ctx)}))
        return _Claim(v)


class EventDouble:
    def set(self):
        return materialize(script("shutdown_event.set", {}))


class AsyncioDouble:
    CancelledError = asyncio.CancelledError

    @staticmethod
    def wait(tasks, timeout=None):
        ids = sorted(t.id for t in tasks)
        v = materialize(script("asyncio.wait", {"tasks": ids, "timeout": timeout}))
        if v is None:
            return _Claim(None)
        done = {CUR["tasks"][i] for i in v.get("done", []) if i in CUR["tasks"]}
        pending = {CUR["tasks"][i] for i in v.get("pending", []) if i in CUR["tasks"]}
        return _Claim((done, pending))

    @staticmethod
    def ensure_future(x):
        tid = materialize(script("asyncio.ensure_future", {}))
        val = x.result if isinstance(x, _Claim) else x
        t = TaskDouble(tid, value=val)
        CUR["tasks"][tid] = t
        return t


def run_coro(coro):
    try:
        coro.send(None)
    except StopIteration as si:
        return si.value
    raise RuntimeError("coroutine yielded unexpectedly")


class DatetimeDouble:
    @staticmethod
    def now(tz=None):
        v = materialize(script("datetime.now", {"tz": "UTC"}))
        return datetime.fromisoformat(v)


def denorm(x):
    if isinstance(x, RunnerDouble):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, (AttrDouble, AdapterDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, Plat):
        return x.value
    if isinstance(x, TaskDouble):
        return x.id
    if isinstance(x, set):
        return {"set": sorted(denorm(v) for v in x)}
    if isinstance(x, dict):
        return {denorm(k): denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, datetime):
        return x.isoformat()
    return x


# ── live-def extraction ──

WANTED = (
    "_env_timeout_override", "_wait_or_detach", "_connect_adapter_bounded",
    "_connect_adapter_with_timeout", "_extend_systemd_boot_for", "_handle_reaction_event",
    "_on_all_adapters_down", "_adapter_listener_claim", "_adapter_credential_claim",
    "_reconnect_queue_entry", "_queue_retryable_best_effort", "_retain_background_task",
    "_track_task_in", "_request_clean_exit", "_flag_reconnect_needs_attention",
    "_bump_reconnect_backoff", "_adapter_may_heal", "_reconnect_failed_platform",
)


def extract(rel, tops=(), methods=(), pre=None, seed=True, strip_decorators=False):
    path = os.path.join(QUARRY, rel)
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "raf_extracted_" + rel.replace("/", "_").replace(".", "_"),
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
            if strip_decorators:
                node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in methods:
                    if strip_decorators:
                        sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), path, "exec", flags=flags), ns)
    if seed:
        seed_assigns()
    return ns


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


NS_SHUTDOWN = extract("gateway/run_shutdown.py", tops=("_log_suppressed",),
                      pre={"contextmanager": contextlib.contextmanager, "logger": RecLogger()})
NS = extract("gateway/run_adapters.py", methods=WANTED, strip_decorators=True,
             pre={"logging": types.SimpleNamespace(DEBUG=10, INFO=20, WARNING=30, ERROR=40),
                  "logger": RecLogger(), "contextlib": contextlib})

M_GATEWAY = mod("gateway")
M_GATEWAY.run = mod("gateway.run")
M_GATEWAY.platform_registry = mod("gateway.platform_registry")
M_GATEWAY.systemd_notify = mod("gateway.systemd_notify")


def install_case():
    SEQPOS.clear()
    CUR["tasks"] = {}
    logger = RecLogger()
    NS["logger"] = logger
    NS_SHUTDOWN["logger"] = logger
    NS["_log_suppressed"] = NS_SHUTDOWN["_log_suppressed"]
    NS["asyncio"] = AsyncioDouble
    NS["time"] = types.SimpleNamespace(monotonic=lambda: materialize(script("time.monotonic", {})))
    NS["os"] = types.SimpleNamespace(getenv=lambda name, default="": (
        lambda v: v if v is not None else default)(materialize(script("os.getenv", {"name": name, "default": default}))))
    NS["datetime"] = DatetimeDouble
    NS["timedelta"] = timedelta
    NS["UTC"] = UTC
    NS["ON_ALL_ADAPTERS_DOWN_POLICIES"] = ("exit", "stay_alive")
    NS["consume_detached_task_result"] = consume_detached_task_result
    NS["carry_inbound_dedup"] = lambda predecessor, adapter: materialize(script(
        "carry_inbound_dedup", {"predecessor": predecessor.id if predecessor is not None else None,
                                "adapter": adapter.id}))
    NS["hand_over_held_inbound"] = lambda adapter, predecessor: materialize(script(
        "hand_over_held_inbound", {"adapter": adapter.id,
                                   "predecessor": predecessor.id if predecessor is not None else None}))
    NS["_adapter_unavailable_message"] = lambda platform, **k: materialize(script(
        "_adapter_unavailable_message", {"platform": platform.value}))
    NS["record_platform_connect"] = _record_platform_connect
    NS["record_platform_disconnect"] = lambda *a, **k: materialize(script("record_platform_disconnect", {}))

    builtin = CUR["in"].get("_BUILTIN_ADAPTERS")
    if builtin is None:
        builtin = ["api_server", "webhook", "msgraph_webhook"]
    M_GATEWAY.run._BUILTIN_ADAPTERS = {Plat(p): () for p in builtin}
    M_GATEWAY.run._reconnect_needs_attention = lambda info, now: materialize(script(
        "_reconnect_needs_attention", {"info": denorm(info), "now": now}))
    M_GATEWAY.run._reconnect_backoff = lambda attempt: materialize(script(
        "_reconnect_backoff", {"attempt": attempt}))
    M_GATEWAY.run._platform_has_bot_credential = lambda platform, platform_config: materialize(script(
        "_platform_has_bot_credential", {"platform": platform.value, "platform_config": denorm(platform_config)}))
    M_GATEWAY.run._dispose_unused_adapter = lambda adapter: _Claim(materialize(script(
        "_dispose_unused_adapter", {"adapter": adapter.id})))

    class _GR:
        @staticmethod
        def _adapter_credential_fingerprint(adapter):
            return materialize(script("adapter_credential_fingerprint", {"adapter": adapter.id}))
    M_GATEWAY.run.GatewayRunner = _GR

    def _registry_get(value):
        return materialize(script("platform_registry.get", {"platform": value}))
    M_GATEWAY.platform_registry.platform_registry = types.SimpleNamespace(
        is_registered=lambda value: materialize(script("platform_registry.is_registered", {"platform": value})),
        get=_registry_get)
    M_GATEWAY.systemd_notify.extend_start_timeout = lambda seconds: materialize(script(
        "extend_start_timeout", {"seconds": seconds}))


def _record_platform_connect(adapter, platform, *, is_reconnect, ok, exc=None):
    args = {"adapter": adapter.id, "platform": platform.value, "is_reconnect": is_reconnect, "ok": ok}
    if exc is not None:
        args["exc"] = raise_spec(exc)
    return materialize(script("record_platform_connect", args))


_UNSET = NS.get("_UNSET") or object()


def build_runner(inp):
    d = inp.get("self") or {}
    sd = RunnerDouble()
    for k, v in d.items():
        if k == "adapters":
            sd._d[k] = {Plat(pv): mk_adapter(av) for pv, av in v.items()}
        elif k == "_failed_platforms":
            sd._d[k] = {Plat(pv): mk_info(iv) for pv, iv in v.items()}
        elif k == "config":
            sd._d[k] = AttrDouble(v)
        elif k == "_background_tasks":
            if isinstance(v, dict) and "set" in v:
                sd._d[k] = {register_task(i) for i in v["set"]}
            elif isinstance(v, dict) and "other" in v:
                sd._d[k] = v["other"]
            else:
                sd._d[k] = v
        elif k == "hooks":
            sd._d[k] = v
            object.__setattr__(sd, "hooks", HooksDouble())
        elif k == "_shutdown_event":
            sd._d[k] = v
            object.__setattr__(sd, "_shutdown_event", EventDouble())
        else:
            sd._d[k] = v

    # real internal methods (ported siblings)
    object.__setattr__(sd, "_wait_or_detach", NS["_wait_or_detach"])
    object.__setattr__(sd, "_connect_adapter_bounded",
                       types.MethodType(NS["_connect_adapter_bounded"], sd))
    object.__setattr__(sd, "_adapter_credential_claim", NS["_adapter_credential_claim"])
    object.__setattr__(sd, "_adapter_listener_claim", NS["_adapter_listener_claim"])
    object.__setattr__(sd, "_flag_reconnect_needs_attention",
                       types.MethodType(NS["_flag_reconnect_needs_attention"], sd))
    object.__setattr__(sd, "_bump_reconnect_backoff",
                       types.MethodType(NS["_bump_reconnect_backoff"], sd))
    object.__setattr__(sd, "_adapter_may_heal", types.MethodType(NS["_adapter_may_heal"], sd))

    # scripted self-method seams
    object.__setattr__(sd, "_platform_connect_timeout_secs",
                       lambda platform=None, *, initial=False: materialize(script(
                           "platform_connect_timeout_secs",
                           {"platform": platform.value if platform is not None else None, "initial": initial})))

    def _update_status(status_key, *, platform_state=None, error_code=None, error_message=None,
                       needs_attention=None, retrying_since=_UNSET):
        args = {"status_key": status_key}
        if platform_state is not None:
            args["platform_state"] = platform_state
        if error_code is not None:
            args["error_code"] = error_code
        if error_message is not None:
            args["error_message"] = error_message
        if needs_attention is not None:
            args["needs_attention"] = needs_attention
        if retrying_since is not _UNSET:
            args["retrying_since"] = retrying_since
        return materialize(script("update_platform_runtime_status", args))
    object.__setattr__(sd, "_update_platform_runtime_status", _update_status)

    def _drop(platform, reason):
        materialize(script("drop_from_reconnect_queue", {"platform": platform.value, "reason": reason}))
        del sd._failed_platforms[platform]
    object.__setattr__(sd, "_drop_from_reconnect_queue", _drop)

    object.__setattr__(sd, "_mark_platform_fatal", lambda status_key, adapter: materialize(script(
        "mark_platform_fatal", {"status_key": status_key, "adapter": adapter.id})))

    def _mark_unavail(status_key, platform, *, retrying, needs_attention=None):
        args = {"status_key": status_key, "platform": platform.value, "retrying": retrying}
        if needs_attention is not None:
            args["needs_attention"] = needs_attention
        return materialize(script("mark_adapter_unavailable", args))
    object.__setattr__(sd, "_mark_adapter_unavailable", _mark_unavail)

    object.__setattr__(sd, "_rearm_failed_plugin_load", lambda platform, info: _Claim(materialize(script(
        "rearm_failed_plugin_load", {"platform": platform.value, "info": denorm(info)}))))

    def _create_off(platform, config):
        v = materialize(script("create_adapter_off_loop_deps",
                               {"platform": platform.value, "config": denorm(config)}))
        return _Claim(mk_adapter(v) if isinstance(v, dict) else v)
    object.__setattr__(sd, "_create_adapter_off_loop_deps", _create_off)

    object.__setattr__(sd, "_wire_adapter_handlers", lambda adapter, **kw: materialize(script(
        "wire_adapter_handlers", {"adapter": adapter.id})))
    object.__setattr__(sd, "_connect_adapter_with_timeout",
                       lambda adapter, platform, *, is_reconnect=False, initial=False: _Claim(materialize(script(
                           "connect_adapter_with_timeout",
                           {"adapter": adapter.id, "platform": platform.value, "is_reconnect": is_reconnect}))))
    object.__setattr__(sd, "_install_reconnected_adapter", lambda platform, adapter: _Claim(materialize(script(
        "install_reconnected_adapter", {"platform": platform.value, "adapter": adapter.id}))))

    def _run_exec(fn, *a):
        fnname = "platform_registry.get" if fn is M_GATEWAY.platform_registry.platform_registry.get \
            else getattr(fn, "__name__", "fn")
        return _Claim(materialize(script("_run_in_executor_with_context",
                                         {"fn": fnname, "arg": a[0] if a else None})))
    object.__setattr__(sd, "_run_in_executor_with_context", _run_exec)
    object.__setattr__(sd, "_queue_retryable_fatal_platform", lambda adapter, delay=0.0: materialize(script(
        "queue_retryable_fatal_platform", {"adapter": adapter.id})))
    return sd


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": state_fn()}


def drive(func, inp):
    platform = Plat(inp.get("platform")) if inp.get("platform") is not None else None
    adapter = mk_adapter(inp.get("adapter")) if isinstance(inp.get("adapter"), dict) else inp.get("adapter")
    sd = build_runner(inp)
    nostate = lambda: None

    if func == "env_timeout_override":
        return run_flow(lambda: NS["_env_timeout_override"](inp.get("name", "")), nostate)
    if func == "wait_or_detach":
        task = register_task(inp.get("task", ""))
        return run_flow(lambda: run_coro(NS["_wait_or_detach"](task, inp.get("timeout", 0.0))), nostate)
    if func == "connect_adapter_bounded":
        return run_flow(lambda: run_coro(NS["_connect_adapter_bounded"](
            sd, adapter, platform, is_reconnect=inp.get("is_reconnect", False),
            initial=inp.get("initial", False))), nostate)
    if func == "connect_adapter_with_timeout":
        return run_flow(lambda: run_coro(NS["_connect_adapter_with_timeout"](
            sd, adapter, platform, is_reconnect=inp.get("is_reconnect", False),
            initial=inp.get("initial", False))), nostate)
    if func == "extend_systemd_boot_for":
        return run_flow(lambda: NS["_extend_systemd_boot_for"](sd, platform), nostate)
    if func == "handle_reaction_event":
        return run_flow(lambda: run_coro(NS["_handle_reaction_event"](sd, inp.get("ctx") or {})), nostate)
    if func == "on_all_adapters_down":
        return run_flow(lambda: NS["_on_all_adapters_down"](sd), nostate)
    if func == "adapter_listener_claim":
        return run_flow(lambda: NS["_adapter_listener_claim"](platform, adapter), nostate)
    if func == "adapter_credential_claim":
        return run_flow(lambda: NS["_adapter_credential_claim"](platform, adapter), nostate)
    if func == "reconnect_queue_entry":
        return run_flow(lambda: NS["_reconnect_queue_entry"](
            sd, platform, adapter, inp.get("platform_config") or {},
            attempts=inp.get("attempts", 0), delay=inp.get("delay", 0.0),
            queued=inp.get("queued", True)), nostate)
    if func == "queue_retryable_best_effort":
        return run_flow(lambda: NS["_queue_retryable_best_effort"](sd, adapter, inp.get("why", "")), nostate)
    if func == "retain_background_task":
        task = register_task(inp.get("task", ""))
        return run_flow(lambda: NS["_retain_background_task"](sd, task), lambda: denorm(sd._d))
    if func == "track_task_in":
        tasks = {register_task(i) for i in (inp.get("tasks") or [])}
        task = register_task(inp.get("task", ""))
        return run_flow(lambda: NS["_track_task_in"](tasks, task),
                        lambda: {"tasks": sorted(t.id for t in tasks)})
    if func == "request_clean_exit":
        return run_flow(lambda: NS["_request_clean_exit"](sd, inp.get("reason", "")), lambda: denorm(sd._d))
    if func == "flag_reconnect_needs_attention":
        info = mk_info(inp.get("info") or {})
        return run_flow(lambda: NS["_flag_reconnect_needs_attention"](
            sd, platform, info, inp.get("now", 0.0), status_key=inp.get("status_key")),
            lambda: {"info": denorm(info)})
    if func == "bump_reconnect_backoff":
        info = mk_info(inp.get("info") or {})
        return run_flow(lambda: NS["_bump_reconnect_backoff"](
            sd, platform, info, inp.get("attempt", 0),
            inp.get("error_code"), inp.get("error_message")), lambda: {"info": denorm(info)})
    if func == "adapter_may_heal":
        return run_flow(lambda: NS["_adapter_may_heal"](sd, platform, inp.get("platform_config") or {}), nostate)
    if func == "reconnect_failed_platform":
        return run_flow(lambda: run_coro(NS["_reconnect_failed_platform"](sd, platform, inp.get("now", 0.0))),
                        lambda: denorm(sd._d))
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
    install_case()
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
    print("ORACLE run_adapters_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
