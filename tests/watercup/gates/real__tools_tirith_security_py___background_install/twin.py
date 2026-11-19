"""twin for tools/tirith_security.py:_background_install (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tirith_security.py"
    PY_FN = "_background_install"
    C_FN = "_background_install"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
