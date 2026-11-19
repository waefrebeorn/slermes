"""twin for tools/mcp_tool.py:sanitize_mcp_name_component (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "sanitize_mcp_name_component"
    C_FN = "sanitize_mcp_name_component"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
