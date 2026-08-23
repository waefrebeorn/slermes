"""twin for clarify_tool_mark_recommended.

Imports LIVE tools/clarify_tool.py::mark_recommended and traces the SAME
ladder as the C harness (clarify_mark_recommended):
  B1 count<2 / B2 already-labeled suffix / decorate-first.
Choices arrive '\\x1f'-joined; the final value is LIVE Python's own return.
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


def trace(choices):
    c = Cup("mark_recommended")
    if len(choices) < 2:                                   # B1
        c.branch(True)
        result = list(choices)
    else:
        c.branch(False)
        first = str(choices[0]).strip()
        if first[-len(LABEL):].casefold() == LABEL.casefold():   # B2
            c.branch(True)
            result = list(choices)
        else:
            c.branch(False)
            result = [f"{first} {LABEL}"] + list(choices[1:])
    c.valstr("\x1f".join(result))
    return result, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        choices = raw.split("\x1f")
        live = mod.mark_recommended(list(choices))
        result, cup = trace(choices)
        assert result == live, f"LIVE diverge on {raw!r}: {live!r} vs {result!r}"
        cups.append(cup)
    return cups
