"""twin for tools/mcp_tool.py:_get_connected_server_for_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_get_connected_server_for_call"
    C_FN = "mcp_get_connected_server_for_call"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
