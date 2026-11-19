"""twin for cron/scheduler.py:_interpreter_shutting_down (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_interpreter_shutting_down"
    C_FN = "scheduler_interpreter_shutting_down"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
