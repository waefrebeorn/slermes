"""twin for tools/mcp_oauth.py:_server_declined_cimd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "_server_declined_cimd"
    C_FN = "mcpo_server_declined_cimd"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
