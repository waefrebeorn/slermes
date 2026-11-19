"""twin for hermes_cli/mcp_config.py:_success (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_config.py"
    PY_FN = "_success"
    C_FN = "desktop_notify_success"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
