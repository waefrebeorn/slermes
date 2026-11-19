"""twin for tools/mcp_oauth_manager.py:get_manager (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth_manager.py"
    PY_FN = "get_manager"
    C_FN = "get_manager"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
