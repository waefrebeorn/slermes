"""twin for tools/browser_tool.py:_camofox_eval (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_camofox_eval"
    C_FN = "_camofox_eval"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
