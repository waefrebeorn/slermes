"""twin for tools/mcp_tool.py:_handle_stdio_child_exited_and_retry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_handle_stdio_child_exited_and_retry"
    C_FN = "_handle_stdio_child_exited_and_retry"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
