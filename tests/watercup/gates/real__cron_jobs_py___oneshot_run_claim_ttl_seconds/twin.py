"""twin for cron/jobs.py:_oneshot_run_claim_ttl_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_oneshot_run_claim_ttl_seconds"
    C_FN = "cronjobs_oneshot_run_claim_ttl_seconds"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
