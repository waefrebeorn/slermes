"""twin for hermes_cli/kanban_db.py:_reclaim_dangling_run (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_reclaim_dangling_run"
    C_FN = "_reclaim_dangling_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
