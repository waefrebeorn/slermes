"""twin for cron/jobs.py:_claim_job_for_fire_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_claim_job_for_fire_locked"
    C_FN = "_claim_job_for_fire_locked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
