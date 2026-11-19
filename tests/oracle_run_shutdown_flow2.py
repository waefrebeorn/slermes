#!/usr/bin/env python3
"""Oracle for the gateway/run_shutdown.py UNCLAIMED-remainder FLOW cores
(src/gateway/run_shutdown_flow2.c).

AST-extracts the LIVE top-level helpers and GatewayShutdownMixin methods (no
heavy gateway import), seeds the module constants / logger, and replaces every
live I/O dependency with recording stubs driven by the SAME per-name scripts the
C harness feeds its universal seams (in.io). Compares the ordered seam-call trace
plus the return value and the mutated runner state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text} raises a real exception of that kind; a scripted
null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for <name>"
(recorded as ret, then raised). BaseException kinds pass through every
`except Exception`. Ported siblings run inline (real) on both sides; claimed /
foreign siblings are seams.

Input: stdin JSON lines from t_port_run_shutdown_flow2.
Summary: ORACLE run_shutdown_flow2: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import contextlib
import json
import os
import sys
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


class Platform:
    def __init__(self, value):
        self.value = value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, Platform) and self.value == other.value

    def __repr__(self):
        return "Platform(%r)" % self.value


Platform.API_SERVER = Platform("api_server")
Platform.RELAY = Platform("relay")
Platform.LOCAL = Platform("local")
Platform.WEBHOOK = Platform("webhook")


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


class ApiAdapter:
    """API-server adapter double: present hook keys model callable methods."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            def method(*args):
                return materialize(script("api_server_hook", {"name": name, "args": list(args)}))
            return method
        raise AttributeError("missing attribute '%s'" % name)


