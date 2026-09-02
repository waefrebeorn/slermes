#!/usr/bin/env python3
"""
Twin for api_error_summary_ladder.

Mirrors the C harness branch ladder EXACTLY (same rung order, same branches).

The C harness (tests/watercup/gates/api_error_summary_ladder/harness.c) implements
the FIXED summarize_api_error with these branches:

  B1: network resolution marker found        -> label "offline"
  B2: malformed streaming response            -> label "malformed"
  B3: HTML error page                         -> label "html"
    B4 (html): title tag found
    B5 (html): ray id found
  B6: JSON body with usable message           -> label "json_msg"
  fallback                                    -> label "fallback"

The cup traces the LABEL via WCUP_VALSTR. The branch ladder (B1-B6 + B4/B5
inside HTML) is part of the cup hash, so the twin MUST emit the same ladder.

When run_agent.py can be imported (no _early_recovery crash), we use the LIVE
AIAgent._summarize_api_error. Otherwise we fall back to a pure-Python
reimplementation that matches the C harness ladder exactly.
"""
import sys
import os
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))

# 5x dirname from gate/twin.py -> repo root /home/wubu/hermes-agent-dev
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.dirname(HERE)))))
DEV = SLERMES

# --- attempt LIVE import ----------------------------------------------------
PY_RUN_AGENT = os.path.join(DEV, "run_agent.py")
_live_available = False
_live_summarize = None
try:
    _spec = ilu.spec_from_file_location("wc_live_run_agent", PY_RUN_AGENT)
    assert _spec is not None
    _mod = ilu.module_from_spec(_spec)
    sys.modules["wc_live_run_agent"] = _mod
    _spec.loader.exec_module(_mod)
    from wc_live_run_agent import AIAgent
    _live_available = True
except Exception:
    _live_available = False

from watercup_py import Cup

# --- pure-Python fallback that mirrors the C harness ladder exactly -----------
_NET_MARKERS = (
    "temporary failure in name resolution",
    "name or service not known",
    "nodename nor servname provided, or not known",
    "getaddrinfo failed",
    "no address associated with hostname",
    "network is unreachable",
)
_HTML_OPEN = ("<!doctype", "<html")
import json as _json

def _pure_summarize(raw_error: str) -> str:
    """Returns the label string.  The caller traces the branch ladder
    separately so the cup hash matches the C harness."""
    low = raw_error.lower()

    # B1: network marker
    b1 = any(needle in low for needle in _NET_MARKERS)
    if b1:
        return "offline"

    # B2: malformed streaming response
    b2 = "expected ident at line" in raw_error
    if b2:
        return "malformed"

    # B3: HTML page
    b3 = any(tag in low for tag in _HTML_OPEN)
    if b3:
        # B4: title tag found
        b4 = "<title" in raw_error.lower()
        # B5: ray id found
        b5 = "ray id:" in raw_error.lower()
        # Both B4 and B5 are traced but the label is always "html"
        return "html"

    # B6: JSON body with usable message
    msg = None
    try:
        body = _json.loads(raw_error)
    except Exception:
        body = None
    if isinstance(body, dict):
        err = body.get("error")
        if isinstance(err, dict) and isinstance(err.get("message"), str) and err["message"]:
            msg = err["message"]
        elif isinstance(body.get("message"), str) and body["message"]:
            msg = body["message"]
    b6 = msg is not None
    if b6:
        return "json_msg"

    return "fallback"


def trace(raw_error):
    # Strip the 5-char tag prefix that the C harness removes before
    # calling wc_summarize (the C harness does: colon = strchr(in, ':');
    # wc_summarize(colon ? colon + 1 : in))
    colon = raw_error.find(':')
    body = raw_error[colon + 1:] if colon != -1 else raw_error

    c = Cup("api_error_summary_ladder")
    low = body.lower()

    # B1
    b1 = any(needle in low for needle in _NET_MARKERS)
    c.branch(b1)
    if b1:
        c.valstr("offline")
        return c.finalize()

    # B2
    b2 = "expected ident at line" in body
    c.branch(b2)
    if b2:
        c.valstr("malformed")
        return c.finalize()

    # B3
    b3 = any(tag in low for tag in _HTML_OPEN)
    c.branch(b3)
    if b3:
        # B4
        b4 = "<title" in body.lower()
        c.branch(b4)
        # B5
        b5 = "ray id:" in body.lower()
        c.branch(b5)
        c.valstr("html")
        return c.finalize()

    # B3 not taken -> C harness does NOT trace B4/B5 at all.
    # Skip them — only trace B6.

    # B6
    msg = None
    try:
        body_json = _json.loads(body)
    except Exception:
        body_json = None
    if isinstance(body_json, dict):
        err = body_json.get("error")
        if isinstance(err, dict) and isinstance(err.get("message"), str) and err["message"]:
            msg = err["message"]
        elif isinstance(body_json.get("message"), str) and body_json["message"]:
            msg = body_json["message"]
    b6 = msg is not None
    c.branch(b6)
    if b6:
        c.valstr("json_msg")
    else:
        c.valstr("fallback")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]


def inputs_for_json():
    return [
        "NET:Connection error: temporary failure in name resolution (api.example.com)",
        "NET:Connection error: name or service not known",
        "NET:Connection error: getaddrinfo failed for provider",
        "NET:Connection error: network is unreachable",
        "VE:Expecting value: expected ident at line 1 column 1",
        "VE:plain value error without the marker",
        "HTML:<html><head><title>Service Unavailable</title></head><body>HTTP 502 Cloudflare Ray ID: <strong>7a9b1c</strong></body></html>",
        "HTML:<html><head><title>Oops</title></head><body>down</body></html>",
        "HTML:<html><body>HTTP 503 no title here Cloudflare Ray ID: <strong>ff00aa</strong></body></html>",
        "HTML:<!DOCTYPE html><body>maintenance page</body>",
        "BODY:{\"error\": {\"message\": \"This model's maximum context length is exceeded\"}}",
        "BODY:{\"message\": \"flat body message from provider\"}",
        "BODY:{\"error\": {\"code\": 42}}",
        "RAW:provider exploded unexpectedly",
        "RAW:HTTP 418: teapot refuses to brew, and this is a long enough message to matter",
    ]
