"""twin for hermes_cli/plugins.py:has_enabled_portable_mcp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "has_enabled_portable_mcp"
    C_FN = "has_enabled_portable_mcp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
