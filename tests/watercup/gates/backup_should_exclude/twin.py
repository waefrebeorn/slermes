"""twin for backup_should_exclude.

Traces LIVE hermes_cli/backup.py::_should_exclude with the same rung order
as the C harness.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_backup",
    os.path.join(DEV, "hermes_cli", "backup.py"))
_bk = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_backup"] = _bk
_spec.loader.exec_module(_bk)


def trace(rel_path):
    c = Cup("should_exclude")
    result = False
    if not rel_path:
        c.branch(True)                                   # B1
    else:
        c.branch(False)
        parts = rel_path.split("/")
        for i, part in enumerate(parts):
            c.loop()                                     # L
            if part not in _bk._EXCLUDED_DIRS:
                c.branch(False)                          # B2
                continue
            c.branch(True)
            nested = (part == "hermes-agent" and part != parts[0])
            c.branch(nested)                             # B3
            if not nested:
                result = True
                break
    if not result:
        name = rel_path.rsplit("/", 1)[-1] if rel_path else ""
        c.valstr(name)                                   # V1
        in_names = name in _bk._EXCLUDED_NAMES
        c.branch(in_names)                               # B4
        if in_names:
            result = True
        else:
            for suf in _bk._EXCLUDED_SUFFIXES:
                ends = name.endswith(suf)
                c.branch(ends)                           # B5
                if ends:
                    result = True
                    break
    return c.finalize()


def run(inputs):
    return [trace(p) for p in inputs]
