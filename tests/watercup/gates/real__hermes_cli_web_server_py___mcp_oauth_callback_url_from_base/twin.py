"""twin for hermes_cli/web_server.py:_mcp_oauth_callback_url_from_base (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_mcp_oauth_callback_url_from_base"
    C_FN = "_mcp_oauth_callback_url_from_base"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
