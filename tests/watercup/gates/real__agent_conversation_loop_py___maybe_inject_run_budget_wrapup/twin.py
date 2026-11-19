"""twin for agent/conversation_loop.py:_maybe_inject_run_budget_wrapup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_maybe_inject_run_budget_wrapup"
    C_FN = "_maybe_inject_run_budget_wrapup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
