"""twin for normalize_url_ladder.

Traces the LIVE tools/url_safety.py::normalize_url_for_request with the SAME
rung order as the C harness:
  U1 empty after strip / U2 scheme not http(s) / U3 hostname present /
  U4 IDNA changed the host.  V: normalized result.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_live_urlsafety", os.path.join(DEV, "tools", "url_safety.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_urlsafety"] = mod
spec.loader.exec_module(mod)

_live = mod.normalize_url_for_request


def run(inputs):
    cups = []
    for url in inputs:
        raw = url.strip() if isinstance(url, str) else url
        c = Cup("normalize_url_ladder")
        if not raw:                                            # U1
            c.branch(True)
            c.valstr(raw if isinstance(raw, str) else "")
            cups.append(c.finalize())
            continue
        c.branch(False)
        import re as _re
        repaired = _re.sub(r"^([A-Za-z][A-Za-z0-9+.-]*://)\s+", r"\1", raw)
        from urllib.parse import urlsplit
        parsed = urlsplit(repaired)
        if (parsed.scheme or "").lower() not in ("http", "https"):   # U2
            c.branch(True)
            c.valstr(url)
            cups.append(c.finalize())
            continue
        c.branch(False)

        netloc = parsed.netloc
        hostname = parsed.hostname or ""
        if hostname:                                           # U3
            c.branch(True)
        else:
            c.branch(False)
            hostname_idna = hostname
        try:
            ascii_host = hostname.encode("idna").decode("ascii")
        except UnicodeError:
            ascii_host = hostname
        if hostname:
            hostname_idna = ascii_host
            if ascii_host != hostname:                         # U4
                c.branch(True)
                netloc = netloc.replace(hostname, ascii_host, 1)
            else:
                c.branch(False)
        else:
            hostname_idna = hostname
            c.branch(False)

        from urllib.parse import quote, urlunsplit
        path = quote(parsed.path, safe="/%:@!$&'()*+,;=")
        query = quote(parsed.query, safe="/%:@!$&'()*+,;=?")
        fragment = quote(parsed.fragment, safe="/%:@!$&'()*+,;=?")
        out = urlunsplit((parsed.scheme, netloc, path, query, fragment))
        live_out = _live(url)
        assert out == live_out, "twin drift: %r vs LIVE %r" % (out, live_out)
        c.valstr(out)
        cups.append(c.finalize())
    return cups
