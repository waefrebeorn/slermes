"""twin for mcp_tool_safe_numeric.

Traces LIVE tools/mcp_tool.py::_safe_numeric(value, default, coerce=int,
minimum=1) with the same rung order as the C harness
(src/tools/mcp_tool.c::mcp_tool_safe_numeric):
  B1 null / B2 unparseable.
Inputs are "<value|NULL>\t<default>" tokens restricted to the agreement
region (see gate.json notes): integer-parseable strings whose int() result is
>= the minimum clamp of 1.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_mcp_tool", os.path.join(DEV, "tools", "mcp_tool.py"))
_mt = ilu.module_from_spec(_spec)
sys.modules["wc_live_mcp_tool"] = _mt
_spec.loader.exec_module(_mt)


def trace(raw_value, default):
    c = Cup("safe_numeric")
    value = None if raw_value == "NULL" else raw_value
    if value is None:                                # B1
        c.branch(True)
        res = default
    else:
        c.branch(False)
        try:
            res = int(value)                          # parse
            ok = True
        except (TypeError, ValueError):
            ok = False
            res = default
        c.branch(not ok)                              # B2 (failed parse)
        if not ok:
            res = default
    live = _mt._safe_numeric(value, default, coerce=int, minimum=1)
    assert live == res, (raw_value, default, live, res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for tok in inputs:
        raw, default = tok.split("\t")
        _, cup = trace(raw, float(default))
        cups.append(cup)
    return cups
