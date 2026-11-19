"""twin for hermes_cli/web_routers/mcp.py:set_mcp_server_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/mcp.py"
    PY_FN = "set_mcp_server_enabled"
    C_FN = "set_mcp_server_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
