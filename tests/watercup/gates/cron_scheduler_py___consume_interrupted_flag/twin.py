"""twin for cron/scheduler.py:_consume_interrupted_flag (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_consume_interrupted_flag"
    C_FN = "scheduler_consume_interrupted_flag"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
