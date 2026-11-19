"""twin for tools/mcp_tool.py:_mark_lifecycle_started (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_mark_lifecycle_started"
    C_FN = "mcp_mark_lifecycle_started"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
