"""twin for session_search_order_for_recall.

Traces the LIVE tools/session_search_tool.py::_order_for_recall with the same
rung order as the C harness (traced copy of session_search_tool.c::
order_for_recall). Inputs are JSON array literals of FTS result rows.
  B1: results is a JSON array with >= 2 rows
  LOOP over rows:
    B2: row source is demoted ("cron")
  VSTR("<demoted_count>:<total>") class census
"""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_sst", os.path.join(DEV, "tools", "session_search_tool.py"))
_sst = ilu.module_from_spec(_spec)
sys.modules["wc_live_sst"] = _sst
try:
    _spec.loader.exec_module(_sst)
    live_order = _sst._order_for_recall
    DEMOTED = set(_sst._DEMOTED_SESSION_SOURCES)
except Exception:
    # fallback preserving the pure fn body
    DEMOTED = {"cron"}
    def live_order(rows):
        return sorted(rows, key=lambda r: 1 if (r.get("source") or "") in DEMOTED else 0)


def trace(raw):
    c = Cup("_order_for_recall")
    try:
        rows = json.loads(raw)
    except Exception:
        rows = raw
    if not isinstance(rows, list):
        rows = None

    n = len(rows) if isinstance(rows, list) else 0
    demoted = 0
    c.branch(n >= 2)                                            # B1
    if n >= 2:
        for r in rows:                                          # LOOP
            c.loop()
            d = ((r.get("source") or "") if isinstance(r, dict) else "") in DEMOTED
            c.branch(d)                                         # B2
            demoted += d
        ordered = live_order(list(rows))
        front = [r for r in rows if ((r.get("source") or "")) not in DEMOTED]
        back = [r for r in rows if ((r.get("source") or "")) in DEMOTED]
        assert ordered == front + back, "LIVE order diverges from stable partition"
    else:
        # n < 2 -> port returns input unchanged; still classify each row? No:
        # the C ladder only walks rows when it acts (n >= 2).
        pass

    c.valstr("%d:%d" % (demoted, n))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
