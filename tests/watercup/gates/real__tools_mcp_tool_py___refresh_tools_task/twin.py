"""twin for tools/mcp_tool.py:_refresh_tools_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_refresh_tools_task"
    C_FN = "mcp_tool_refresh_tools_task"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
