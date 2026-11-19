"""twin for hermes_cli/kanban_db.py:reconcile_orphaned_running (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "reconcile_orphaned_running"
    C_FN = "reconcile_orphaned_running"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
