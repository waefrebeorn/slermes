"""twin for shell_heredoc_parse_heredoc_operator.

Imports the LIVE Python tools/shell_heredoc.py::_parse_heredoc_operator and
traces the same result-shape rungs as the C harness (which calls the REAL
port via textual inclusion of shell_heredoc.c).
Input format: "<index>\t<command>".
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_shell_heredoc", os.path.join(DEV, "tools", "shell_heredoc.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_shell_heredoc"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        if "\t" in raw:
            idx_s, command = raw.split("\t", 1)
            index = int(idx_s) if idx_s else 0
        else:
            index, command = 0, raw
        c = Cup("parse_heredoc_operator")
        res = mod._parse_heredoc_operator(command, index)
        ok = res is not None
        c.branch(ok)
        if ok:
            end, delim, strip_tabs, quoted = res
            c.branch(strip_tabs)
            c.branch(quoted)
            c.valstr(str(end))
            c.valstr(delim)
        else:
            c.valstr("None")
        cups.append(c.finalize())
    return cups
