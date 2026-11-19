"""twin for tools/tirith_security.py:_warn_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tirith_security.py"
    PY_FN = "_warn_once"
    C_FN = "_warn_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
