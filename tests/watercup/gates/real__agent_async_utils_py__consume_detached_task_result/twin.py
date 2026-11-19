"""twin for agent/async_utils.py:consume_detached_task_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/async_utils.py"
    PY_FN = "consume_detached_task_result"
    C_FN = "async_consume_detached"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
