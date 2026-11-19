"""twin for agent/curator_backup.py:_backup_cron_jobs_into (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator_backup.py"
    PY_FN = "_backup_cron_jobs_into"
    C_FN = "_backup_cron_jobs_into"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
