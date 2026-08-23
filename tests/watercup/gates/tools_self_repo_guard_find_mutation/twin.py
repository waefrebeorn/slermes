"""twin for tools_self_repo_guard_find_mutation.

Imports LIVE tools/self_repo_guard.py::_find_mutation and traces the same
coarse ladder as the C harness (which links the REAL port):
  B1 empty command   B2 mutation detected   V: operation string / "".
Fixed context identical to the harness: cwd=/tmp, root=dev checkout.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_srg", os.path.join(DEV, "tools", "self_repo_guard.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_srg"] = mod
spec.loader.exec_module(mod)

from pathlib import Path
from watercup_py import Cup

SRG_CWD = Path("/tmp")
SRG_ROOT = Path(DEV)


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("find_mutation")
        c.branch(not raw)                                    # B1
        op = mod._find_mutation(raw, SRG_CWD, SRG_ROOT, 0) if raw else None
        c.branch(op is not None)                             # B2
        c.valstr(op if op else "")
        cups.append(c.finalize())
    return cups
