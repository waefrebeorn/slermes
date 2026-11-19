"""twin for cron/scheduler.py:_open_continuable_cron_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_open_continuable_cron_thread"
    C_FN = "_open_continuable_cron_thread"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
