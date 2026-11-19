"""twin for hermes_cli/mcp_config.py:cmd_mcp_configure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_config.py"
    PY_FN = "cmd_mcp_configure"
    C_FN = "cmd_mcp_configure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
