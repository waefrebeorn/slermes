"""twin for agent/nous_rate_guard.py:_has_exhausted_bucket (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"
    PY_FN = "_has_exhausted_bucket"
    C_FN = "nrg_has_exhausted_bucket"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
