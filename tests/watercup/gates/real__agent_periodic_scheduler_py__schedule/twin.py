"""twin for agent/periodic_scheduler.py:schedule (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/periodic_scheduler.py"
    PY_FN = "schedule"
    C_FN = "schedule"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
