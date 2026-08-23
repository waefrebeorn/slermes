"""twin for session_search_tool_normalize_title_query.

Traces LIVE tools/session_search_tool.py::_normalize_title_query with the
same rung order as the C harness (src/tools/session_search_tool.c::
normalize_title_query):
  B1 null / LOOP lead-ws / LOOP trail-ws / LOOP lead-quotes /
  LOOP trail-quotes / B2 nonempty.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_session_search_tool", os.path.join(DEV, "tools", "session_search_tool.py"))
_sst = ilu.module_from_spec(_spec)
sys.modules["wc_live_session_search_tool"] = _sst
_spec.loader.exec_module(_sst)


def trace(query):
    c = Cup("normalize_title_query")
    if query is None:                                # B1
        c.branch(True)
        res = ""
    else:
        c.branch(False)
        start = 0
        n = len(query)
        while start < n and query[start].isspace():  # LOOP lead ws
            c.loop()
            start += 1
        end = n
        while end > start and query[end - 1].isspace():  # LOOP trail ws
            c.loop()
            end -= 1
        lo = start
        hi = end
        while lo < hi and query[lo] in "`'\"":       # LOOP lead quotes
            c.loop()
            lo += 1
        while hi > lo and query[hi - 1] in "`'\"":   # LOOP trail quotes
            c.loop()
            hi -= 1
        res = query[lo:hi]
        c.branch(len(res) > 0)                       # B2
    live = _sst._normalize_title_query(query if query is not None else "")
    assert live == res, (query, live, res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for q in inputs:
        _, cup = trace(q)
        cups.append(cup)
    return cups
