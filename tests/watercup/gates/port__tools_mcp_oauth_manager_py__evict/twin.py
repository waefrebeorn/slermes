"""twin for tools/mcp_oauth_manager.py:evict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth_manager.py"
    PY_FN = "evict"
    C_FN = "cli_tools_mcp_oauth_manager_evict"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
