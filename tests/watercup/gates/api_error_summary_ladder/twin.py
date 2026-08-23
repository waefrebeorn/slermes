"""twin for api_error_summary_ladder.

Traces the LIVE run_agent.py::AIAgent._summarize_api_error branch ladder with
the SAME rung order as the C harness:
  B1 network marker / B2 malformed / B3 html / B4 title / B5 ray /
  B6 json body message; VSTR(label).
The LIVE fn is called on a tagged fake exception and its result class is
asserted against the mirrored label.
"""
import os, sys, json, re, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_run_agent", os.path.join(DEV, "run_agent.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_run_agent"] = _live
_spec.loader.exec_module(_live)
_summarize = _live.AIAgent._summarize_api_error

MARKERS = [
    "temporary failure in name resolution",
    "name or service not known",
    "nodename nor servname provided, or not known",
    "getaddrinfo failed",
    "no address associated with hostname",
    "network is unreachable",
]

OFFLINE = ("Hermes can't reach the model provider. You may be offline. "
           "Check your internet connection and try again.")


class FakeValueError(ValueError):
    pass


class FakeErr(Exception):
    def __init__(self, text, kind="raw", status_code=None, body=None):
        super().__init__(text)
        self._kind = kind
        if status_code is not None:
            self.status_code = status_code
        if body is not None:
            self.body = body


def _build(tagged):
    tag, _, rest = tagged.partition(":")
    if tag == "NET":
        return FakeErr(rest, "net")
    if tag == "VE":
        return FakeValueError(rest)
    if tag == "HTML":
        m = re.search(r"HTTP (\d+)", rest)
        return FakeErr(rest, "html", status_code=int(m.group(1)) if m else None)
    if tag == "BODY":
        return FakeErr(rest, "body", body=json.loads(rest))
    return FakeErr(rest, "raw")


def trace(tagged):
    err = _build(tagged)
    raw = str(err)
    c = Cup("api_error_summary_ladder")

    # B1: network marker (LIVE walks the exception chain; single-link here)
    hit = any(m in raw.lower() for m in MARKERS)
    c.branch(hit)
    if hit:
        label = "offline"
        assert _summarize(err) == OFFLINE
        c.valstr(label)
        return c.finalize()

    # B2: malformed streaming response (ValueError + expected ident at line)
    malformed = isinstance(err, ValueError) and "expected ident at line" in raw.lower()
    c.branch(malformed)
    if malformed:
        c.valstr("malformed")
        out = _summarize(err)
        assert out.startswith("Malformed provider streaming response:")
        return c.finalize()

    # B3: HTML page
    is_html = "<!DOCTYPE" in raw or "<html" in raw
    c.branch(is_html)
    if is_html:
        has_title = re.search(r"<title[^>]*>([^<]+)</title>", raw, re.IGNORECASE) is not None
        c.branch(has_title)      # B4
        has_ray = re.search(r"Cloudflare Ray ID:\s*<strong[^>]*>([^<]+)</strong>", raw) is not None
        c.branch(has_ray)        # B5
        out = _summarize(err)
        assert not out.startswith("Malformed") and len(out) > 0
        c.valstr("html")
        return c.finalize()

    # B6: json body dict with usable message
    msg = None
    body = getattr(err, "body", None)
    if isinstance(body, dict):
        e = body.get("error")
        if isinstance(e, dict):
            msg = e.get("message")
        if not msg:
            msg = body.get("message")
    got = bool(msg)
    c.branch(got)
    label = "json_msg" if got else "fallback"
    out = _summarize(err)
    if got:
        # class check: the live summary must carry the message, not HTML noise
        assert "HTML error page" not in out
        assert str(msg)[:80] in out
    c.valstr(label)
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
