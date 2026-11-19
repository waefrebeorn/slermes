"""twin for tools/computer_use/tool.py:_bounds_scale (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "_bounds_scale"
    C_FN = "_bounds_scale"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
