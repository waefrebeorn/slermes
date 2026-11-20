#!/usr/bin/env python3
"""Oracle for auxiliary_client_flow3 (UNCLAIMED guard/accumulator leaves).

Reads harness JSONL on stdin, drives LIVE agent/auxiliary_client.py under the
same seam scripts, diffs {"ret","trace","state"}, prints
    ORACLE auxiliary_client_flow3: N cases, M mismatches

Ported targets: the 19 strict-PoP-unclaimed guard/accumulator leaves named in
src/agent/auxiliary_client_flow3.c. All collaborators are scripted seams; ported
methods compose on the injected `self` carrier. Domain notes in the C header.
"""
import ast
import builtins
import contextlib
import __future__
import json
import math
import sys
import types
from pathlib import Path
from typing import Any, Callable, Optional

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
SRC = str(QUARRY / "agent" / "auxiliary_client.py")
NAME = "auxiliary_client_flow3"

GUARD_METHODS = [
    "effective_deadline", "cancel_requested", "adopt_stream", "release_stream",
    "close_attempt_stream", "record_progress", "timeout_message",
    "_close_client_on_timeout", "check_cancelled", "_watchdog_fire", "_arm_timer",
    "start", "on_event",
]
ACC_METHODS = ["_check_deadlines", "_feed_reasoning_details", "_feed_tool_calls"]
LEAVES = ["__getattr__", "remember_temperature_rejection", "_get_provider_chain"]
PORTED = GUARD_METHODS + ACC_METHODS + LEAVES
WANTED = set(PORTED)
GUARD_CLASSES = {"_CodexStreamGuard": GUARD_METHODS,
                 "_ChatStreamAccumulator": ACC_METHODS,
                 "_AuxProbeClientStub": ["__getattr__"]}

EVENT_FIELDS = {"saw_content", "timed_out", "timeout_release_pending", "stream_finished"}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


