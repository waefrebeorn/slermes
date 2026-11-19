"""twin for cron/scheduler.py:_shutdown_parallel_pool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_shutdown_parallel_pool"
    C_FN = "scheduler_shutdown_parallel_pool"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
