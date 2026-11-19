"""twin for agent/deadline.py:run_bounded_sync (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/deadline.py"
    PY_FN = "run_bounded_sync"
    C_FN = "run_bounded_sync"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
