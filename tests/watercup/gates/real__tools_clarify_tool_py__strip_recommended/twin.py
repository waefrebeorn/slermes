"""twin for tools/clarify_tool.py:strip_recommended (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_tool.py"
    PY_FN = "strip_recommended"
    C_FN = "strip_recommended"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
