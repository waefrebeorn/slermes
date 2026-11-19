"""twin for cron/jobs.py:mark_preflight_alerted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "mark_preflight_alerted"
    C_FN = "cronjobs_mark_preflight_alerted"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
