"""twin for cron/scheduler.py:_is_lock_contention_errno (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_is_lock_contention_errno"
    C_FN = "_is_lock_contention_errno"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
