"""twin for cron/jobs.py:clear_drift_alerted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "clear_drift_alerted"
    C_FN = "cronjobs_clear_drift_alerted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
