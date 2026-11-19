"""twin for cron/jobs.py:get_catch_up_occurrence_count (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "get_catch_up_occurrence_count"
    C_FN = "get_catch_up_occurrence_count"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
