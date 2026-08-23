"""twin for path_security_traversal.

Traces the LIVE tools/path_security.py::has_traversal_component branch
ladder with the SAME rung order as the C harness:
  one B event per Path.parts component, compared == ".."; then VSTR(result).
The LIVE fn's own result is asserted against the mirrored ladder outcome.
"""
import os, sys, importlib.util as ilu
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_path_security",
                                    os.path.join(DEV, "tools", "path_security.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_path_security"] = _live
_spec.loader.exec_module(_live)


def trace(path_str):
    parts = Path(path_str).parts if path_str else ()
    c = Cup("path_security_traversal")
    found = False
    for part in parts:
        if set(part) <= {"/"}:   # '/' or '//' — separators, not components
            continue
        is_dotdot = (part == "..")
        c.branch(is_dotdot)
        if is_dotdot:
            found = True
            break
    mirrored = "1" if found else "0"
    c.valstr(mirrored)

    live = _live.has_traversal_component(path_str)
    assert (live and mirrored == "1") or (not live and mirrored == "0"), \
        "LIVE result %r disagrees with mirrored ladder for %r" % (live, path_str)
    return c.finalize()


def run(inputs):
    return [trace(text) for text in inputs]
