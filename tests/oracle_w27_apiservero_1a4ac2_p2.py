#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_openai_routes.py (gate p2).

Live module imported by path behind a tiny aiohttp shim (absent in this venv).
Only the module-level text-shaping helpers are called — they are pure functions
of their arguments, so no adapter/request/stream is constructed:

  * ``_cap_text``                  — the "...[N more chars]" rule
  * ``_cap_history_tool_outputs``  — stored-transcript tool-output cap
  * ``_trim_tool_items``           — response.completed tool-payload trim
  * ``_message_item``              — the Responses ``message`` output item
  * ``_post_stream_transform``     — (tail, appended) after a rewrite
  * ``_is_reasoning_input_item``   — echoed reasoning-item predicate
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
GATE = "w27_apiservero_1a4ac2_p2"


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
    "oair_live_1a4ac2_p2", QUARRY / "gateway/platforms/api_server_openai_routes.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"cap_text", "cap_history_tool_outputs", "trim_tool_items", "message_item",
          "post_stream_transform", "is_reasoning_input_item"}


def expected(r):
    f = r.get("func")
    if f == "cap_text":
        return live._cap_text(r.get("text", ""), r.get("keep", 0))
    if f == "cap_history_tool_outputs":
        # the live helper returns rows by reference unless it rewrote them; a
        # JSON round-trip pins the comparison to values, not identity.
        out = live._cap_history_tool_outputs(r.get("history") or [], r.get("max_chars", 0))
        return json.loads(json.dumps(out))
    if f == "trim_tool_items":
        # the live helper mutates its input list in place before returning it
        items = json.loads(json.dumps(r.get("items") or []))
        return live._trim_tool_items(items)
    if f == "message_item":
        return live._message_item(r.get("text"))
    if f == "post_stream_transform":
        return list(live._post_stream_transform(r.get("result")))
    if f == "is_reasoning_input_item":
        return live._is_reasoning_input_item(r.get("item"))
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
