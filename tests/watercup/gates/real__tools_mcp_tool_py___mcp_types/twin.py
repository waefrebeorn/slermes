"""twin for tools/mcp_tool.py:_mcp_types (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_mcp_types"
    C_FN = "_mcp_types"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
