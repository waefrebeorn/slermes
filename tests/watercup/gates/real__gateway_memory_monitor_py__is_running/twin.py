"""twin for gateway/memory_monitor.py:is_running (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_monitor.py"
    PY_FN = "is_running"
    C_FN = "is_alive"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
