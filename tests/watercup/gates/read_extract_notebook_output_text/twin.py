"""twin for read_extract_notebook_output_text.

Imports LIVE tools/read_extract.py::_notebook_output_text and traces the SAME
coarse rung ladder as the C harness (which drives the REAL linked C port).
Inputs are JSON output dicts (one per cup).
"""
import json as _json
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_read_extract",
                                   os.path.join(DEV, "tools", "read_extract.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_read_extract"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("_notebook_output_text")
        result = ""
        if not raw:
            c.branch(True)                       # B1: empty
            c.valstr("")
            cups.append(c.finalize())
            continue
        c.branch(False)
        try:
            output = _json.loads(raw)
        except Exception:
            output = None
        if not isinstance(output, dict):
            output = None
        otype = output.get("output_type") if output else None
        if otype == "stream":
            c.branch(True)                       # B3
        else:
            c.branch(False)
            if otype in ("error", "pyerr"):
                c.branch(True)                   # B4
            else:
                c.branch(False)
                if otype in ("execute_result", "display_data", "pyout"):
                    c.branch(True)               # B5
                else:
                    c.branch(False)
        if output is not None:
            try:
                result = mod._notebook_output_text(output) or ""
            except Exception:
                result = ""
        c.valstr(result)
        cups.append(c.finalize())
    return cups
