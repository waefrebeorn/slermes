"""twin for agent/insights.py:_compute_model_breakdown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/insights.py"
    PY_FN = "_compute_model_breakdown"
    C_FN = "_compute_model_breakdown"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
