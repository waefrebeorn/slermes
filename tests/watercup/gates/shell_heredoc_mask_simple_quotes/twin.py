"""twin for shell_heredoc_mask_simple_quotes.

Imports the LIVE tools/shell_heredoc.py::_mask_simple_quotes and traces the
SAME observable ladder as the C harness (which calls the REAL C port):

  B1 contains ' / B2 contains " / B3 contains ` / V masked output.
"""
import os
import sys
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_shell_heredoc",
    os.path.join(DEV, "tools", "shell_heredoc.py"))
_sh = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_shell_heredoc"] = _sh
_spec.loader.exec_module(_sh)


def run(inputs):
    cups = []
    for cmd in inputs:
        c = Cup("mask_simple_quotes")
        c.branch("'" in cmd)
        c.branch('"' in cmd)
        c.branch("`" in cmd)
        masked = _sh._mask_simple_quotes(cmd)
        c.valstr(masked)
        cups.append(c.finalize())
    return cups
