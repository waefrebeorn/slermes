"""twin for cron/lifecycle_guard.py:check_gateway_lifecycle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "check_gateway_lifecycle"
    C_FN = "check_gateway_lifecycle"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
