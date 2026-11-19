"""twin for tools/mcp_tool.py:_stdio_children_dead (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_stdio_children_dead"
    C_FN = "mcp_tool_stdio_children_dead"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
