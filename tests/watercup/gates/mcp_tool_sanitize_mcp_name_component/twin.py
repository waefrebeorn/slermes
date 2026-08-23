"""twin for mcp_tool_sanitize_mcp_name_component.

Traces LIVE tools/mcp_tool.py::sanitize_mcp_name_component with the same rung
order as the C harness (src/tools/mcp_tool.c::
mcp_tool_sanitize_mcp_name_component):
  B1 null / LOOP per-char sanitize.
Hyphen-bearing inputs are excluded — LIVE maps '-' to '_' while the C port
preserves it (documented divergence in gate.json).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_mcp_tool2", os.path.join(DEV, "tools", "mcp_tool.py"))
_mt = ilu.module_from_spec(_spec)
sys.modules["wc_live_mcp_tool2"] = _mt
_spec.loader.exec_module(_mt)


def trace(raw):
    c = Cup("sanitize_mcp_name_component")
    name = None if raw == "NULL" else raw
    if name is None:                                 # B1
        c.branch(True)
        res = "unknown"
    else:
        c.branch(False)
        kept = []
        for ch in str(name):                         # LOOP chars
            c.loop()
            kept.append(ch if (ch.isalnum() and ch.isascii()) or ch == "_" else "_")
        res = "".join(kept)
    live = _mt.sanitize_mcp_name_component(name if name is not None else "")
    assert live == res, (raw, live, res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        _, cup = trace(raw)
        cups.append(cup)
    return cups
