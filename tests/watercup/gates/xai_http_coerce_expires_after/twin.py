"""twin for xai_http_coerce_expires_after.

Imports LIVE tools/xai_http.py::_coerce_expires_after and traces the SAME
rung ladder as cli_tools_xai_http__coerce_expires_after:

    C:   null -> keyword set -> strtol parse -> <=0 -> clamp MAX
    PY:  None -> {"","default"} then {"none",...,"0"} -> int() -> <=0 -> min(MAX)

Value events: final encoded outcome ("null" / decimal string / SAFE "172800").
argv inputs are strings, so the LIVE str/int/float type ladder is exercised
on its string rung only (documented).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_xai_http",
                                   os.path.join(DEV, "tools", "xai_http.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_xai_http"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

MAX_EXP = mod.MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS
SAFE = mod.SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS


def trace(value):
    c = Cup("coerce_expires_after")
    live = mod._coerce_expires_after(value)
    # C NULL rung is unreachable via argv (all inputs are strings) -> B(False)
    c.branch(False)
    b = value.strip().lower()
    keywords = ("", "default", "none", "null", "never", "permanent", "forever", "0")
    if b in keywords:
        c.branch(True)
        c.valstr("null")
        return c.finalize()
    c.branch(False)
    try:
        seconds = int(b)
        parsed = True
    except ValueError:
        parsed = False
    if parsed:
        c.branch(True)
        if seconds <= 0:
            c.branch(True)
            c.valstr("null")
            return c.finalize()
        c.branch(False)
        c.valstr(str(min(seconds, MAX_EXP)))
        return c.finalize()
    c.branch(False)
    c.valstr(str(SAFE))
    return c.finalize()


def run(inputs):
    return [trace(v) for v in inputs]
