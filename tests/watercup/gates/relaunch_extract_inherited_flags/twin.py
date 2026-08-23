"""twin for relaunch_extract_inherited_flags.

Imports LIVE hermes_cli/relaunch.py::_extract_inherited_flags and traces the
same branch ladder as the fixed C wrapper. The V event is LIVE's real list.
NOTE: the C table snapshots LIVE's argparse-derived _INHERITED_FLAGS_TABLE at
gate-writing time; a CLI flag change must update both sides.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_relaunch", os.path.join(DEV, "hermes_cli", "relaunch.py"))
mod = ilu.module_from_spec(spec); sys.modules["wc_relaunch"] = mod
spec.loader.exec_module(mod)
fn = mod._extract_inherited_flags
TABLE = dict(mod._INHERITED_FLAGS_TABLE)

from watercup_py import Cup

def run(inputs):
    cups = []
    for arg in inputs:
        argv = [t for t in arg.split(" ") if t]
        flags = fn(arg.split(" ")) if False else fn(argv)
        c = Cup("extract_inherited_flags")
        if not arg:
            c.branch(True)
        else:
            c.branch(False)
            i = 0
            while i < len(argv):
                c.loop()
                tok = argv[i]
                if "=" in tok:
                    c.branch(True)
                    key = tok.split("=", 1)[0]
                    hit = key in TABLE
                    c.branch(hit)
                    i += 1
                    continue
                c.branch(False)
                hit = tok in TABLE
                c.branch(hit)
                if hit:
                    takes = TABLE[tok]
                    if takes:
                        c.branch(True)
                        if i + 1 < len(argv):
                            c.branch(True)
                            novalue = not argv[i + 1].startswith("-")
                            c.branch(novalue)
                            if novalue:
                                i += 1
                        else:
                            c.branch(False)
                    else:
                        c.branch(False)
                i += 1
        c.valstr("\x1f".join(flags))
        cups.append(c.finalize())
    return cups
