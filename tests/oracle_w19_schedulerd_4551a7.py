#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_delivery_standalone.py (gate w19_schedulerd_4551a7).

Both lane decisions are executed as the live functions with the transport replaced by a
fixture-driven fake: asyncio.run answers the fixture's outcome, the standalone sender and
`request_never_left` are stubs, the ledger verbs capture what the queue path records.
"""
import ast
import json
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "cron" / "scheduler_delivery_standalone.py"
LEDGER = Q / "gateway" / "delivery_ledger.py"
GATE = "w19_schedulerd_4551a7"
_ANN = __future__.annotations.compiler_flag


def _exec_nodes(path, funcs, assigns, g):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in funcs:
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign) and assigns:
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in assigns:
                    mod = ast.Module(body=[node], type_ignores=[])
                    exec(compile(mod, str(path), "exec", flags=_ANN, dont_inherit=True), g)
    return g


import concurrent.futures  # noqa: E402
import contextvars  # noqa: E402
import functools  # noqa: E402

G = {"__name__": "sds_live", "_UPDATER_STOP_TIMEOUT": None,
     "concurrent": concurrent, "contextvars": contextvars, "functools": functools,
     "Optional": object, "Any": object, "TYPE_CHECKING": False, "_TargetDelivery": object}
_exec_nodes(SRC, {"_standalone_send", "_queue_for_live_reconnect"}, {"_PRE_SEND_LIVE_REJECTIONS"}, G)

# --- stub packages the live code imports lazily at call time ---
_pkgs = ["gateway", "gateway.outbound", "gateway.platforms", "gateway.delivery_ledger"]
for _name in _pkgs:
    if _name not in sys.modules:
        _m = types.ModuleType(_name)
        _m.__path__ = []
        sys.modules[_name] = _m

_ledger_g = {"__name__": "dl_live", "hashlib": __import__("hashlib")}
_exec_nodes(LEDGER, {"compute_obligation_id", "is_reconnect_only"}, {"_RUNTIME_RETRYABLE_ERRORS"}, _ledger_g)
_ledger_mod = sys.modules["gateway.delivery_ledger"]
_ledger_mod.compute_obligation_id = _ledger_g["compute_obligation_id"]
_ledger_mod.is_reconnect_only = _ledger_g["is_reconnect_only"]
_ledger_mod.ledger_enabled = lambda: True

_send_mod = types.ModuleType("gateway.outbound.send")


async def _fake_send_to_platform(*a, **k):
    return {"success": True}


_send_mod._send_to_platform = _fake_send_to_platform
sys.modules["gateway.outbound.send"] = _send_mod

_shared_mod = types.ModuleType("gateway.platforms._shared")
_shared_mod.request_never_left = lambda exc: True
sys.modules["gateway.platforms._shared"] = _shared_mod

_LOGGER = types.SimpleNamespace(warning=lambda *a, **k: None, error=lambda *a, **k: None,
                                exception=lambda *a, **k: None, info=lambda *a, **k: None,
                                debug=lambda *a, **k: None)


class FakeAsyncio:
    """asyncio.run answering the fixture's outcome; wait_for is never awaited."""

    def __init__(self, r):
        self.r = r
        self.calls = 0

    def wait_for(self, coro, timeout=None):
        return None

    def run(self, coro):
        try:
            coro.close()
        except Exception:  # noqa: BLE001
            pass
        self.calls += 1
        outcome = self.r.get("outcome", "ok")
        exc_text = self.r.get("exc_text") or ""
        if outcome == "ok":
            return self.r.get("ok_result")
        if outcome == "timeout":
            raise TimeoutError("slow")
        if outcome == "error":
            raise ValueError(exc_text)
        if outcome in ("runtime_ok", "runtime_timeout", "runtime_error"):
            if self.calls == 1:
                raise RuntimeError("no running event loop")
            if outcome == "runtime_ok":
                return self.r.get("ok_result")
            if outcome == "runtime_timeout":
                raise TimeoutError("pool slow")
            raise ValueError(exc_text)
        return self.r.get("ok_result")


def case_standalone_send(r):
    t = types.SimpleNamespace(
        where=r.get("where", ""), job={"id": r.get("job_id", "j")},
        platform=r.get("platform_name", "telegram"), pconfig=None,
        chat_id=r.get("chat_id", ""), thread_id=r.get("thread_id") or None,
        notify_delivery=False, force_document=False,
        standalone_in_flight=bool(r.get("in_flight0")))
    G["asyncio"] = FakeAsyncio(r)
    G["logger"] = _LOGGER
    G["_sched"] = types.SimpleNamespace(
        _interpreter_shutting_down=lambda *a: bool(r.get("shutting_down") if not a else r.get("shutdown_on_retry")))
    G["_delivery"] = types.SimpleNamespace(_get_standalone_send_timeout=lambda: r.get("base_timeout", 0.0))
    G["_script"] = types.SimpleNamespace(
        _media_scaled_timeout=lambda base, media: r.get("send_timeout", base))
    sys.modules["gateway.platforms._shared"].request_never_left = (
        lambda exc: bool(r.get("request_never_left")))
    result, error = G["_standalone_send"](t, r.get("content", ""),
                                          [None] * int(r.get("media_count") or 0))
    return {"result": result, "error": error, "in_flight": bool(t.standalone_in_flight)}


def case_queue_for_reconnect(r):
    records = []
    fails = []

    def record_obligation(**kwargs):
        if r.get("record_raises"):
            raise RuntimeError("ledger down")
        records.append(kwargs)

    def mark_failed(oid, err):
        fails.append((oid, err))

    _ledger_mod.record_obligation = record_obligation
    _ledger_mod.mark_failed = mark_failed
    _ledger_mod.ledger_enabled = lambda: bool(r.get("ledger_enabled", True))

    calls = []

    class Loop:
        def call_soon_threadsafe(self, fn):
            calls.append(fn)

    profile = r.get("adapter_profile") or ""
    adapter = types.SimpleNamespace(
        _owner_profile=profile or None,
        gateway_runner=types.SimpleNamespace(
            _schedule_flood_redelivery=(lambda *a, **k: None) if r.get("has_schedule") else None))
    t = types.SimpleNamespace(
        where=r.get("where", ""), platform_name=r.get("platform_name", ""),
        chat_id=r.get("chat_id", ""), thread_id=r.get("thread_id") or None,
        job={"id": r.get("job_id", "")}, live_error=r.get("live_error"),
        live_retryable=bool(r.get("live_retryable")), transport=types.SimpleNamespace(adapter=adapter),
        loop=Loop() if r.get("has_loop") else None)
    del_errors = []
    G["logger"] = _LOGGER
    G["_queue_for_live_reconnect"](t, r.get("content", ""), [None] * int(r.get("media_count") or 0),
                                   del_errors)
    rec = records[-1] if records else None
    return {
        "queued": bool(records),
        "scheduled": bool(calls),
        "session_key": rec.get("session_key") if rec else None,
        "obligation_id": rec.get("obligation_id") if rec else None,
        "note": del_errors[-1] if del_errors else None,
        "mark_error": fails[-1][1] if fails else None,
        "adapter_profile": rec.get("adapter_profile") if rec else None,
    }


CASES = {"standalone_send": case_standalone_send, "queue_for_reconnect": case_queue_for_reconnect}


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
        if f not in CASES:
            continue
        cases += 1
        try:
            py = json.loads(json.dumps(CASES[f](r), default=str))
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
