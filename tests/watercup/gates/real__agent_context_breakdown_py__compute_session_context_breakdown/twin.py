"""twin for agent/context_breakdown.py:compute_session_context_breakdown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_breakdown.py"
    PY_FN = "compute_session_context_breakdown"
    C_FN = "compute_session_context_breakdown"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
