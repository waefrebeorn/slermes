"""twin for agent/turn_finalizer.py:_record_kanban_budget_exhausted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_finalizer.py"
    PY_FN = "_record_kanban_budget_exhausted"
    C_FN = "_record_kanban_budget_exhausted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
