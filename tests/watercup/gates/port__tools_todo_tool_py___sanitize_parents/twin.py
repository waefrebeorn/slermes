"""twin for tools/todo_tool.py:_sanitize_parents (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/todo_tool.py"
    PY_FN = "_sanitize_parents"
    C_FN = "_sanitize_parents"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
