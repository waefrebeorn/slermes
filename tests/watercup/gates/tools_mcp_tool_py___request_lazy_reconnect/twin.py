"""twin for tools/mcp_tool.py:_request_lazy_reconnect (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_request_lazy_reconnect"
    C_FN = "mcp_request_lazy_reconnect"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
