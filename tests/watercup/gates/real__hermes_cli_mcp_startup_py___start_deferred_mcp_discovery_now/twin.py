"""twin for hermes_cli/mcp_startup.py:_start_deferred_mcp_discovery_now (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_startup.py"
    PY_FN = "_start_deferred_mcp_discovery_now"
    C_FN = "_start_deferred_mcp_discovery_now"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
