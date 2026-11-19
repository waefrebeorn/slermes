"""twin for hermes_cli/kanban_db.py:_repo_root_for_worktree_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_repo_root_for_worktree_target"
    C_FN = "_repo_root_for_worktree_target"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
