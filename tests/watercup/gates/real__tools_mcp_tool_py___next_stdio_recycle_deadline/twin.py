"""twin for tools/mcp_tool.py:_next_stdio_recycle_deadline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_next_stdio_recycle_deadline"
    C_FN = "mcp_next_stdio_recycle_deadline"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
