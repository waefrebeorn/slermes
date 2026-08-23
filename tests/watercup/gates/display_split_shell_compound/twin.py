"""twin for display_split_shell_compound.

Imports LIVE agent/display.py::_split_shell_compound and traces the SAME
segmentation rungs as the harness around the REAL C port:
B(non-empty), per-segment B(1)+V(segment), terminator B(0).
Inputs avoid backslash-escaped closing quotes (a documented C/LIVE divergence
candidate) so the agreed sub-contract is exercised.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_agent_display", os.path.join(DEV, "agent", "display.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_agent_display"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("split_shell_compound")
        has = bool(raw)
        c.branch(has)
        if not has:
            c.valstr("")
        else:
            segs = mod._split_shell_compound(raw)
            for s in segs:
                c.branch(True)
                c.valstr(s)
            c.branch(False)
        cups.append(c.finalize())
    return cups
