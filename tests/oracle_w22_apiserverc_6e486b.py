#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_config_guard.py
(gate w22_apiserverc_6e486b).

Dependency seam: `from aiohttp import web` cannot load on this host --
  ModuleNotFoundError: No module named 'attr'   (aiohttp -> attr)
so the oracle injects a minimal `aiohttp` / `aiohttp.web` whose `middleware` is
the identity decorator and whose `json_response` records body+status. Everything
else runs live: `hermes_cli.config_backend.ConfigBackendUnavailable` is the real
SystemExit subclass, and the handler raises a real one carrying a real `code`.

  middleware -- live `config_unavailable_middleware(request, handler)` -> the
                handler's value, a recorded 503 response, or the escaping error.
  body       -- the 503 payload the middleware builds for a given `code`
                (`str(exc.code)`), compared on its own.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "api_server_config_guard.py"
GATE = "w22_apiserverc_6e486b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))


class Resp:
    """What web.json_response hands back: the body plus the status code."""

    def __init__(self, body, status=200):
        self.body = body
        self.status = status


def _middleware(fn):
    return fn


web_mod = types.ModuleType("aiohttp.web")
web_mod.middleware = _middleware
web_mod.json_response = lambda body, status=200: Resp(body, status)
aiohttp_mod = types.ModuleType("aiohttp")
aiohttp_mod.web = web_mod
sys.modules.setdefault("aiohttp", aiohttp_mod)
sys.modules.setdefault("aiohttp.web", web_mod)
sys.modules["aiohttp"].web = web_mod

_spec = importlib.util.spec_from_file_location("api_server_config_guard_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["api_server_config_guard_live"] = live
_spec.loader.exec_module(live)

Unavailable = live.ConfigBackendUnavailable
WANTED = {"middleware", "body"}


def middleware_case(outcome, value, code):
    async def handler(_request):
        if outcome == "config_unavailable":
            # SystemExit always carries `.code`; building it bare leaves None.
            exc = Unavailable()
            exc.code = code
            raise exc
        if outcome == "runtime_error":
            raise RuntimeError("boom")
        return value

    async def main():
        try:
            res = await live.config_unavailable_middleware(object(), handler)
        except BaseException as exc:  # noqa: BLE001  (SystemExit is not Exception)
            return {"action": "raise", "type": exc.__class__.__name__}
        if isinstance(res, Resp):
            return {"action": "response", "status": res.status, "body": res.body}
        return {"action": "return", "value": res}

    return asyncio.run(main())


def body_case(code):
    async def handler(_request):
        exc = Unavailable()
        exc.code = code
        raise exc

    async def main():
        res = await live.config_unavailable_middleware(object(), handler)
        return res.body

    return asyncio.run(main())


def expected(r):
    f = r.get("func")
    if f == "middleware":
        outcome = r.get("outcome") or "return"
        return middleware_case(outcome, r.get("value"), r.get("code"))
    if f == "body":
        return body_case(r.get("code"))
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
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
