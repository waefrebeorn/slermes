#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_host_http_auth.py
(gate w22_pluginhost_948f76).

Two seams, both outside the module's logic:

  _require_token  -- imported lazily from hermes_cli.web_server, which pulls the
                     whole web server; the oracle injects a stub whose behaviour
                     is described by the fixture (`ok` / raise an HTTPException
                     with a given status / raise something else). The real
                     fastapi HTTPException type is used, so the `except
                     HTTPException` arm and `status_code != 401` are live.
  ASGI app        -- a recorder, so the merged scope `app({**scope, "state":
                     state}, ...)` is what gets compared.

  admitted -- live `admitted_for_sensitive_route(request)` -> {"admitted": b}
              or {"raised": {...}} for a re-raised HTTPException / other error.
  scope    -- live `with_hosted_admission(app, authenticated)`, awaited once.
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
SRC = QUARRY / "hermes_cli" / "plugin_host_http_auth.py"
GATE = "w22_pluginhost_948f76"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from fastapi import HTTPException  # noqa: E402  (real type, the module's own import)

_spec = importlib.util.spec_from_file_location("plugin_host_http_auth_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["plugin_host_http_auth_live"] = live
_spec.loader.exec_module(live)

WANTED = {"admitted", "scope"}


def admitted_case(guard):
    spec = guard if isinstance(guard, dict) else {}
    kind = spec.get("kind")
    if kind not in ("http", "raise"):
        kind = "ok"

    def _require_token(_request):
        if kind == "http":
            raise HTTPException(status_code=spec.get("status"),
                                detail=spec.get("detail", "Unauthorized"))
        if kind == "raise":
            raise ValueError(spec.get("detail", ""))

    stub = types.ModuleType("hermes_cli.web_server")
    stub._require_token = _require_token
    prev = sys.modules.get("hermes_cli.web_server")
    sys.modules["hermes_cli.web_server"] = stub
    try:
        try:
            return {"admitted": bool(live.admitted_for_sensitive_route(object()))}
        except HTTPException as exc:
            return {"raised": {"type": "HTTPException", "status": exc.status_code,
                               "detail": exc.detail}}
        except Exception as exc:  # noqa: BLE001
            return {"raised": {"type": exc.__class__.__name__, "detail": str(exc)}}
    finally:
        if prev is None:
            sys.modules.pop("hermes_cli.web_server", None)
        else:
            sys.modules["hermes_cli.web_server"] = prev


def scope_case(scope, authenticated):
    captured = {}

    async def app(merged, _receive, _send):
        captured["scope"] = merged

    async def receive():
        return {"type": "http.request"}

    async def send(_message):
        return None

    wrapped = live.with_hosted_admission(app, authenticated)
    try:
        asyncio.run(wrapped(scope, receive, send))
    except Exception as exc:  # noqa: BLE001
        return {"raised": {"type": exc.__class__.__name__}}
    return {"scope": captured["scope"]}


def expected(r):
    f = r.get("func")
    if f == "admitted":
        return admitted_case(r.get("guard"))
    if f == "scope":
        return scope_case(r.get("scope"), r.get("authenticated"))
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
