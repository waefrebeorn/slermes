"""twin for tools/browser_camofox.py:_drop_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_camofox.py"
    PY_FN = "_drop_session"
    C_FN = "_drop_session"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
