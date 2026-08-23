"""twin for clarify_tool_strip_recommended.

Imports LIVE tools/clarify_tool.py::strip_recommended and traces the SAME
ladder as the C harness (clarify_strip_recommended):
  B1 None/empty / B2 len>=label / B3 case-insensitive suffix hit.
The final value is LIVE Python's own return.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location("wc_clarify", os.path.join(DEV, "tools", "clarify_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_clarify"] = mod
spec.loader.exec_module(mod)

LABEL = "(Recommended)"


def trace(text):
    c = Cup("strip_recommended")
    if text is None:                                   # B1
        c.branch(True)
        out = ""
    else:
        c.branch(False)
        out = str(text).strip()
        if len(out) >= len(LABEL):                     # B2
            c.branch(True)
            if out[-len(LABEL):].casefold() == LABEL.casefold():   # B3
                c.branch(True)
                out = out[:-len(LABEL)].strip()
            else:
                c.branch(False)
        else:
            c.branch(False)
    c.valstr(out)
    return out, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        text = raw
        live = mod.strip_recommended(text)
        out, cup = trace(text)
        assert out == live, f"LIVE diverge on {raw!r}: {live!r} vs {out!r}"
        cups.append(cup)
    return cups
