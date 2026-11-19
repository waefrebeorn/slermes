"""twin for hermes_cli/web_routers/mcp.py:add_mcp_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/mcp.py"
    PY_FN = "add_mcp_server"
    C_FN = "add_mcp_server"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
