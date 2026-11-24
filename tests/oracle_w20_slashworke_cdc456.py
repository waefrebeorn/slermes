#!/usr/bin/env python3
"""Differential oracle for tui_gateway/slash_worker_client.py (w20_slashworke_cdc456).

The live module is imported as ``tui_gateway.slash_worker_client`` against
stubbed package siblings (method_ctx, hermes_constants, process_identity,
hermes_cli._startup_fast), then each harness line is replayed through the real
``_prepend_tool_paths``, ``_SlashWorker.run`` (a scripted message queue) and
``_stop_worker_group`` (a scripted process).
"""
import importlib.util
import json
import os
import queue
import signal
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


STATE = {}


def _release(pgid, proc=None):
    STATE.setdefault("released", []).append(pgid)


def _load(name, path):
    sp = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(sp)
    sys.modules[name] = mod
    sp.loader.exec_module(mod)
    return mod


_mk("tui_gateway")
_mk("tui_gateway.method_ctx", bind_module=lambda globals_, server: None)
_mk("hermes_constants", get_hermes_home=lambda: STATE["home"])
_mk("hermes_cli", __path__=[])
_mk("hermes_cli.process_identity", spawn_owned=lambda *a, **k: None,
    release_process_group_once_empty=_release)
_mk("hermes_cli._startup_fast", project_root_str=lambda: ".")
_mk("pm", store_first_path=lambda path: "PM:" + path)

NAME = "tui_gateway.slash_worker_client"
live = _load(NAME, os.path.join(Q, "tui_gateway", "slash_worker_client.py"))


class _P:
    """Path stand-in: only the joins this module performs."""

    def __init__(self, value):
        self.value = value

    def __truediv__(self, other):
        return _P(self.value + "\\" + str(other))

    def __str__(self):
        return self.value

    @property
    def parent(self):
        return _P(self.value.rsplit("\\", 1)[0] if "\\" in self.value else ".")


class _PathShim:
    def __init__(self, home):
        self.home_dir = home

    def __call__(self, value):
        return _P(str(value))

    def home(self):
        return _P(self.home_dir)


def _prepend_expected(r):
    home = r.get("managed_home")
    if r.get("managed_error"):
        def boom():
            raise OSError("no home")
        live.get_hermes_home = boom
    else:
        live.get_hermes_home = lambda: home
    live.Path = _PathShim(r.get("home_dir", "HD"))
    live.sys = types.SimpleNamespace(executable=r.get("executable", "V\\python.exe"))
    if r.get("store_first"):
        sys.modules["pm"].store_first_path = lambda path: "PM:" + path
    else:
        sys.modules.pop("pm", None)
    env = {"PATH": r.get("env_path", "")}
    try:
        out = live._prepend_tool_paths(dict(env))
    finally:
        _mk("pm", store_first_path=lambda path: "PM:" + path)
    return {"path": out["PATH"], "store_first": bool(r.get("store_first"))}


class _Proc:
    def __init__(self, alive):
        self.alive = alive
        self.pid = 4242
        self.stdin = types.SimpleNamespace(write=lambda s: None, flush=lambda: None)
        self.stdout = None
        self.stderr = None

    def poll(self):
        return None if self.alive else 0

    def terminate(self):
        STATE.setdefault("signals", []).append("terminate")

    def kill(self):
        STATE.setdefault("signals", []).append("kill")

    def wait(self, timeout=None):
        return 0


def _run_expected(r):
    worker = object.__new__(live._SlashWorker)
    worker._lock = threading.Lock()
    worker._seq = r.get("seq", 0)
    worker.stdout_queue = queue.Queue()
    for msg in r.get("messages") or []:
        worker.stdout_queue.put(msg)
    worker.stderr_tail = list(r.get("stderr_tail") or [])
    worker.proc = _Proc(alive=not r.get("exited"))
    live._SLASH_WORKER_TIMEOUT_S = 0.01
    try:
        output = worker.run(r.get("command", "/help"))
    except Exception as exc:  # noqa: BLE001
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    return {"status": "ok", "output": output, "seed": getattr(worker, "_last_seed", "")}


def _stop_expected(r):
    STATE.clear()
    proc = _Proc(alive=bool(r.get("alive")))
    real_os = live.os
    real_signal = live.signal
    live.signal = types.SimpleNamespace(SIGTERM=15, SIGKILL=9)
    live.os = types.SimpleNamespace(
        name="posix" if r.get("posix") else "nt",
        killpg=lambda pgid, sig: STATE.setdefault("signals", []).append(
            "SIGTERM" if sig == 15 else "SIGKILL"))
    try:
        live._stop_worker_group(proc)
    finally:
        live.os = real_os
        live.signal = real_signal
    return {"signals": STATE.get("signals", []), "released": bool(r.get("posix"))}


def expected(r):
    f = r.get("func")
    if f == "prepend_tool_paths":
        return _prepend_expected(r)
    if f == "run_reply":
        return _run_expected(r)
    if f == "closed_pipe":
        tail = r.get("stderr_tail") or []
        return "slash worker closed pipe" + (": " + "\n".join(tail[-8:]) if tail else "")
    if f == "stop_plan":
        return _stop_expected(r)
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_slashworke_cdc456: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
