"""twin for tools/mcp_tool.py:get_registered_mcp_server_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "get_registered_mcp_server_names"
    C_FN = "get_registered_mcp_server_names"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
