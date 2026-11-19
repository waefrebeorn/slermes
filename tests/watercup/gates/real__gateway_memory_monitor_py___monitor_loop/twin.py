"""twin for gateway/memory_monitor.py:_monitor_loop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_monitor.py"
    PY_FN = "_monitor_loop"
    C_FN = "_monitor_loop"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
