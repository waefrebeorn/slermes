"""twin for hermes_cli/mcp_security.py:validate_mcp_server_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_security.py"
    PY_FN = "validate_mcp_server_entry"
    C_FN = "validate_mcp_server_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
