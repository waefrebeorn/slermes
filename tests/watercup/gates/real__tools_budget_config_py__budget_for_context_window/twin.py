"""twin for tools/budget_config.py:budget_for_context_window (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/budget_config.py"
    PY_FN = "budget_for_context_window"
    C_FN = "budget_for_context_window"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
