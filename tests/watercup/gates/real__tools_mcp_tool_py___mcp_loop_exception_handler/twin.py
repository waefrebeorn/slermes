"""twin for tools/mcp_tool.py:_mcp_loop_exception_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_mcp_loop_exception_handler"
    C_FN = "mcp_tool_mcp_loop_exception_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
