"""twin for agent/insights.py:_compute_activity_patterns (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/insights.py"
    PY_FN = "_compute_activity_patterns"
    C_FN = "_compute_activity_patterns"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
