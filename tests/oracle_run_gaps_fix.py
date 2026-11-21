#!/usr/bin/env python3
"""Leaf differential oracle for the in-place re-port of run_gaps.c placeholders.

Covered (real body re-ported, LIVE Python truth replayed with recording stubs):
  start_startup_warmup        <- gateway/run_startup.py::_start_startup_warmup
  ensure_hosted_room_worker   <- gateway/run_startup.py::_ensure_hosted_room_worker
  stop_hosted_room_worker     <- gateway/run_startup.py::_stop_hosted_room_worker
  hosted_room_worker_watcher  <- gateway/run_startup.py::_hosted_room_worker_watcher

The C harness (tests/t_port_run_gaps_fix.c) links the object src/gateway/run_gaps.o
and passes its grg_seams_t context as the void* "gateway".  Both sides drive the
same in.io scripts; the ORACLE DIFFS THE ORDERED SEAM-CALL TRACE (event name +
args + scripted return).  Script shapes: plain value (reused), {"__seq__":[...]}
(consumed per call, last repeats), {"__raise__":k,"msg":m} (raised), absent/None.

Input: harness JSONL on stdin.  Summary: ORACLE run_gaps_fix: N cases, M mismatches.
"""
import ast
import json
import os
import sys
import types
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise _make_exc(v)
    return v


def _make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    builtins = {
        "RuntimeError": RuntimeError, "TypeError": TypeError, "ValueError": ValueError,
        "AttributeError": AttributeError, "OSError": OSError,
    }
    return builtins.get(kind, type(kind, (Exception,), {}))(msg)


def script(name, args):
    """Consume the case script for a seam and record the event (C twin)."""
    if name.startswith("param."):
        sc = (CUR["in"].get("io") or {}).get(name)
        return sc
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
    CUR["trace"].append({"ev": name, "args": args, "ret": val})
    return val


class RecLogger:
    def _rec(self, name, fmt, a, k, level=None):
        args = {"fmt": fmt}
        if level is not None:
            args["level"] = level
        if a:
            args["args"] = list(a)
        if k.get("exc_info"):
            args["exc_info"] = True
        return materialize(script(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", fmt, a, k)

    def log(self, level, fmt, *a, **k):
        return self._rec("logger.log", fmt, a, k, level)


class AwaitVal:
    """Immediately-completed awaitable (no real suspension)."""
    def __init__(self, v):
        self.v = v

    def __await__(self):
        if False:
            yield
        return self.v


def _named(name):
    o = types.SimpleNamespace()
    o.__name__ = name
    return o


def _call_args(fn, a, k):
    d = {"fn": getattr(fn, "__name__", str(fn))}
    for x in a:
        d.setdefault("pos", []).append(x) if False else None
    d.update(k)
    return d


class AsyncioDouble:
    CancelledError = __import__("asyncio").CancelledError

    @staticmethod
    def ensure_future(x):
        return materialize(script("asyncio.ensure_future", {"fn": getattr(x, "__name__", "coroutine")}))

    @staticmethod
    def to_thread(fn, *a, **k):
        return AwaitVal(materialize(script("asyncio.to_thread", _call_args(fn, a, k))))

    @staticmethod
    def sleep(s):
        return AwaitVal(materialize(script("asyncio.sleep", {"seconds": s})))

    @staticmethod
    def create_task(coro, **k):
        return materialize(script("asyncio.create_task", {"fn": getattr(coro, "__name__", "coroutine")}))


class RunnerDouble:
    """Minimal 'self' carrying only the attrs the live bodies touch."""
    def __init__(self):
        object.__setattr__(self, "_store", {})

    @property
    def session_authority(self):
        return materialize(script("getattr", {"obj": "self", "name": "session_authority"}))

    @property
    def _running(self):
        return materialize(script("getattr", {"obj": "self", "name": "_running"}))

    def _warm_turn_prerequisites(self):
        return _named("_warm_turn_prerequisites")

    def _start_hosted_room_worker_sync(self):
        return _named("_start_hosted_room_worker_sync")

    def _ensure_hosted_room_worker(self):
        return AwaitVal(materialize(script("_ensure_hosted_room_worker", {})))

    def __setattr__(self, name, value):
        materialize(script("setattr", {"obj": "self", "name": name, "value": value}))
        object.__getattribute__(self, "_store")[name] = value


def extract(rel, methods=(), pre=None):
    """AST-extract selected class methods from the LIVE quarry source."""
    path = os.path.join(QUARRY, rel)
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "rgf_" + rel.replace("/", "_").replace(".", "_"), "__file__": path}
    if pre:
        ns.update(pre)
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in methods:
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), path, "exec", flags=flags), ns)
    return ns


