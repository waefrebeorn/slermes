#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_openai_routes.py (gate p3).

Live module imported by path behind a tiny aiohttp shim (absent in this venv);
the shim's ``web.json_response`` records ``(body, status)`` so the validators'
400 envelopes can be compared as values.  The Responses envelope helpers are
called on a ``_ResponsesStream`` built with ``__new__``: no response writer, no
store, no agent — only the attributes the value methods read.

  * ``_parse_chat_messages``          — messages -> (system prompt, turns, error)
  * ``_parse_responses_input``        — Responses ``input`` reader
  * ``_parse_conversation_history``   — explicit history reader
  * ``_ResponsesStream.envelope``     — in_progress envelope
  * ``_ResponsesStream.terminal_envelope`` — terminal envelope (+usage)
  * ``_ResponsesStream._history_with_user``
  * ``_ResponsesStream._final_items``
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
GATE = "w27_apiservero_1a4ac2_p3"

# The origin adapter module resolves the Hermes home at import time; give it a
# stable home if this shell lost USERPROFILE/HOME (see repo-parent rule).
import os

for _var, _fallback in (("USERPROFILE", str(REPO.parent)),
                        ("HOME", str(REPO.parent)),
                        ("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))):
    if not os.environ.get(_var):
        os.environ[_var] = _fallback


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
    "oair_live_1a4ac2_p3", QUARRY / "gateway/platforms/api_server_openai_routes.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

# The envelope helpers reach the adapter module for the usage payload; that is a
# real import of the origin module (the shim only supplies aiohttp).
import gateway.platforms.api_server as api  # noqa: E402

WANTED = {"parse_chat_messages", "parse_responses_input", "parse_conversation_history",
          "stream_envelope", "terminal_envelope", "history_with_user", "final_items"}


def _err(resp):
    if resp is None:
        return None
    return {"status": getattr(resp, "status", None), "body": getattr(resp, "body", None)}


def _stream(r):
    st = live._ResponsesStream.__new__(live._ResponsesStream)
    st._api = api
    st.response_id = r.get("response_id", "")
    st.created_at = r.get("created_at", 0)
    st.model = r.get("model", "")
    st.usage = r.get("usage") or {}
    st.conversation_history = r.get("history") or []
    st.user_message = r.get("user_message")
    st.emitted_items = r.get("emitted_items") or []
    st.final_response_text = r.get("final_response_text") or ""
    st.agent_error = r.get("agent_error")
    st.durable_key = r.get("durable_key")
    return st


def expected(r):
    f = r.get("func")
    if f == "parse_chat_messages":
        sp, msgs, err = live._parse_chat_messages(r.get("messages") or [])
        return {"system_prompt": sp, "messages": msgs, "error": _err(err)}
    if f == "parse_responses_input":
        msgs, err = live._parse_responses_input(r.get("input"))
        return {"messages": msgs, "error": _err(err)}
    if f == "parse_conversation_history":
        hist, err = live._parse_conversation_history(r.get("conversation_history"))
        return {"history": hist, "error": _err(err)}
    if f == "stream_envelope":
        st = _stream(r)
        return st.envelope(r.get("status", ""))
    if f == "terminal_envelope":
        st = _stream(r)
        return st.terminal_envelope(r.get("status", ""), r.get("output") or [],
                                    error=r.get("error"))
    if f == "history_with_user":
        st = _stream(r)
        return st._history_with_user()
    if f == "final_items":
        st = _stream(r)
        return st._final_items()
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
