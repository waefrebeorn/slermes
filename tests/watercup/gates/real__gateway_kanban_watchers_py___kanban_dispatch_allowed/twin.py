"""twin for gateway/kanban_watchers.py:_kanban_dispatch_allowed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/kanban_watchers.py"
    PY_FN = "_kanban_dispatch_allowed"
    C_FN = "_kanban_dispatch_allowed"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
