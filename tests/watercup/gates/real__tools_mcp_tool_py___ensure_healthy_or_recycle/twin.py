"""twin for tools/mcp_tool.py:_ensure_healthy_or_recycle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_ensure_healthy_or_recycle"
    C_FN = "mcp_tool_ensure_healthy_or_recycle"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
