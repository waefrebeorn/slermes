"""twin for gateway/kanban_watchers.py:_release_kanban_dispatcher_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/kanban_watchers.py"
    PY_FN = "_release_kanban_dispatcher_lock"
    C_FN = "held"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
