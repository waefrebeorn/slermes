"""twin for cron/jobs.py:_stored_wall_clock_is_future (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_stored_wall_clock_is_future"
    C_FN = "stored_wall_clock_is_future"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
