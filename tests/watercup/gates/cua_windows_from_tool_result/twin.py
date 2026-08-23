"""twin for cua_windows_from_tool_result.

Imports LIVE tools/computer_use/cua_backend.py::_windows_from_tool_result and
traces the SAME shape-detection ladder as the harness around the REAL C port.
"""
import os, sys, json as _json
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location(
    "wc_cua_backend", os.path.join(DEV, "tools", "computer_use", "cua_backend.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cua_backend"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def _is_list_nonempty(x):
    return isinstance(x, list) and bool(x)

def run(inputs):
    cups = []
    for inp in inputs:
        try:
            out = _json.loads(inp)
        except Exception:
            out = None
        c = Cup("windows_from_tool_result")
        if not isinstance(out, dict):
            c.branch(True)
            n = 0
        else:
            c.branch(False)
            structured = out.get("structuredContent")
            w = structured.get("windows") if isinstance(structured, dict) else None
            if _is_list_nonempty(w):
                c.branch(1)
            else:
                c.branch(0)
                data = out.get("data")
                if not isinstance(data, dict):
                    c.branch(1)
                else:
                    c.branch(0)
                    dw = data.get("windows")
                    if _is_list_nonempty(dw):
                        c.branch(1)
                    else:
                        c.branch(0)
                        lw = data.get("_legacy_windows")
                        if _is_list_nonempty(lw):
                            c.branch(1)
                        else:
                            c.branch(0)
                            tw = out.get("windows")
                            if _is_list_nonempty(tw):
                                c.branch(1)
                            else:
                                c.branch(0)
                                tl = out.get("_legacy_windows")
                                if _is_list_nonempty(tl):
                                    c.branch(1)
                                else:
                                    c.branch(0)
            res = mod._windows_from_tool_result(out)
            n = len(res)
        c.valstr(str(n))
        cups.append(c.finalize())
    return cups
