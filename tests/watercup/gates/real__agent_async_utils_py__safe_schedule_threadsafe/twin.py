"""twin for agent/async_utils.py:safe_schedule_threadsafe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/async_utils.py"
    PY_FN = "safe_schedule_threadsafe"
    C_FN = "async_schedule_threadsafe"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
