"""twin for croniter:croniter.get_next (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "croniter"
    PY_FN = "croniter.get_next"
    C_FN = "croniter.get_next"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
