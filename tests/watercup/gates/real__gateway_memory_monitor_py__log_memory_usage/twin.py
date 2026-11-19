"""twin for gateway/memory_monitor.py:log_memory_usage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_monitor.py"
    PY_FN = "log_memory_usage"
    C_FN = "log_memory_usage"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