class RedialAdapter:
    """Relay adapter double: hold_redial / release_redial hooks."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            def method():
                return materialize(script("adapter." + name, {}))
            return method
        raise AttributeError("missing attribute '%s'" % name)


def mat_result(v):
    return AttrDouble(v) if isinstance(v, dict) else v


def denorm(x):
    if isinstance(x, RunnerDouble):
        return {denorm(k): denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, (AttrDouble, ApiAdapter, RedialAdapter)):
        return {denorm(k): denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, Platform):
        return x.value
    if isinstance(x, dict):
        return {denorm(k): denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    return x


# ── live-def extraction ──

TOPS = (
    "_log_suppressed", "_send_failed", "_send_error", "_notice_target_key",
    "_delivery_target_key", "_effective_watchdog_leash", "_active_async_delegation_count",
)
METHODS = (
    "_active_work_count", "_running_cron_job_count", "_active_cron_job_count",
    "_api_server_hook", "_active_api_run_count", "_active_api_worker_count",
    "_mark_api_runs_shutdown_requested", "_drain_work_counts", "_gateway_cfg_section",
    "_scale_to_zero_idle_timeout_seconds", "_relay_wake_url_or_none",
    "_scale_to_zero_hold_redial", "_scale_to_zero_abandon_suspend",
    "_restart_notification_allowed", "_restart_wait_cron_counts",
    "_wedged_cron_job_count", "_restart_safe_cron_count", "_wedged_agent_count",
    "_awaitable_work_count",
)
STATIC_METHODS = {"_running_cron_job_count", "_gateway_cfg_section", "_relay_wake_url_or_none"}
PORTED = set(TOPS) | set(METHODS)


def extract():
    path = os.path.join(QUARRY, "gateway", "run_shutdown.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "rs2_extracted", "__file__": path,
          "contextmanager": contextlib.contextmanager, "logger": RecLogger()}
    sys.modules.setdefault("rs2_extracted", types.ModuleType("rs2_extracted"))
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TOPS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHODS:
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = PORTED - set(ns)
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


def install_globals():
    # module-top foreign names used as bare globals -> seams
    NS["logger"] = RecLogger()
    NS["Platform"] = Platform
    NS["managed_turn_count"] = lambda runner: materialize(script("managed_turn_count", {}))
    NS["effective_stop_drain_timeout"] = lambda runner: materialize(script("effective_stop_drain_timeout", {}))
    NS["resolve_shutdown_watchdog_delay"] = lambda delay: materialize(script("resolve_shutdown_watchdog_delay", {"delay": delay}))
    NS["effective_stop_watchdog_delay"] = lambda runner, delay: materialize(script("effective_stop_watchdog_delay", {"delay": delay}))
    NS["time"] = types.SimpleNamespace(time=lambda: materialize(script("time.time", {})))
    # in-function imports -> sys.modules stubs (resolve at call time)
    pkg("gateway"); pkg("tools"); pkg("cron"); pkg("gateway.platforms")
    stubmod("gateway.delivery", looks_like_telegram_private_chat_id=lambda chat_id: materialize(
        script("looks_like_telegram_private_chat_id", {"chat_id": chat_id})))
    stubmod("tools.async_delegation", active_count=lambda: materialize(script("async_delegation.active_count", {})))
    stubmod("cron.scheduler",
            get_running_job_ids=lambda: materialize(script("cron.get_running_job_ids", {})),
            get_restart_wait_cron_counts=lambda: materialize(script("cron.get_restart_wait_cron_counts", {})))
    stubmod("gateway.platforms.api_server_runs",
            api_worker_live_count=lambda: materialize(script("api_worker_live_count", {})))
    stubmod("gateway.run", _load_gateway_config=lambda: materialize(script("_load_gateway_config", {})))
    stubmod("gateway.scale_to_zero",
            parse_idle_timeout_seconds=lambda value: materialize(script("parse_idle_timeout_seconds", {"value": value})))
    stubmod("gateway.relay", relay_wake_url=lambda: materialize(script("relay_wake_url", {})))


install_globals()


def build_self(inp):
    sd = RunnerDouble()
    d = inp.get("self") or {}
    for k, v in d.items():
        if k == "adapters":
            sd._d[k] = {Platform(pv): ApiAdapter(av) for pv, av in v.items()}
        elif k == "config":
            cfg = dict(v)
            if isinstance(cfg.get("platforms"), dict):
                cfg["platforms"] = {Platform(pv): (AttrDouble(pc) if isinstance(pc, dict) else pc)
                                    for pv, pc in cfg["platforms"].items()}
            sd._d[k] = AttrDouble(cfg)
        else:
            sd._d[k] = v
    # ported siblings run inline (real)
    for nm in METHODS:
        if nm in STATIC_METHODS:
            object.__setattr__(sd, nm, REAL[nm])
        else:
            object.__setattr__(sd, nm, types.MethodType(REAL[nm], sd))
    # claimed / foreign self-method seams
    object.__setattr__(sd, "_running_agent_count",
                       lambda: materialize(script("running_agent_count", {})))
    object.__setattr__(sd, "_active_deferred_agent_worker_count",
                       lambda: materialize(script("active_deferred_agent_worker_count", {})))
    object.__setattr__(sd, "_wedged_chat_agent_count",
                       lambda: materialize(script("wedged_chat_agent_count", {})))

    def _relay():
        v = materialize(script("relay_adapter_for_dormancy", {}))
        return RedialAdapter(v) if isinstance(v, dict) else v
    object.__setattr__(sd, "_relay_adapter_for_dormancy", _relay)
    object.__setattr__(sd, "_serving_state", lambda: materialize(script("serving_state", {})))
    object.__setattr__(sd, "_scale_to_zero_status",
                       lambda state, fail_msg: materialize(script("scale_to_zero_status",
                                                                  {"state": state, "fail_msg": fail_msg})))
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

    if func == "send_failed":
        return run_flow(lambda: NS[live](mat_result(inp.get("result"))), nostate)
    if func == "send_error":
        return run_flow(lambda: NS[live](mat_result(inp.get("result"))), nostate)
    if func == "notice_target_key":
        return run_flow(lambda: NS[live](inp.get("platform", ""), inp.get("chat_id"), inp.get("thread_id")), nostate)
    if func == "delivery_target_key":
        return run_flow(lambda: NS[live](inp.get("platform", ""), inp.get("chat_id"),
                                         inp.get("thread_id"), profile=inp.get("profile")), nostate)
    if func == "log_suppressed":
        level = inp.get("level", 10)
        msg = inp.get("msg", "")
        args = inp.get("args") or []
        exc_info = inp.get("exc_info", False)

        def body():
            with NS[live](level, msg, *args, exc_info=exc_info):
                materialize(script("step.fn", {}))
            return None
        return run_flow(body, nostate)
    if func == "effective_watchdog_leash":
        return run_flow(lambda: NS[live](sd), nostate)
    if func == "active_async_delegation_count":
        return run_flow(lambda: NS[live](), nostate)
    if func == "running_cron_job_count":
        return run_flow(lambda: REAL[live](), nostate)
    if func == "gateway_cfg_section":
        return run_flow(lambda: REAL[live](inp.get("name", "")), nostate)
    if func == "relay_wake_url_or_none":
        return run_flow(lambda: REAL[live](), nostate)
    if func == "restart_notification_allowed":
        return run_flow(lambda: getattr(sd, live)(Platform(inp.get("platform", ""))), nostate)
    if func == "api_server_hook":
        return run_flow(lambda: getattr(sd, live)(inp.get("name", ""), *(inp.get("args") or [])), nostate)
    if func == "scale_to_zero_hold_redial":
        return run_flow(lambda: getattr(sd, live)(inp.get("held", True)), nostate)
    if func == "scale_to_zero_abandon_suspend":
        return run_flow(lambda: getattr(sd, live)(), lambda: denorm(sd._d))
    # remaining zero-arg instance methods
    if live in METHODS:
        return run_flow(lambda: getattr(sd, live)(), nostate)
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
    print("ORACLE run_shutdown_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())