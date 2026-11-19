"""twin for tools/mcp_tool.py:has_registered_mcp_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "has_registered_mcp_tools"
    C_FN = "mct_has_registered_mcp_tools"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
