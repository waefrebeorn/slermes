"""twin for hermes_cli/mcp_catalog.py:installed_servers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_catalog.py"
    PY_FN = "installed_servers"
    C_FN = "installed_servers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
