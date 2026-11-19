"""twin for agent/nous_rate_guard.py:nous_rate_limit_remaining (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"
    PY_FN = "nous_rate_limit_remaining"
    C_FN = "nrg_nous_rate_limit_remaining"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
