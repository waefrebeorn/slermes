"""twin for hermes_cli/mcp_startup.py:set_mcp_server_filter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_startup.py"
    PY_FN = "set_mcp_server_filter"
    C_FN = "set_mcp_server_filter"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
