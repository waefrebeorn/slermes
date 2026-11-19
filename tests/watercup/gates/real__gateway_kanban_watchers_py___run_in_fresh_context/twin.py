"""twin for gateway/kanban_watchers.py:_run_in_fresh_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/kanban_watchers.py"
    PY_FN = "_run_in_fresh_context"
    C_FN = "_run_in_fresh_context"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
