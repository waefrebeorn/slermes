"""twin for terminal_tool_rewrite_compound_background.

Traces LIVE tools/terminal_tool.py::_rewrite_compound_background with the
same rung order as the C harness:
  B1 rewrite found (result != command) / V whitespace-normalized result.

Agreed sub-contract (see gate.json): value events hash the normalized
result because the C port inserts an extra space before '&' when applying
rewrites; the scan loop (which rewrites are found, where) is exact.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_live_terminal_tool", os.path.join(DEV, "tools", "terminal_tool.py"))
_tt = ilu.module_from_spec(spec)
sys.modules["wc_live_terminal_tool"] = _tt
spec.loader.exec_module(_tt)

_rewrite = _tt._rewrite_compound_background


def trace(command):
    c = Cup("rewrite_compound_background")
    result = _rewrite(command)
    c.branch(result != command)          # B1
    c.valstr(" ".join(result.split()))   # V (normalized)
    return c.finalize()


def run(inputs):
    return [trace(cmd) for cmd in inputs]
