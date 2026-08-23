"""twin for libansi_strip.

Traces LIVE tools/ansi_strip.py::strip_ansi with the same rung order as the
C harness: B1 fast path (_HAS_ESCAPE miss) / B2 regex sub / V1 result.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_ansi_strip", os.path.join(DEV, "tools", "ansi_strip.py"))
_as = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_ansi_strip"] = _as
_spec.loader.exec_module(_as)


def trace(text):
    c = Cup("strip_ansi")
    if not _as._HAS_ESCAPE.search(text):     # B1
        c.branch(True)
        result = text
    else:
        c.branch(False)                      # B2
        result = _as._ANSI_ESCAPE_RE.sub("", text)
    c.valstr(result)
    c.finalize()
    return result, c


def run(inputs):
    return [trace(t)[1].finalize() for t in inputs]