METHOD_CODE = {}
DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name in GUARD_CLASSES:
        for _sub in _node.body:
            if isinstance(_sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and _sub.name in WANTED:
                METHOD_CODE[_sub.name] = _compile_node(_sub, SRC)
_missing = WANTED - (set(DEF_CODE) | set(METHOD_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
class CallableMarker:
    """A callable double that records a seam and projects to its marker string."""
    def __init__(self, marker, seam, begin=False):
        self.marker = marker
        self.seam = seam
        self.has_begin = begin
        if begin:
            self.begin_timeout_cleanup = CallableMarker(marker + "_begin",
                                                        "begin_timeout_cleanup")

    def __call__(self, *a, **kw):
        return record(self.seam, {})


class HostDeadlineMarker:
    def __init__(self, marker):
        self.marker = marker

    def __call__(self, *a, **kw):
        return record("host_deadline", {})


class EventDouble:
    def __init__(self, d):
        self.d = d

    def is_set(self):
        return bool(self.d.get("set", False))

    def set(self):
        self.d["set"] = True

    def clear(self):
        self.d["set"] = False


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class TimerDouble:
    def __init__(self, d):
        self.d = dict(d) if isinstance(d, dict) else {}
        self.daemon = True

    def start(self):
        return record("timer.start", {})


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, CallableMarker):
        return v.marker
    if isinstance(v, HostDeadlineMarker):
        return v.marker
    if isinstance(v, EventDouble):
        return {"set": bool(v.d.get("set", False))}
    if isinstance(v, TimerDouble):
        return norm(v.d)
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.counters = {}

    def next(self, name):
        if name not in self.io:
            return None
        sc = self.io[name]
        if isinstance(sc, dict) and isinstance(sc.get("$seq"), list):
            i = self.counters.get(name, 0)
            seq = sc["$seq"]
            self.counters[name] = i + 1
            return seq[i] if i < len(seq) else {"__raise__": "RuntimeError",
                                                "msg": "script exhausted: " + name}
        return sc


def materialize(name, script):
    if name == "threading.Timer":
        return TimerDouble(script if isinstance(script, dict) else {})
    return script


SEAM_ARGS = {
    "time.monotonic": lambda a, k: {},
    "captured_aux_cancel_requested": lambda a, k: {"check": a[0]},
    "close_quietly": lambda a, k: {"stream": a[0], "note": a[1] if len(a) > 1 else None},
    "threading.get_ident": lambda a, k: {},
    "attempt_stream_socket": lambda a, k: {"stream": a[0]},
    "shutdown_socket": lambda a, k: {"socket": a[0]},
    "force_close_tcp_sockets": lambda a, k: {"client": a[0]},
    "evict_cached_client_instance": lambda a, k: {"client": a[0]},
    "begin_timeout_cleanup": lambda a, k: {},
    "threading.Timer": lambda a, k: {"delay": a[0]},
    "timer.start": lambda a, k: {},
    "is_interrupted": lambda a, k: {},
    "aux_interrupt_cancel_requested": lambda a, k: {},
    "aux_interrupt_protected": lambda a, k: {},
    "codex_event_has_content": lambda a, k: {"event": a[0]},
    "notify_aux_provider_response": lambda a, k: {},
    "notify_aux_timing_response": lambda a, k: {},
    "host_deadline": lambda a, k: {},
    "route_key": lambda a, k: {"provider": a[0], "base_url": a[1]},
    "bare_model": lambda a, k: {"model": a[0]},
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": [norm(x) for x in args]}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)


_LOGGER_SINGLETON = LoggerDouble()


class ThreadingDouble:
    def __init__(self):
        self.get_ident = SeamFn("threading.get_ident")

    def Timer(self, delay, callback):
        return SeamFn("threading.Timer")(delay)


_THREADING_SINGLETON = ThreadingDouble()


class TimeDouble:
    def monotonic(self):
        return SeamFn("time.monotonic")()


_TIME_SINGLETON = TimeDouble()


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs():
    agent = _mod("agent")
    agent.__path__ = []
    ahr = _mod("agent.agent_runtime_helpers",
               _shutdown_socket=SeamFn("shutdown_socket"),
               force_close_tcp_sockets=SeamFn("force_close_tcp_sockets"))
    agent.agent_runtime_helpers = ahr
    tools = _mod("tools")
    tools.__path__ = []
    ti = _mod("tools.interrupt", is_interrupted=SeamFn("is_interrupted"))
    tools.interrupt = ti


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "ac3_live_auxiliary_client",
    "__builtins__": builtins,
    "contextlib": contextlib,
    "threading": _THREADING_SINGLETON,
    "time": _TIME_SINGLETON,
    "logging": types.SimpleNamespace(getLogger=lambda *a: _LOGGER_SINGLETON),
    "logger": _LOGGER_SINGLETON,
    "json": json,
    "Any": Any, "Optional": Optional, "Callable": Callable,
    "CODEX_STREAM_STALL_MARKER": "stream stalled",
    "_REASONING_DETAIL_TEXT_FIELDS": ("summary", "thinking", "content", "text"),
}
# custom exception family (BaseException, NOT Exception)
class AuxiliaryExplicitCancellation(BaseException):
    pass
_BASE_NS["AuxiliaryExplicitCancellation"] = AuxiliaryExplicitCancellation

_MOD = types.ModuleType("ac3_live_auxiliary_client")
_MOD.__dict__.update(_BASE_NS)
sys.modules["ac3_live_auxiliary_client"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in list(METHOD_CODE.items()) + list(DEF_CODE.items()):
    exec(_code, _BASE_NS)

# bare-global collaborators (module-top imports or lazy-imported names)
for _nm in ("_captured_aux_cancel_requested", "_close_quietly", "_attempt_stream_socket",
            "_codex_event_has_content", "_notify_aux_provider_response",
            "_notify_aux_timing_response", "_aux_interrupt_cancel_requested",
            "_aux_interrupt_protected", "_route_key", "_bare_model"):
    _BASE_NS[_nm] = SeamFn(_nm if _nm != "_captured_aux_cancel_requested"
                            else "captured_aux_cancel_requested")
# fix seam names that drop leading underscore (matching the C seam names)
_BASE_NS["_captured_aux_cancel_requested"] = SeamFn("captured_aux_cancel_requested")
_BASE_NS["_close_quietly"] = SeamFn("close_quietly")
_BASE_NS["_attempt_stream_socket"] = SeamFn("attempt_stream_socket")
_BASE_NS["_codex_event_has_content"] = SeamFn("codex_event_has_content")
_BASE_NS["_notify_aux_provider_response"] = SeamFn("notify_aux_provider_response")
_BASE_NS["_notify_aux_timing_response"] = SeamFn("notify_aux_timing_response")
_BASE_NS["_aux_interrupt_cancel_requested"] = SeamFn("aux_interrupt_cancel_requested")
_BASE_NS["_aux_interrupt_protected"] = SeamFn("aux_interrupt_protected")
_BASE_NS["_route_key"] = SeamFn("route_key")
_BASE_NS["_bare_model"] = SeamFn("bare_model")
_BASE_NS["_evict_cached_client_instance"] = SeamFn("evict_cached_client_instance")
# provider-chain function references project to marker strings
for _nm in ("_try_openrouter", "_try_nous", "_try_custom_endpoint", "_resolve_api_key_provider"):
    _BASE_NS[_nm] = _nm
install_stubs()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    routes = ((inp.get("self") or {}).get("routes")) or []
    _BASE_NS["_TEMPERATURE_REJECTED_ROUTES"] = {tuple(x) for x in routes}
    return _BASE_NS, trace, scripts


# ── self-carrier materialization / projection ──────────────────────────────
def mat_val(k, v):
    if k in EVENT_FIELDS and isinstance(v, dict):
        return EventDouble(dict(v))
    if k in ("_deadline_lock", "_attempt_stream_lock"):
        return NoopLock()
    if k == "_protected_cancel_check" and isinstance(v, dict):
        return CallableMarker(v.get("__callable__", "cc"), "begin_timeout_cleanup",
                              begin=bool(v.get("begin", False)))
    if k == "_host_deadline" and isinstance(v, dict) and "__callable__" in v:
        return HostDeadlineMarker(v["__callable__"])
    if k == "_prompt_reading" and isinstance(v, dict):
        return types.SimpleNamespace(**v)
    if k == "tool_calls_acc" and isinstance(v, dict):
        return {int(kk): vv for kk, vv in v.items()}
    return v


def mat_self(d, methods):
    o = types.SimpleNamespace()
    keys = []
    for k, v in (d or {}).items():
        keys.append(k)
        setattr(o, k, mat_val(k, v))
    for m in methods:
        setattr(o, m, types.MethodType(_BASE_NS[m], o))
    return o, keys


def proj_val(k, v):
    if isinstance(v, NoopLock):
        return {}
    if isinstance(v, EventDouble):
        return {"set": bool(v.d.get("set", False))}
    if isinstance(v, CallableMarker):
        d = {"__callable__": v.marker}
        if v.has_begin:
            d["begin"] = True
        return d
    if isinstance(v, HostDeadlineMarker):
        return {"__callable__": v.marker}
    if isinstance(v, TimerDouble):
        return norm(v.d)
    if isinstance(v, types.SimpleNamespace):
        return {kk: proj_val(kk, x) for kk, x in vars(v).items()}
    if isinstance(v, dict):
        return {(str(kk) if k == "tool_calls_acc" else kk): proj_val(k, x)
                for kk, x in v.items()}
    if isinstance(v, list):
        return [proj_val(k, x) for x in v]
    return norm(v)


def proj_self(o, keys):
    return {k: proj_val(k, getattr(o, k)) for k in keys}


def mat_delta(d):
    o = types.SimpleNamespace()
    d = d or {}
    o.reasoning_details = d.get("reasoning_details")
    o.model_extra = d.get("model_extra")
    tcs = d.get("tool_calls")
    if isinstance(tcs, list):
        arr = []
        for tc in tcs:
            if isinstance(tc, dict):
                t = types.SimpleNamespace()
                for k, v in tc.items():
                    if k == "function" and isinstance(v, dict):
                        setattr(t, k, types.SimpleNamespace(**v))
                    else:
                        setattr(t, k, v)
                arr.append(t)
            else:
                arr.append(tc)
        o.tool_calls = arr
    else:
        o.tool_calls = tcs
    return o


# ── drive ───────────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    sd = inp.get("self") or {}

    if func in GUARD_METHODS:
        o, keys = mat_self(sd, GUARD_METHODS)
        holder["_self"] = (o, keys)
        fn = ns[func]
        if func in ("adopt_stream", "release_stream"):
            return fn(o, inp.get("stream"))
        if func == "close_attempt_stream":
            return fn(o, inp.get("failure_note", ""))
        if func == "_arm_timer":
            return fn(o, inp.get("delay", 0))
        if func == "on_event":
            return fn(o, inp.get("event"))
        return fn(o)

    if func in ACC_METHODS:
        o, keys = mat_self(sd, ACC_METHODS)
        holder["_self"] = (o, keys)
        if func in ("_feed_reasoning_details", "_feed_tool_calls"):
            return ns[func](o, mat_delta(inp.get("delta")))
        return ns[func](o)

    if func == "__getattr__":
        stub = types.SimpleNamespace()
        for k, v in sd.items():
            setattr(stub, k, v)
        return ns["__getattr__"](stub, inp.get("name", ""))

    if func == "remember_temperature_rejection":
        o, keys = mat_self(sd, [])
        holder["_self"] = (o, keys)
        ns[func](inp.get("provider", ""), inp.get("base_url", ""),
                 inp.get("rejected_kwargs") or {}, RuntimeError("ignored"))
        o.routes = ns["_TEMPERATURE_REJECTED_ROUTES"]
        if "routes" not in keys:
            keys.append("routes")
        return None

    if func == "_get_provider_chain":
        return ns[func]()

    raise KeyError(f"unknown func {func}")


def state_of(holder, raised):
    if raised:
        return {}
    if "_self" in holder:
        o, keys = holder["_self"]
        return proj_self(o, keys)
    return {}


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        raised = False
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace,
                       "state": state_of(holder, False)}
            except BaseException as e:
                raised = True
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": {}}
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


def selftest(fixtures):
    bad = 0
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                drive(rec, ns, scripts, holder)
            except BaseException as e:
                if type(e).__name__ == "AttributeError":
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError: {e}")
        except BaseException as e:
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "auxiliary_client_flow3.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
