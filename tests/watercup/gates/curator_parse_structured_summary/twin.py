"""twin for curator_parse_structured_summary.

Calls LIVE agent/curator.py::_parse_structured_summary and traces the same
derived-shape ladder as the C harness:
  B1 no result / B2 consolidations rows / B3 prunings rows,
with canonical V rows "from>into" and "name>reason".
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_curator", os.path.join(DEV, "agent", "curator.py"))
_cu = ilu.module_from_spec(_spec)
sys.modules["wc_live_curator"] = _cu
_spec.loader.exec_module(_cu)


def trace(text):
    c = Cup("parse_structured_summary")
    try:
        data = _cu._parse_structured_summary(text)
    except Exception:
        data = None
    if not isinstance(data, dict):                         # B1
        c.branch(True)
        return c.finalize()
    c.branch(False)
    cons = data.get("consolidations") or []
    prunes = data.get("prunings") or []
    c.branch(len(cons) > 0)                                # B2
    for item in cons:
        c.loop()
        a = item.get("from", "") if isinstance(item, dict) else ""
        b = item.get("into", "") if isinstance(item, dict) else ""
        c.valstr("%s>%s" % (a or "", b or ""))
    c.branch(len(prunes) > 0)                              # B3
    for item in prunes:
        c.loop()
        a = item.get("name", "") if isinstance(item, dict) else ""
        b = item.get("reason", "") if isinstance(item, dict) else ""
        c.valstr("%s>%s" % (a or "", b or ""))
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
