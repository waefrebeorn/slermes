#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_openai_routes.py (gate p1).

The live module is imported by path (aiohttp is absent in this venv, so a tiny
shim module is installed first — the C port never sees the shim).  Only the
module-level pure helpers are called: they take a result dict and return
values, so no adapter, request, store or stream is ever constructed.

  * ``_result_flags``            — (completed, partial, failed, error)
  * ``_budget_exhausted``        — iteration-budget verdict
  * ``_finish_reason``           — the finish_reason an OpenAI SDK reads
  * ``_response_status``         — the Responses ``status`` string
  * ``_hermes_extras``           — the ``hermes`` block
  * ``_settled``                 — settled idempotency record
  * ``_responses_fingerprint_keys`` — identity key list
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_apiservero_1a4ac2_p1"


def _install_aiohttp_shim():
    """Minimal aiohttp.web surface so the origin module imports without it."""
    try:
        import aiohttp  # noqa: F401
        return
    except Exception:
        pass

    class _Resp:
        def __init__(self, body=None, status=200, headers=None, **kw):
            self.body = body
            self.status = status
            self.headers = dict(headers or {})

    def _middleware(fn=None, **kw):
        return fn if fn is not None else (lambda f: f)

    class _Bare:
        def __init__(self, *a, **k):
            pass

    web = types.ModuleType("aiohttp.web")
    web.Response = _Resp
    web.StreamResponse = _Resp
    web.Request = object
    web.middleware = _middleware
    web.Application = _Bare
    web.AppRunner = _Bare
    web.TCPSite = _Bare
    web.RouteTableDef = list
    web.HTTPException = Exception
    web.HTTPRequestEntityTooLarge = Exception
    web.FileResponse = _Resp
    web.WebSocketResponse = _Resp
    web.json_response = lambda body=None, *, status=200, headers=None, **kw: _Resp(body, status, headers)
    mod = types.ModuleType("aiohttp")
    mod.web = web
    sys.modules["aiohttp"] = mod
    sys.modules["aiohttp.web"] = web


_install_aiohttp_shim()
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "oair_live_1a4ac2_p1", QUARRY / "gateway/platforms/api_server_openai_routes.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"result_flags", "budget_exhausted", "finish_reason", "response_status",
          "hermes_extras", "settled", "fingerprint_keys"}


def expected(r):
    f = r.get("func")
    if f == "result_flags":
        return list(live._result_flags(r.get("result")))
    if f == "budget_exhausted":
        return live._budget_exhausted(r.get("result"))
    if f == "finish_reason":
        return live._finish_reason(
            r.get("completed", True), r.get("is_partial", False), r.get("is_failed", False),
            r.get("err_msg"), r.get("agent_error"),
            budget_exhausted=bool(r.get("budget_exhausted", False)))
    if f == "response_status":
        return live._response_status(r.get("result"))
    if f == "hermes_extras":
        return live._hermes_extras(
            r.get("completed", True), r.get("is_partial", False), r.get("is_failed", False),
            r.get("err_msg"), r.get("finish_reason", "stop"),
            budget_exhausted=bool(r.get("budget_exhausted", False)))
    if f == "settled":
        return live._settled(r.get("record"))
    if f == "fingerprint_keys":
        return list(live._responses_fingerprint_keys(r.get("body") or {}))
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
        if r.get("skip") is True:
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
