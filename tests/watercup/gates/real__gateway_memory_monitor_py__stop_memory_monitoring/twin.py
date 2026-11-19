"""twin for gateway/memory_monitor.py:stop_memory_monitoring (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_monitor.py"
    PY_FN = "stop_memory_monitoring"
    C_FN = "stop_memory_monitoring"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
