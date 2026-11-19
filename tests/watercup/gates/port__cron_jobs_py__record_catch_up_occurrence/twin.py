"""twin for cron/jobs.py:record_catch_up_occurrence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "record_catch_up_occurrence"
    C_FN = "record_catch_up_occurrence"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
