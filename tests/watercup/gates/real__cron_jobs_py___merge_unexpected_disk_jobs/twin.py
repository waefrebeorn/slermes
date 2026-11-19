"""twin for cron/jobs.py:_merge_unexpected_disk_jobs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_merge_unexpected_disk_jobs"
    C_FN = "_merge_unexpected_disk_jobs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
