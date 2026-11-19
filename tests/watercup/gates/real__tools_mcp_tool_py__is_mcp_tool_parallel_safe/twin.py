"""twin for tools/mcp_tool.py:is_mcp_tool_parallel_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "is_mcp_tool_parallel_safe"
    C_FN = "mcp_is_tool_parallel_safe"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
