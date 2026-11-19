"""twin for gateway/run.py:_loop_wakeup_watcher (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_loop_wakeup_watcher"
    C_FN = "_loop_wakeup_watcher"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
