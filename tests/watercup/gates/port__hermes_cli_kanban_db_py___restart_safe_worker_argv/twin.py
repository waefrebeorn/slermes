"""twin for hermes_cli/kanban_db.py:_restart_safe_worker_argv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_restart_safe_worker_argv"
    C_FN = "_restart_safe_worker_argv"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
