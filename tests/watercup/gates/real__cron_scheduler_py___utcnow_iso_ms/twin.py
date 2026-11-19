"""twin for cron/scheduler.py:_utcnow_iso_ms (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_utcnow_iso_ms"
    C_FN = "_utcnow_iso_ms"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
