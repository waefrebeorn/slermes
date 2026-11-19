"""twin for tools/slash_confirm.py:resolve (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/slash_confirm.py"
    PY_FN = "resolve"
    C_FN = "resolve"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
