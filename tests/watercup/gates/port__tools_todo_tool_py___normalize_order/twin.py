"""twin for tools/todo_tool.py:_normalize_order (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/todo_tool.py"
    PY_FN = "_normalize_order"
    C_FN = "tdt_normalize_order"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
