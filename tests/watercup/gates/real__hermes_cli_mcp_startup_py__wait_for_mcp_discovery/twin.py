"""twin for hermes_cli/mcp_startup.py:wait_for_mcp_discovery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_startup.py"
    PY_FN = "wait_for_mcp_discovery"
    C_FN = "wait_for_mcp_discovery"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
