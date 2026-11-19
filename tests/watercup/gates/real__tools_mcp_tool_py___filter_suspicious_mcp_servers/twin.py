"""twin for tools/mcp_tool.py:_filter_suspicious_mcp_servers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_filter_suspicious_mcp_servers"
    C_FN = "_filter_suspicious_mcp_servers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
