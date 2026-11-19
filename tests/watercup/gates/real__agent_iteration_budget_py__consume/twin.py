"""twin for agent/iteration_budget.py:consume (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/iteration_budget.py"
    PY_FN = "consume"
    C_FN = "itb_consume"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
