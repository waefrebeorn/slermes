"""twin for hermes_cli/mcp_config.py:mcp_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_config.py"
    PY_FN = "mcp_command"
    C_FN = "mcp_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
