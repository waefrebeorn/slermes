"""twin for tools/mcp_tool.py:_try_acquire_mcp_discovery_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_try_acquire_mcp_discovery_lock"
    C_FN = "MCP_LOCK_COOKIE"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
