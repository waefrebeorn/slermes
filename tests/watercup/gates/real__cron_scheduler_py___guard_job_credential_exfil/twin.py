"""twin for cron/scheduler.py:_guard_job_credential_exfil (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_guard_job_credential_exfil"
    C_FN = "_guard_job_credential_exfil"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
