"""twin for gateway/run.py:_reset_hygiene_failure_streak (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_reset_hygiene_failure_streak"
    C_FN = "gw_reset_hygiene_failure_streak"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
