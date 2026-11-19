"""twin for tools/kanban_tools.py:_enforce_worker_task_ownership (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/kanban_tools.py"
    PY_FN = "_enforce_worker_task_ownership"
    C_FN = "_enforce_worker_task_ownership"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
