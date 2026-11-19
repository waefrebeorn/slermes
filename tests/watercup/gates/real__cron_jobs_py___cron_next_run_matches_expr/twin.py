"""twin for cron/jobs.py:_cron_next_run_matches_expr (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_cron_next_run_matches_expr"
    C_FN = "_cron_next_run_matches_expr"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
