"""twin for tools/setup_mcp_tool.py:setup_mcp_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/setup_mcp_tool.py"
    PY_FN = "setup_mcp_tool"
    C_FN = "setup_mcp_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
