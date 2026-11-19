"""twin for cron/jobs.py:normalize_repeat_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "normalize_repeat_value"
    C_FN = "normalize_repeat_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