NS = extract(
    "gateway/run_startup.py",
    methods=("_start_startup_warmup", "_ensure_hosted_room_worker",
             "_stop_hosted_room_worker", "_hosted_room_worker_watcher"),
    pre={"logger": RecLogger(), "asyncio": AsyncioDouble,
         "logging": types.SimpleNamespace(WARNING=30, DEBUG=10, INFO=20, ERROR=40)},
)


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _startup_warmup_timeout_secs():
    return materialize(script("_startup_warmup_timeout_secs", {}))


def ensure_hosted_service(self):
    return AwaitVal(materialize(script("ensure_hosted_service", {})))


def stop_hosted_service(self, timeout=5.0):
    return AwaitVal(materialize(script("stop_hosted_service", {"timeout": timeout})))


def stop_hosted_room_service(timeout=5.0):
    return materialize(script("stop_hosted_room_service_builtin", {"timeout": timeout}))


M_GW = mod("gateway")
M_GW.run = mod("gateway.run", _startup_warmup_timeout_secs=_startup_warmup_timeout_secs)
M_GW.session_hosted_service = mod(
    "gateway.session_hosted_service",
    ensure_hosted_service=ensure_hosted_service, stop_hosted_service=stop_hosted_service)
M_TUI = mod("tui_gateway")
M_TUI.methods_groups = mod("tui_gateway.methods_groups", stop_hosted_room_service=stop_hosted_room_service)
M_TUI.server = mod("tui_gateway.server")


def run_coro(coro):
    try:
        coro.send(None)
    except StopIteration as si:
        return si.value
    raise RuntimeError("live coroutine suspended unexpectedly")


DISPATCH = {
    "start_startup_warmup": ("fn", "_start_startup_warmup"),
    "ensure_hosted_room_worker": ("coro", "_ensure_hosted_room_worker"),
    "stop_hosted_room_worker": ("coro", "_stop_hosted_room_worker"),
    "hosted_room_worker_watcher": ("coro", "_hosted_room_worker_watcher"),
}


def drive(func, in_data):
    CUR["func"] = func
    CUR["in"] = in_data
    CUR["trace"] = []
    SEQPOS.clear()
    NS["logger"] = RecLogger()
    self = RunnerDouble()
    kind, live_name = DISPATCH[func]
    live = NS[live_name]
    if kind == "fn":
        live(self)
    else:
        run_coro(live(self))
    return CUR["trace"]


def main():
    total = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec["func"]
        if func not in DISPATCH:
            continue
        total += 1
        in_data = rec["in"]
        live_trace = drive(func, in_data)
        c_trace = rec["out"]["trace"]
        # Normalize integral floats so JSON 30.0 vs 30 compare equal.
        def norm(x):
            if isinstance(x, float) and x.is_integer():
                return int(x)
            if isinstance(x, dict):
                return {k: norm(v) for k, v in x.items()}
            if isinstance(x, list):
                return [norm(v) for v in x]
            return x
        if norm(live_trace) != norm(c_trace):
            mism += 1
            print("MISMATCH %s case=%s" % (func, json.dumps(in_data, ensure_ascii=False)))
            print("  C    :", json.dumps(c_trace, ensure_ascii=False))
            print("  LIVE :", json.dumps(live_trace, ensure_ascii=False))
    print("ORACLE run_gaps_fix: %d cases, %d mismatches" % (total, mism))
    return 0 if (total > 0 and mism == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
