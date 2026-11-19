"""twin for hermes_cli/web_routers/mcp.py:list_mcp_servers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/mcp.py"
    PY_FN = "list_mcp_servers"
    C_FN = "list_mcp_servers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
