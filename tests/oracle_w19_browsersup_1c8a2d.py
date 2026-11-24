#!/usr/bin/env python3
"""Differential oracle for tools/browser_supervisor_capture.py (gate w19_browsersup_1c8a2d).

The live CapturedCDP/capture() are driven with fake supervisor, registry and loop objects; the
async dispatch is stubbed so every admission branch is reachable without a browser.
"""
import ast
import asyncio
import concurrent.futures
import inspect
import json
import math
import sys
import threading
import time
import types
import __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/tools/browser_supervisor_capture.py"
GATE = "w19_browsersup_1c8a2d"
_ANN = __future__.annotations.compiler_flag

CUR = {"sched_unavail": False}


class _LoopUnavailable(Exception):
    pass


def _schedule(coro, loop, timeout=10.0):
    if CUR["sched_unavail"]:
        coro.close()
        raise _LoopUnavailable()
    return asyncio.run(coro)


class FakeFut:
    def result(self, timeout=None):
        raise concurrent.futures.TimeoutError()

    def cancel(self):
        pass


def _safe_schedule(coro, loop):
    coro.close()
    return FakeFut()


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


_mk("agent")
_mk("agent.async_utils", safe_schedule_threadsafe=_safe_schedule)
_mk("tools")
_mk("tools.browser_supervisor", _LoopUnavailable=_LoopUnavailable, _schedule=_schedule)


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    g = {"__name__": "bsc_live", "asyncio": asyncio, "concurrent": __import__("concurrent"),
         "inspect": inspect, "math": math, "threading": threading, "time": time,
         "Any": object, "Callable": object, "Optional": object, "TYPE_CHECKING": False,
         "__builtins__": __builtins__}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef, ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
CLS = G["CapturedCDP"]


class FakeLoop:
    def __init__(self, running=True):
        self.running = running

    def is_running(self):
        return self.running

    def is_closed(self):
        return False


class FakeSup:
    def __init__(self, loop=None):
        self.task_id = "t1"
        self.cdp_url = "ws://cdp/1"
        self._page_session_id = "page-1"
        self._ws = object()
        self._active = True
        self._stop_requested = False
        self._loop = loop if loop is not None else FakeLoop()


class FakeReg:
    def __init__(self, sup):
        self.sup = sup

    def get(self, _tid):
        return self.sup


def _outcome(fn):
    try:
        fn()
        return {"error": None, "message": None}
    except BaseException as e:
        return {"error": type(e).__name__, "message": str(e)}


def run_case(r):
    f = r.get("func")
    if f == "invalid_message":
        sup = FakeSup()
        sup.task_id = r.get("task_id", "t1")
        cap = CLS(sup, FakeReg(sup))
        return str(cap._invalid())
    if f == "is_valid":
        sup = FakeSup()
        sup._active = bool(r.get("active", True))
        sup._stop_requested = bool(r.get("stop_requested", False))
        sup._loop = FakeLoop(running=bool(r.get("loop_running", True)))
        reg = FakeReg(sup)
        cap = CLS(sup, reg)
        if not r.get("ws_same", True):
            sup._ws = object()
        if not r.get("registry_same", True):
            reg.sup = FakeSup()
        return cap.is_valid()
    if f == "call_gate":
        timeout = r.get("timeout", 10.0)
        bs_kind = r.get("before_send", "none")
        before_send = None if bs_kind == "none" else (lambda: None) if bs_kind == "callable" else 42
        method = r.get("method", "Runtime.evaluate")

        if r.get("loop_misuse"):

            async def _main():
                loop = asyncio.get_running_loop()
                sup = FakeSup(loop=loop)
                sup.task_id = r.get("task_id", "t1")
                cap = CLS(sup, FakeReg(sup))
                return _outcome(lambda: cap.call(method, timeout=timeout, before_send=before_send))

            return asyncio.run(_main())

        sup = FakeSup()
        sup.task_id = r.get("task_id", "t1")
        reg = FakeReg(sup)
        cap = CLS(sup, reg)
        if not r.get("valid", True):
            sup._active = False
        return _outcome(lambda: cap.call(method, timeout=timeout, before_send=before_send))
    if f == "capture_error":
        sup = FakeSup()
        sup._loop = FakeLoop(running=bool(r.get("loop_running", True)))
        sup._active = bool(r.get("active", True))
        sup._page_session_id = "page-1" if r.get("page_present", True) else ""
        if not r.get("ws_present", True):
            sup._ws = None
        reg = FakeReg(sup if r.get("found", True) else None)
        CUR["sched_unavail"] = bool(r.get("schedule_unavailable"))
        try:
            got = G["capture"](reg, r.get("task_id", "t1"))
            return {"error": None}
        except BaseException as e:
            return {"error": str(e)}
        finally:
            CUR["sched_unavail"] = False
        return {"error": None} if got else {"error": None}
    return "UNKNOWN_FUNC"


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
