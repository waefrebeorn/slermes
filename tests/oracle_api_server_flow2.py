#!/usr/bin/env python3
"""Oracle for the gateway/platforms/api_server.py UNCLAIMED remainder
(src/gateway/platforms/api_server_flow2.c).

AST-extracts the 24 target functions plus the PURE claimed leaves (run REAL, no
PoP re-claim) and the _ProviderAuthResolutionError class, seeds faithful seam
recorders for every EFFECTFUL collaborator (config reads, provider resolution,
profile/model lookups, the clock, asyncio.sleep, web.json_response, the
runtime-status writer, the runs-module delegates, i18n t(), the auth rate-limit
probe and the request-profile ContextVar), and binds the targets onto a
SimpleNamespace adapter double. Ported targets run INLINE and compose with each
other; the pure leaves run real. State is injected as a JSON mirror; the oracle
materializes the adapter double, lets live mutate it, then norms it back to JSON.
Compares ordered seam trace + return + final state. BaseException is caught per
case; scripted raises round-trip bare (identity-matched to the Raise instance).

Input: stdin JSON lines from t_port_api_server_flow2.
Summary: ORACLE api_server_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import json
import os
import re
import sys
import time as _real_time
import types
from datetime import datetime, UTC
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "as2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server.py")

# 24 UNCLAIMED targets. is_rate_limited / user_text are _ProviderAuthResolutionError
# methods (driven on a constructed error instance); the rest are APIServerAdapter
# methods or module functions.
METHOD_TARGETS = [
    "mark_shutdown_requested", "_active_structured_run_count", "_resolve_api_server_int",
    "_metrics_day_key", "_iso_timestamp", "_reset_metrics_if_needed",
    "_total_tokens_from_usage", "_latency_p95_ms", "_api_server_status_payload",
    "_publish_runtime_status", "_record_api_metrics", "_heartbeat_loop",
    "_resolve_provider_runtime", "_apply_provider_runtime", "_recover_or_record_model",
    "_select_agent_runtime", "_effective_turn_runtime", "_turn_runtime_metadata",
    "_finish_turn_result", "_handle_health", "_handle_models", "_handle_capabilities",
]
ERROR_METHODS = ["is_rate_limited", "user_text"]
TARGETS = METHOD_TARGETS + ERROR_METHODS
# PURE claimed leaves: run REAL on both sides (C inlines them, no PoP re-claim).
MODULE_LEAVES = ["_clean_request_string", "_apply_runtime_agent_overrides"]
CLASS_LEAVES = ["_clean_runtime_id", "_requested_ids", "_sanitize_runtime_metadata",
                "_result_runtime", "_model_lock_state"]
# EFFECTFUL claimed siblings -> seamed.
SEAMED_MODULE = {"_resolve_request_runtime_agent_kwargs": "resolve_request_runtime_agent_kwargs",
                 "_hermes_version": "hermes_version"}
SEAMED_SELF = ["session_model_override_for", "resolve_model_name"]
ASYNC_TARGETS = {"_heartbeat_loop", "_handle_health", "_handle_models", "_handle_capabilities"}

STATE_DEFAULTS = {
    "host": "127.0.0.1", "port": 8642, "inflight_agent_runs": 0,
    "metrics_day": "", "metrics_requests_today": 0, "metrics_messages_today": 0,
    "metrics_tokens_today": 0, "metrics_last_request_at": None,
    "metrics_last_heartbeat_at": None, "model_name": "hermes-agent",
    "gateway_runner": None, "api_key": "",
}
STATE_OBJ_DEFAULTS = ["run_statuses", "last_resolved_model", "model_routes",
                      "runtime_kwargs", "result"]
STATE_ARR_DEFAULTS = ["metrics_latency_ms", "cors_origins"]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [],
       "self": None, "ns": None}


def rec(ev):
    CUR["trace"].append(ev)


# ── faithful exception double for the typed provider-auth error ──────────────
class _ProviderAuthResolutionError(RuntimeError):
    pass


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "_ProviderAuthResolutionError":
        return _ProviderAuthResolutionError(msg)
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
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
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


def SeamFn(name):
    def f(*a, **kw):
        return seam(name, proj_call(a, kw))
    return f


def proj_call(a, kw):
    """Generic positional+keyword projection for bare-global seams."""
    args = {}
    for i, v in enumerate(a):
        args["a%d" % i] = proj(v)
    for k, v in kw.items():
        args[k] = proj(v)
    return args


def proj(x):
    if isinstance(x, SimpleNamespace):
        return {k: proj(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: proj(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [proj(v) for v in x]
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, type):
        return x.__name__
    return x
def recorder(name, argfn):
    def f(*a, **kw):
        return seam(name, argfn(a, kw))
    return f


def proj_exc(x):
    if x is None:
        return None
    if isinstance(x, BaseException):
        return {"kind": type(x).__name__, "message": str(x)}
    return x


STATIC_TARGETS = {"_resolve_api_server_int", "_metrics_day_key", "_iso_timestamp",
                  "_total_tokens_from_usage", "_latency_p95_ms", "_resolve_provider_runtime"}
CLASSMETHOD_LEAVES = {"_requested_ids", "_sanitize_runtime_metadata"}


class As2Adapter:
    @property
    def is_connected(self):
        return seam("is_connected", {})


def _install_module_stubs(ns):
    # time: seam time.time(), keep real strftime/gmtime/monotonic (C transcribes them).
    tmod = types.ModuleType("time")
    tmod.time = recorder("time.time", lambda a, k: {})
    tmod.strftime = _real_time.strftime
    tmod.gmtime = _real_time.gmtime
    tmod.monotonic = _real_time.monotonic
    sys.modules["time"] = tmod
    ns["time"] = tmod
    # asyncio.sleep seam (async); real asyncio imported already for asyncio.run.
    amod = types.ModuleType("asyncio")
    async def _sleep(seconds):
        return seam("asyncio.sleep", {"seconds": seconds})
    amod.sleep = _sleep
    amod.CancelledError = asyncio.CancelledError
    ns["asyncio"] = amod
    # hermes_cli.config
    cfgmod = types.ModuleType("hermes_cli.config")
    cfgmod.load_config = recorder("config.load_config", lambda a, k: {})
    cfgmod.cfg_get = recorder("config.cfg_get",
        lambda a, k: {"config": proj(a[0]), "key": a[3], "default": k.get("default")})
    _stub_pkg("hermes_cli", "config", cfgmod)
    ns["hermes_cli"] = sys.modules["hermes_cli"]
    # gateway.run fallback
    runmod = types.ModuleType("gateway.run")
    runmod._resolve_runtime_agent_kwargs_for_provider = recorder(
        "run._resolve_runtime_agent_kwargs_for_provider",
        lambda a, k: {"provider": a[0], "target_model": k.get("target_model")})
    _stub_pkg("gateway", "run", runmod)
    # hermes_cli.models
    modelsmod = types.ModuleType("hermes_cli.models")
    modelsmod.get_default_model_for_provider = recorder(
        "models.get_default_model_for_provider", lambda a, k: {"provider": a[0]})
    sys.modules["hermes_cli.models"] = modelsmod
    ns["hermes_cli"].models = modelsmod
    # hermes_cli.model_switch
    msmod = types.ModuleType("hermes_cli.model_switch")
    msmod.resolve_effective_model = recorder("model_switch.resolve_effective_model",
        lambda a, k: {"explicit": proj(a[0]), "profile_name": proj(a[1]), "default": proj(a[2])})
    sys.modules["hermes_cli.model_switch"] = msmod
    ns["hermes_cli"].model_switch = msmod
    # hermes_cli.auth
    authmod = types.ModuleType("hermes_cli.auth")
    authmod.is_rate_limited_auth_error = recorder("auth.is_rate_limited_auth_error",
        lambda a, k: {"cause": proj_exc(a[0])})
    sys.modules["hermes_cli.auth"] = authmod
    ns["hermes_cli"].auth = authmod
    # web (aiohttp)
    webmod = types.ModuleType("aiohttp.web")
    webmod.json_response = recorder("web.json_response",
        lambda a, k: {"data": proj(a[0]), "status": k.get("status"), "headers": proj(k.get("headers"))})
    ns["web"] = webmod
    # _api_runs delegates
    runsmod = types.ModuleType("gateway.platforms.api_server_runs")
    runsmod._mark_shutdown_requested = recorder("runs._mark_shutdown_requested", lambda a, k: {})
    runsmod._idempotency_capabilities = recorder("runs._idempotency_capabilities",
        lambda a, k: {"store_type": proj(k.get("store_type"))})
    ns["_api_runs"] = runsmod
    # request-profile ContextVar
    ns["_api_request_profile"] = SimpleNamespace(get=lambda: seam("request_profile", {}))
    # i18n t() bare global
    ns["t"] = recorder("t", lambda a, k: {"key": a[0], "kwargs": {kk: proj(vv) for kk, vv in k.items()}})
    # claimed module siblings -> seams
    for live, seamname in SEAMED_MODULE.items():
        if live == "_resolve_request_runtime_agent_kwargs":
            ns[live] = recorder(seamname, lambda a, k: {"provider": a[0], "target_model": k.get("target_model")})
        else:
            ns[live] = recorder(seamname, lambda a, k: {})


def _stub_pkg(parent, child, mod):
    if parent not in sys.modules or not isinstance(sys.modules[parent], types.ModuleType):
        p = types.ModuleType(parent)
        p.__path__ = []
        sys.modules[parent] = p
    sys.modules[parent + "." + child] = mod
    setattr(sys.modules[parent], child, mod)


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("api_server_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "json": json, "contextlib": contextlib, "copy": copy, "re": re,
        "suppress": contextlib.suppress, "datetime": datetime, "UTC": UTC,
        "asyncio": asyncio, "Optional": object, "Any": object,
        "ContextVar": __import__("contextvars").ContextVar,
        "RunIdempotencyStore": type("RunIdempotencyStore", (), {}),
        "_ProviderAuthResolutionError": _ProviderAuthResolutionError,
        "logging": SimpleNamespace(getLogger=lambda name: SimpleNamespace()),
    })
    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # module-level pure leaves
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in MODULE_LEAVES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    # the typed error class (lightweight; base RuntimeError)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_ProviderAuthResolutionError":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    # adapter methods + class leaves (strip decorators)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "APIServerAdapter":
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                        m.name in METHOD_TARGETS + CLASS_LEAVES:
                    m2 = copy.copy(m)
                    m2.decorator_list = []
                    exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in METHOD_TARGETS + MODULE_LEAVES + CLASS_LEAVES:
        assert name in ns, "extraction missed %s" % name
    for name in ("_STATIC_FEATURE_FLAGS", "_CAPABILITY_ENDPOINTS", "_RUNTIME_AGENT_OVERRIDE_KEYS",
                 "API_SERVER_LATENCY_SAMPLE_LIMIT", "_ProviderAuthResolutionError"):
        assert name in ns, "constant missed %s" % name
    _install_module_stubs(ns)
    # module-level logger Assign replaced the seeded one; restore a silent logger
    ns["logger"] = SimpleNamespace(
        debug=lambda *a, **k: None, info=lambda *a, **k: None,
        warning=lambda *a, **k: None, exception=lambda *a, **k: None,
        error=lambda *a, **k: None)
    CUR["ns"] = ns
    return ns
def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    stt = inp.get("state") or {}
    state = copy.deepcopy(stt)
    for k, v in STATE_DEFAULTS.items():
        state.setdefault(k, copy.deepcopy(v))
    for k in STATE_OBJ_DEFAULTS:
        if not isinstance(state.get(k), dict):
            state[k] = {}
    for k in STATE_ARR_DEFAULTS:
        if not isinstance(state.get(k), list):
            state[k] = []
    CUR["state"] = state
    CUR["rk"] = state["runtime_kwargs"]
    CUR["result"] = state["result"]

    sd = As2Adapter()
    sd._host = state["host"]
    sd._port = state["port"]
    sd._inflight_agent_runs = state["inflight_agent_runs"]
    sd._run_statuses = state["run_statuses"]
    sd._metrics_day = state["metrics_day"]
    sd._metrics_requests_today = state["metrics_requests_today"]
    sd._metrics_messages_today = state["metrics_messages_today"]
    sd._metrics_tokens_today = state["metrics_tokens_today"]
    sd._metrics_latency_ms = state["metrics_latency_ms"]
    sd._metrics_last_request_at = state["metrics_last_request_at"]
    sd._metrics_last_heartbeat_at = state["metrics_last_heartbeat_at"]
    sd._model_name = state["model_name"]
    sd._last_resolved_model = state["last_resolved_model"]
    sd._model_routes = state["model_routes"]
    sd._cors_origins = tuple(state["cors_origins"])
    sd._api_key = state["api_key"]
    gr = state["gateway_runner"]
    sd.gateway_runner = SimpleNamespace(**copy.deepcopy(gr)) if isinstance(gr, dict) else gr
    for nm in METHOD_TARGETS:
        if nm in STATIC_TARGETS:
            setattr(sd, nm, ns[nm])
        else:
            setattr(sd, nm, types.MethodType(ns[nm], sd))
    for nm in CLASS_LEAVES:
        if nm in CLASSMETHOD_LEAVES:
            setattr(sd, nm, types.MethodType(ns[nm], sd))
        else:
            setattr(sd, nm, ns[nm])
    sd._session_model_override_for = recorder("session_model_override_for",
        lambda a, k: {"session_key": a[0]})
    sd._resolve_model_name = recorder("resolve_model_name", lambda a, k: {"explicit": a[0]})
    sd._write_runtime_status_safe = recorder("self._write_runtime_status_safe",
        lambda a, k: {"context": a[0], "platform_state": k.get("platform_state"),
                      "platform_metrics": proj(k.get("platform_metrics"))})
    CUR["self"] = sd


def norm_state():
    sd = CUR["self"]
    return {
        "host": sd._host, "port": sd._port,
        "inflight_agent_runs": sd._inflight_agent_runs,
        "run_statuses": sd._run_statuses, "metrics_day": sd._metrics_day,
        "metrics_requests_today": sd._metrics_requests_today,
        "metrics_messages_today": sd._metrics_messages_today,
        "metrics_tokens_today": sd._metrics_tokens_today,
        "metrics_latency_ms": sd._metrics_latency_ms,
        "metrics_last_request_at": sd._metrics_last_request_at,
        "metrics_last_heartbeat_at": sd._metrics_last_heartbeat_at,
        "model_name": sd._model_name, "last_resolved_model": sd._last_resolved_model,
        "model_routes": sd._model_routes, "cors_origins": list(sd._cors_origins),
        "gateway_runner": proj(sd.gateway_runner) if sd.gateway_runner is not None else None,
        "api_key": sd._api_key,
        "runtime_kwargs": CUR["rk"], "result": CUR["result"],
    }


def mat_agent(d):
    return SimpleNamespace(**copy.deepcopy(d)) if isinstance(d, dict) else d


def build_cause(c):
    if not isinstance(c, dict):
        return None
    kind = c.get("kind", "Exception")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    e = cls(c.get("message", ""))
    e.__cause__ = build_cause(c.get("cause"))
    return e


def build_error(spec):
    spec = spec or {}
    err = CUR["ns"]["_ProviderAuthResolutionError"](spec.get("message", ""))
    err.__cause__ = build_cause(spec.get("cause"))
    return err


def drive(ns, func, inp):
    sd = CUR["self"]
    g = inp.get
    if func in ERROR_METHODS:
        return getattr(build_error(g("error")), func)()
    if func in ASYNC_TARGETS:
        if func == "_heartbeat_loop":
            return asyncio.run(sd._heartbeat_loop())
        return asyncio.run(getattr(sd, func)(None))
    if func == "mark_shutdown_requested":
        return sd.mark_shutdown_requested()
    if func == "_active_structured_run_count":
        return sd._active_structured_run_count()
    if func == "_resolve_api_server_int":
        return sd._resolve_api_server_int(g("key"), default=g("default"))
    if func == "_metrics_day_key":
        return sd._metrics_day_key(g("timestamp"))
    if func == "_iso_timestamp":
        return sd._iso_timestamp(g("timestamp"))
    if func == "_reset_metrics_if_needed":
        return sd._reset_metrics_if_needed()
    if func == "_total_tokens_from_usage":
        return sd._total_tokens_from_usage(g("usage"))
    if func == "_latency_p95_ms":
        return sd._latency_p95_ms(g("samples"))
    if func == "_api_server_status_payload":
        return sd._api_server_status_payload(heartbeat_at=g("heartbeat_at"))
    if func == "_publish_runtime_status":
        return sd._publish_runtime_status()
    if func == "_record_api_metrics":
        return sd._record_api_metrics(g("usage"), g("latency_seconds"))
    if func == "_resolve_provider_runtime":
        return sd._resolve_provider_runtime(g("provider"), target_model=g("target_model"),
                                            required=g("required", False))
    if func == "_apply_provider_runtime":
        return sd._apply_provider_runtime(CUR["rk"], g("provider"), target_model=g("target_model"),
                                          required=g("required", False))
    if func == "_recover_or_record_model":
        return sd._recover_or_record_model(g("model"), CUR["rk"], g("gateway_session_key"))
    if func == "_select_agent_runtime":
        _gsk = g("gateway_session_key"); _sid = g("session_id")
        _rm = g("requested_model"); _rp = g("requested_provider"); _rt = g("route")
        _sm = g("session_model"); _crl = g("confirmed_runtime_lock", False)
        return sd._select_agent_runtime(CUR["rk"], g("model"), requested_model=_rm,
            requested_provider=_rp, route=_rt, session_model=_sm, confirmed_runtime_lock=_crl,
            gateway_session_key=_gsk, session_id=_sid)
    if func == "_effective_turn_runtime":
        return sd._effective_turn_runtime(g("runtime_request"), g("result"), g("usage"))
    if func == "_turn_runtime_metadata":
        return sd._turn_runtime_metadata(
            mat_agent(g("agent")), route=g("route"), requested_runtime=g("requested_runtime"),
            route_source=g("route_source", ""), confirmed_runtime_lock=g("confirmed_runtime_lock", False))
    if func == "_finish_turn_result":
        return sd._finish_turn_result(
            mat_agent(g("agent")), CUR["result"], g("session_id"), route=g("route"),
            requested_runtime=g("requested_runtime"), route_source=g("route_source", ""),
            confirmed_runtime_lock=g("confirmed_runtime_lock", False))
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
    print("ORACLE api_server_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())