"""twin for binary_extensions_has_binary.

Traces the LIVE tools/binary_extensions.py::has_binary_extension with the
same rung order as the C harness:
  B1 empty / B2 no dot / V lowercased ext / B3 membership.
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
    "wc_live_binary_extensions",
    os.path.join(DEV, "tools", "binary_extensions.py"))
_be = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_binary_extensions"] = _be
_spec.loader.exec_module(_be)


def trace(path):
    c = Cup("has_binary_extension")
    # B1: empty
    if not path:
        c.branch(True)
        c.finalize()
        return False, c
    c.branch(False)
    dot = path.rfind(".")
    if dot == -1:                      # B2
        c.branch(True)
        c.finalize()
        return False, c
    c.branch(False)
    ext = path[dot:].lower()
    c.valstr(ext)                      # V1
    result = ext in _be.BINARY_EXTENSIONS   # B3
    c.branch(result)
    c.finalize()
    return result, c


def run(inputs):
    return [trace(p)[1].finalize() for p in inputs]
