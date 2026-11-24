#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_stream_watchdog.py (w20_auxiliarys_066a48).

The live module is imported against stub seams for its only dependencies:
agent.sleep_clock (an injected clock), agent.agent_runtime_helpers (the socket
helpers, which a response-less stream never reaches), and the three config reads
(waits_like_local / _cloud_stale_timeout_for / _get_task_no_progress_timeout)
whose answers the fixture supplies. The class itself -- __init__, _rearm,
progress, _fire, finish, timeout_error -- is the live code path, driven with
object.__new__/real instances so nothing is re-implemented in the oracle.
"""
import importlib.util
import inspect
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

CLOCK = [0.0]
CFG = {}


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mk("agent")
_mk("agent.sleep_clock", awake_seconds=lambda: CLOCK[0])
_mk("agent.agent_runtime_helpers", _socket_from_response=lambda r: None,
    _shutdown_socket=lambda s: None)
AUX = _mk("agent.auxiliary_client",
          _AUX_STREAM_NO_PROGRESS_TIMEOUT_SECONDS=60,
          _RELAY_AUX_CALL_CONTEXT=SimpleNamespace(
              get=lambda: {"stream_provider": CFG.get("provider")}),
          _get_task_no_progress_timeout=lambda task: CFG.get("task_timeout"))
_mk("agent.chat_completion_helpers",
    _cloud_stale_timeout_for=lambda route, kwargs: CFG["cloud_stale"])
_mk("agent.process_transport", waits_like_local=lambda base_url, provider: CFG["local"])

NAME = "w20_auxiliarys_066a48_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "auxiliary_stream_watchdog.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class _Awaitable:
    def __await__(self):
        if False:  # pragma: no cover - makes this a generator, never yields
            yield None
        return None


class _Chunks:
    pass


def _close_expected(spec):
    spec = spec if isinstance(spec, dict) else {}
    c = _Chunks()
    raises = bool(spec.get("raises"))

    def _fn():
        if raises:
            raise RuntimeError("boom")
        return _Awaitable() if spec.get("awaitable") else object()

    if spec.get("has_close"):
        c.close = _fn
    if spec.get("has_aclose"):
        c.aclose = _fn
    res = live._close_chunk_stream(c, allow_aclose=bool(spec.get("allow_aclose")))
    return "awaitable" if res is not None else None


def _windows_expected(spec):
    spec = spec if isinstance(spec, dict) else {}
    CFG["task_timeout"] = spec.get("task_timeout")
    CFG["cloud_stale"] = spec.get("cloud_stale")
    CFG["local"] = bool(spec.get("local"))
    CFG["provider"] = spec.get("provider")
    AUX._AUX_STREAM_NO_PROGRESS_TIMEOUT_SECONDS = spec.get("aux_default")
    client = SimpleNamespace(base_url=spec.get("base_url", "http://127.0.0.1:8080/v1"))
    kwargs = {"model": spec.get("model")}
    if "timeout" in spec:
        kwargs["timeout"] = spec.get("timeout")
    window, first = live.chat_stream_windows(client, kwargs, None)
    return [float(window), None if first is None else float(first)]


def _simulate_expected(spec):
    spec = spec if isinstance(spec, dict) else {}
    window = spec.get("window")
    first = spec.get("first_token")
    start = spec.get("start", 0.0)
    CLOCK[0] = start
    w = live.ChatStreamWatchdog(SimpleNamespace(response=None), window,
                                first_token_window=first)
    if spec.get("progress_at") is not None:
        CLOCK[0] = spec["progress_at"]
        w.progress()
    if spec.get("fire_at") is not None:
        CLOCK[0] = spec["fire_at"]
        gen = w._generation - (1 if spec.get("stale_generation") else 0)
        w._fire(gen)
    err = str(w.timeout_error())
    out = {
        "saw_progress": bool(w.saw_progress),
        "fired": bool(w.fired),
        "done": bool(w._done),
        "deadline_offset": None if w._deadline is None else float(w._deadline - start),
        "generation": w._generation,
        "error": err,
    }
    w.finish()
    return out


def expected(r):
    f = r.get("func")
    if f == "stream_windows":
        return _windows_expected(r.get("spec"))
    if f == "timeout_error":
        w = object.__new__(live.ChatStreamWatchdog)
        w.window = r.get("window")
        w.first_token_window = r.get("first_token")
        w._started = -float(r.get("elapsed", 0.0))
        w.saw_progress = bool(r.get("saw_progress"))
        CLOCK[0] = 0.0
        return str(w.timeout_error())
    if f == "close_chunk_stream":
        return _close_expected(r.get("spec"))
    if f == "simulate":
        return _simulate_expected(r.get("spec"))
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
print("ORACLE w20_auxiliarys_066a48: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
