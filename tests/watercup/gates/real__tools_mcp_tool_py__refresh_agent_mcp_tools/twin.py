"""twin for tools/mcp_tool.py:refresh_agent_mcp_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "refresh_agent_mcp_tools"
    C_FN = "mcp_init_all"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
