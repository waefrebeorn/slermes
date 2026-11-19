"""twin for tools/clarify_gateway.py:register (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_gateway.py"
    PY_FN = "register"
    C_FN = "register"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
