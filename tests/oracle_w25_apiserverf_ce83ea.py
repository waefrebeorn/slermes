#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_fire_startup.py
(gate w25_apiserverf_ce83ea).

The live coroutines are RUN, not reimplemented.  Three seams are pinned to the
fixture's values so a fire can be driven deterministically:

  * the runner is a bare object carrying ``_draining`` /
    ``_external_drain_active`` / ``_running`` exactly as the fixture spells them
    (a missing ``_running`` keeps getattr's default True);
  * the event loop's clock is a virtual one that starts at the fixture's ``now``
    and advances by the real ``_POLL_SECONDS`` on every ``asyncio.sleep`` -- so
    the poll count and the deadline are the same doubles both sides add;
  * the aiohttp request is a namespace whose ``app`` maps ``gateway_runner``.

``refuse_until_started`` and ``live_adapters_once_started`` are then compared on
their real return values (None vs. the 503 response with Retry-After 60).
"""
import asyncio
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_apiserverf_ce83ea"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import importlib.util


def _install_aiohttp_shim():
    """Minimal aiohttp.web standing in for the HTTP stack (a seam).

    The venv has no aiohttp; the live module only uses ``web.json_response``
    (and ``web.Response``/``web.Request`` in deferred annotations).  The shim's
    response mirrors aiohttp's: ``.status``, ``.headers``, ``.body`` in bytes.
    """
    import types

    class Response:
        def __init__(self, body=b"", status=200, headers=None):
            self.body = body
            self.status = status
            self.headers = dict(headers or {})

    def json_response(payload, status=200, headers=None):
        return Response(json.dumps(payload).encode("utf-8"), status, headers)

    web = types.ModuleType("aiohttp.web")
    web.json_response = json_response
    web.Response = Response
    web.Request = object
    aiohttp = types.ModuleType("aiohttp")
    aiohttp.web = web
    sys.modules["aiohttp"] = aiohttp
    sys.modules["aiohttp.web"] = web

    # The process-wide gateway runner ref is another seam: the fixture's runner
    # is the only one, so the fallback lookup finds nothing.
    run_mod = types.ModuleType("gateway.run")
    run_mod._gateway_runner_ref = lambda: None
    sys.modules["gateway.run"] = run_mod


def _load_live(path, name):
    """Load the upstream module from its own file.

    ``gateway.platforms``' package __init__ drags in the whole gateway stack
    (which wants a real HOME); this module only needs aiohttp/asyncio, so it is
    loaded directly and stays the genuine upstream source.
    """
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


_install_aiohttp_shim()
live = _load_live(QUARRY / "gateway" / "platforms" / "api_server_fire_startup.py",
                  "w25_fire_startup_live")

WANTED = {"is_draining", "startup_gate", "live_adapters", "constants"}


class _Runner:
    pass


def _make_runner(req):
    if not req.get("has_runner", True):
        return None
    r = _Runner()
    if "draining" in req:
        r._draining = req["draining"]
    if "external_drain" in req:
        r._external_drain_active = req["external_drain"]
    if "running" in req:
        r._running = req["running"]
    if "adapters" in req and req["adapters"] is not None:
        r.adapters = req["adapters"]
    return r


class _Adapter:
    def __init__(self, runner):
        self.gateway_runner = runner


def _drive(req, call):
    """Run ``call(runner, job_id, received_at)`` under the virtual clock."""
    runner = _make_runner(req)
    clock = {"t": req.get("now", 0.0)}
    polls = {"n": 0}

    class _Loop:
        def time(self):
            return clock["t"]

    async def _sleep(delay):
        clock["t"] += delay
        polls["n"] += 1

    saved_loop, saved_sleep = asyncio.get_running_loop, asyncio.sleep
    asyncio.get_running_loop = lambda: _Loop()
    asyncio.sleep = _sleep
    loop = asyncio.new_event_loop()
    try:
        value = loop.run_until_complete(
            call(runner, req.get("job_id"), req.get("received_at", 0.0)))
    finally:
        loop.close()
        asyncio.get_running_loop = saved_loop
        asyncio.sleep = saved_sleep
    return value, polls["n"]


def _refusal(resp, polls):
    if resp is None:
        return {"result": "allow", "polls": polls}
    return {
        "result": "refuse",
        "polls": polls,
        "status": resp.status,
        "retry_after": resp.headers["Retry-After"],
        "body": json.loads(resp.body.decode()),
    }


def is_draining(req):
    return live.runner_is_draining(_make_runner(req))


def startup_gate(req):
    resp, polls = _drive(
        req, lambda r, job, ra: live.refuse_until_started(r, job, received_at=ra))
    return _refusal(resp, polls)


def live_adapters(req):
    async def call(runner, job, received_at):
        request = types.SimpleNamespace(
            app={"gateway_runner": runner})
        return await live.live_adapters_once_started(
            _Adapter(runner), request, job, received_at=received_at)

    (resp, adapters), polls = _drive(req, call)
    return {"refusal": _refusal(resp, polls), "adapters": adapters}


def constants(_req):
    return {
        "fire_startup_wait_seconds": live.FIRE_STARTUP_WAIT_SECONDS,
        "poll_seconds": live._POLL_SECONDS,
        "retry_after_seconds": 60,
        "error_text": "gateway unreachable; retry",
    }


HANDLERS = {
    "is_draining": is_draining,
    "startup_gate": startup_gate,
    "live_adapters": live_adapters,
    "constants": constants,
}


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
