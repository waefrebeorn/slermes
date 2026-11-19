"""twin for tools/close_terminal_tool.py:close_terminal_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/close_terminal_tool.py"
    PY_FN = "close_terminal_tool"
    C_FN = "check_close_terminal_requirements"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
