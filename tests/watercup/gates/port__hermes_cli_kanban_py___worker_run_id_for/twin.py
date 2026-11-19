"""twin for hermes_cli/kanban.py:_worker_run_id_for (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban.py"
    PY_FN = "_worker_run_id_for"
    C_FN = "kanban_worker_run_id_for"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
