"""twin for cron/jobs.py:_validate_job_mode_invariants (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_validate_job_mode_invariants"
    C_FN = "_validate_job_mode_invariants"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
