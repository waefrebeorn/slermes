"""twin for tools/mcp_tool.py:_wrap_with_dashboard_oauth_flow (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_wrap_with_dashboard_oauth_flow"
    C_FN = "mcp_wrap_dashboard_oauth_flow"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
