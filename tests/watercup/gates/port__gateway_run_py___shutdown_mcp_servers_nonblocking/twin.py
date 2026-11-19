"""twin for gateway/run.py:_shutdown_mcp_servers_nonblocking (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_shutdown_mcp_servers_nonblocking"
    C_FN = "_shutdown_mcp_servers_nonblocking"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
