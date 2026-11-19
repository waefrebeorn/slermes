"""twin for hermes_cli/web_routers/mcp.py:mcp_oauth_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/mcp.py"
    PY_FN = "mcp_oauth_callback"
    C_FN = "mcp_oauth_callback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
