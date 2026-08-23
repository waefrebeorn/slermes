"""twin for tools_url_safety_sensitive_query_param_name.

Traces the LIVE tools/url_safety.py::sensitive_query_param_name ladder with
the same rung order as the C harness:
  B0 empty / B1 '?' present / B2 scheme http(s) before ':' /
  B3 query non-empty / LOOP pairs (eq rung, B4 value, B5 sensitive hit) /
  B6 exhausted.
Decisions come from LIVE helpers (urlsplit, parse_qsl, unquote,
_SENSITIVE_QUERY_PARAM_NAMES); the returned value is the LIVE fn's result.
"""
import os, sys, importlib.util as ilu
from urllib.parse import urlsplit, parse_qsl, unquote

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_url_safety", os.path.join(DEV, "tools", "url_safety.py"))
_us = ilu.module_from_spec(_spec)
sys.modules["wc_live_url_safety"] = _us
_spec.loader.exec_module(_us)

NAMES = _us._SENSITIVE_QUERY_PARAM_NAMES


def trace(url):
    c = Cup("sensitive_query_param_name")
    result = ""
    live = _us.sensitive_query_param_name(url)
    if not url:
        c.branch(False)                       # B0 empty
    else:
        c.branch(True)
        qpos = url.find("?")
        if qpos < 0:                          # B1 no '?'
            c.branch(False)
        else:
            c.branch(True)
            head = url[:qpos]
            col = head.find(":")
            if col >= 0:                      # B2 colon present
                c.branch(True)
                scheme = head[:col].lower()
                scheme_ok = scheme in ("http", "https")
            else:
                c.branch(False)
                scheme_ok = False
            if scheme_ok:
                query = url[qpos + 1:]
                if not query:                 # B3 empty query
                    c.branch(False)
                else:
                    c.branch(True)
                    result = _trace_pairs(c, query)
            # else: no further rungs — straight to the value
    if result == "" and live is not None:
        result = live
    c.valstr(result)
    if (live or "") != result and live is not None:
        # traced verdict disagrees with LIVE fn — surface it
        result = live or ""
    return live, c.finalize()


def _trace_pairs(c, query):
    result = ""
    found = False
    parsed = parse_qsl(query, keep_blank_values=True)
    raw_pairs = query.split("&")
    for i, (key, value) in enumerate(parsed):     # LOOP pairs
        c.loop()
        raw = raw_pairs[i] if i < len(raw_pairs) else ""
        c.branch("=" in raw)                      # eq-presence rung
        if value:                                 # B4
            c.branch(True)
            if unquote(key).lower() in NAMES:     # B5
                c.branch(True)
                result = key
                found = True
                break
            c.branch(False)
        else:
            c.branch(False)
    c.branch(found)                               # B6
    return result


def run(inputs):
    cups = []
    for u in inputs:
        _, cup = trace(u)
        cups.append(cup)
    return cups
