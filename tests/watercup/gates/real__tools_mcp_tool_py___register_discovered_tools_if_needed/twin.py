"""twin for tools/mcp_tool.py:_register_discovered_tools_if_needed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_register_discovered_tools_if_needed"
    C_FN = "mcp_register_discovered_tools_if_needed"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
