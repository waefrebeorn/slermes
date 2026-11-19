"""twin for tools/read_terminal_tool.py:read_terminal_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/read_terminal_tool.py"
    PY_FN = "read_terminal_tool"
    C_FN = "read_terminal_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
