"""twin for cron/lifecycle_guard.py:_direct_lifecycle_scan (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_direct_lifecycle_scan"
    C_FN = "cron_lifecycle_direct_lifecycle_scan"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
