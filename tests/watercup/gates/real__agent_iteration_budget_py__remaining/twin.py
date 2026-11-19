"""twin for agent/iteration_budget.py:remaining (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/iteration_budget.py"
    PY_FN = "remaining"
    C_FN = "itb_remaining"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
