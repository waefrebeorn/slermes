"""twin for agent/retry_utils.py:adaptive_rate_limit_backoff (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/retry_utils.py"
    PY_FN = "adaptive_rate_limit_backoff"
    C_FN = "rtu_adaptive_rate_limit_backoff"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
