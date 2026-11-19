"""twin for tools/mcp_tool.py:_discover_and_register_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_discover_and_register_server"
    C_FN = "mcp_tool_discover_and_register_server"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
