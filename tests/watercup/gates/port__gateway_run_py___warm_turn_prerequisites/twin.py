"""twin for gateway/run.py:_warm_turn_prerequisites (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_warm_turn_prerequisites"
    C_FN = "_warm_turn_prerequisites"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
