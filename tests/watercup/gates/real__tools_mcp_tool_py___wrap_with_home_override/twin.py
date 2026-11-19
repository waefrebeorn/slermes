"""twin for tools/mcp_tool.py:_wrap_with_home_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_wrap_with_home_override"
    C_FN = "_wrap_with_home_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
