"""twin for tools/todo_tool.py:format_for_injection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/todo_tool.py"
    PY_FN = "format_for_injection"
    C_FN = "format_for_injection"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
