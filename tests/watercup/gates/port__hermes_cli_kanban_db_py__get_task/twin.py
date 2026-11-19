"""twin for hermes_cli/kanban_db.py:get_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "get_task"
    C_FN = "kdb_task_get"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
