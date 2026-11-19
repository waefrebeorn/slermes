"""twin for tools/mcp_tool.py:_is_recycled_stdio (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_is_recycled_stdio"
    C_FN = "mcp_is_recycled_stdio"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
