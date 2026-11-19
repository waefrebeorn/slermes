"""twin for tools/mcp_oauth_manager.py:_same_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth_manager.py"
    PY_FN = "_same_endpoint"
    C_FN = "_same_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
