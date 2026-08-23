"""twin for approval_scan_dollar_paren_end.

Imports LIVE tools/approval.py::_scan_dollar_paren_end and traces the SAME
shared rungs as the C harness: B(unmatched) + V(normalized result).
The internal algorithms differ (see harness comment); the gate pins the
behavioral contract (matched end offset / unmatched) instead.
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
    "wc_live_approval", os.path.join(DEV, "tools", "approval.py"))
_ap = ilu.module_from_spec(_spec)
sys.modules["wc_live_approval"] = _ap
try:
    _spec.loader.exec_module(_ap)
except Exception as e:
    raise RuntimeError("cannot import LIVE tools/approval.py: %s" % e)


def trace(cmd):
    c = Cup("scan_dollar_paren_end")
    r = _ap._scan_dollar_paren_end(cmd, 0)
    unmatched = r is None or r >= len(cmd)
    c.branch(unmatched)
    c.valstr("None" if unmatched else str(int(r)))
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
