#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_bubble_cleanup.py (w20_runturnbub_84c9ef).

The live module is imported with gateway.turn_context stubbed (its only import
beyond asyncio/logging), then each harness line drives the real
register_bubble_cleanup: the admission test, the snapshot it captures and the
asyncio cleanup coroutine it registers are all executed against fake adapters,
and the log line the coroutine emits is captured. `out` is compared.
"""
import asyncio
import importlib.util
import json
import logging
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

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


_mk("gateway")


class TurnContext:  # stand-in for gateway.turn_context.TurnContext
    pass


_mk("gateway.turn_context", TurnContext=TurnContext)

NAME = "w20_runturnbub_84c9ef_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "run_turn_bubble_cleanup.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class _Capture(logging.Handler):
    def __init__(self):
        super().__init__()
        self.records = []

    def emit(self, record):
        self.records.append(record)


def _adapter(has_cb, results=None):
    """Fake adapter; `results` is consumed one per delete_message call."""
    calls = []
    regs = []
    queue = list(results or [])

    class WithCb:
        async def delete_message(self, chat_id, mid):
            calls.append(mid)
            kind = queue.pop(0) if queue else "ok"
            if kind == "raise":
                raise RuntimeError("delete failed")
            if kind == "false":
                return False
            return True

        def register_post_delivery_callback(self, key, fn, generation=None):
            regs.append((key, fn, generation))

    class WithoutCb:
        async def delete_message(self, chat_id, mid):
            calls.append(mid)
            return True

    return (WithCb() if has_cb else WithoutCb()), calls, regs


def _ctx(spec, adapter, ids):
    return types.SimpleNamespace(
        _cleanup_progress=bool(spec.get("cleanup_progress")),
        _cleanup_adapter=adapter,
        _cleanup_msg_ids=list(ids),
        session_key=spec.get("session_key") or "",
        run_generation=spec.get("generation"),
        source=types.SimpleNamespace(chat_id=spec.get("chat_id")),
    )


def expected(r):
    f = r.get("func")
    if f == "delete_timeout_seconds":
        return float(live.DELETE_TIMEOUT_SECONDS)
    if f == "register":
        spec = r.get("ctx") if isinstance(r.get("ctx"), dict) else {}
        has_cb = bool(spec.get("has_register_cb"))
        adapter, calls, regs = _adapter(has_cb)
        adapter = adapter if spec.get("has_adapter") else None
        ctx = _ctx(spec, adapter, spec.get("ids") or [])
        live.register_bubble_cleanup(r.get("response"), ctx)
        if regs:
            asyncio.run(regs[0][1]())
        return {"register": bool(regs),
                "ids": calls if regs else [],
                "chat_id": spec.get("chat_id") if regs else None,
                "generation": regs[0][2] if regs else None}
    if f == "cleanup":
        spec = r.get("spec") if isinstance(r.get("spec"), dict) else {}
        results = list(spec.get("results") or [])
        adapter, _calls, regs = _adapter(True, results)
        ids = ["m%d" % i for i in range(len(results))]
        ctx = _ctx({"cleanup_progress": True, "session_key": spec.get("session_key"),
                    "generation": "g1", "chat_id": "c1"}, adapter, ids)
        capture = _Capture()
        live.logger.addHandler(capture)
        previous = live.logger.level
        live.logger.setLevel(logging.INFO)
        try:
            live.register_bubble_cleanup({"ok": True}, ctx)
            if regs:
                asyncio.run(regs[0][1]())
        finally:
            live.logger.removeHandler(capture)
            live.logger.setLevel(previous)
        if not regs:
            return {"failed": None, "message": None}
        info = [rec for rec in capture.records if rec.levelno >= logging.INFO]
        if not info:
            return {"failed": 0, "message": None}
        return {"failed": int(info[-1].args[1]), "message": info[-1].getMessage()}
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
print("ORACLE w20_runturnbub_84c9ef: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
