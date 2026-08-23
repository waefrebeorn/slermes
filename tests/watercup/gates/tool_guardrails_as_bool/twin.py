"""twin for tool_guardrails_as_bool.

Imports LIVE agent/tool_guardrails.py::_as_bool and traces the SAME strcmp
branch ladder as the C harness (tool_guardrails_ports.c::tg_as_bool).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_tool_guardrails", os.path.join(DEV, "agent", "tool_guardrails.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_tool_guardrails"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

TRUE_TOKENS = ("true", "1", "yes", "on")
FALSE_TOKENS = ("false", "0", "no", "off")


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("tg_as_bool")
        value = None if raw == "null" else raw
        if value is None:
            c.branch(True)
            res = "1"  # default=True on both sides
        else:
            c.branch(False)
            l = value.lower()
            r = -1
            for i, tok in enumerate(TRUE_TOKENS + FALSE_TOKENS):
                if l == tok:
                    c.branch(True)
                    r = 1 if i < len(TRUE_TOKENS) else 0
                    break
                c.branch(False)
            else:
                r = 1  # default=True
            res = str(r)
        c.valstr(res)
        cups.append(c.finalize())
    return cups
