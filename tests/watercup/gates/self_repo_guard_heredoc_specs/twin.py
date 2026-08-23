"""twin for self_repo_guard_heredoc_specs.

Imports LIVE tools/self_repo_guard.py::_heredoc_specs and traces the same
per-spec result-shape rungs as the C harness (which links the REAL port).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_self_repo_guard", os.path.join(DEV, "tools", "self_repo_guard.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_self_repo_guard"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for line in inputs:
        c = Cup("heredoc_specs")
        specs = mod._heredoc_specs(line)
        c.branch(len(specs) > 0)
        for hd in specs:
            c.loop()
            c.branch(hd.strip_tabs)
            c.branch(hd.execute_as_shell)
            c.valstr(hd.delimiter)
        cups.append(c.finalize())
    return cups
