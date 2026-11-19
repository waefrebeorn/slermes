"""twin for tools/mcp_tool.py:_fail_inflight_calls (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_fail_inflight_calls"
    C_FN = "mcp_tool_fail_inflight_calls"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
