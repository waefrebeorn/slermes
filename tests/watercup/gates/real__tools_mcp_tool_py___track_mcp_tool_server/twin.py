"""twin for tools/mcp_tool.py:_track_mcp_tool_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_track_mcp_tool_server"
    C_FN = "mcp_tool_track_mcp_tool_server"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
