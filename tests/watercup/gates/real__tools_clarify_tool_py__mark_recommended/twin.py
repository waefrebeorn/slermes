"""twin for tools/clarify_tool.py:mark_recommended (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_tool.py"
    PY_FN = "mark_recommended"
    C_FN = "mark_recommended"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
