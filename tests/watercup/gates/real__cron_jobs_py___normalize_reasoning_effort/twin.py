"""twin for cron/jobs.py:_normalize_reasoning_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_normalize_reasoning_effort"
    C_FN = "_normalize_reasoning_effort"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
