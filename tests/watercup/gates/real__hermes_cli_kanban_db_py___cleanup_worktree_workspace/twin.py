"""twin for hermes_cli/kanban_db.py:_cleanup_worktree_workspace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_cleanup_worktree_workspace"
    C_FN = "_cleanup_worktree_workspace"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
