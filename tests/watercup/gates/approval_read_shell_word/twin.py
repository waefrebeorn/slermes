"""twin for approval_read_shell_word.

Imports LIVE tools/approval.py::_read_shell_word and traces the same
result-shape rungs as the C harness (which links the REAL port).
Input format: "<pos>\t<command>".
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_approval", os.path.join(DEV, "tools", "approval.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_approval"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        if "\t" in raw:
            pos_s, command = raw.split("\t", 1)
            pos = int(pos_s) if pos_s else 0
        else:
            pos, command = 0, raw
        c = Cup("read_shell_word")
        start, end, word = mod._read_shell_word(command, pos)
        c.branch(end == start)
        c.valstr(str(start))
        c.valstr(str(end))
        c.valstr(word)
        cups.append(c.finalize())
    return cups
