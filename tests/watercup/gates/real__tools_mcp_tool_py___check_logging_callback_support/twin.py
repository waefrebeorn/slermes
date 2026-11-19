"""twin for tools/mcp_tool.py:_check_logging_callback_support (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_check_logging_callback_support"
    C_FN = "mcp_check_logging_callback_support"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
