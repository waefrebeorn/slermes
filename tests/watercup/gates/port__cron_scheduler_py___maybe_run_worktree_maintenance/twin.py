"""twin for cron/scheduler.py:_maybe_run_worktree_maintenance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_maybe_run_worktree_maintenance"
    C_FN = "_maybe_run_worktree_maintenance"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
