"""twin for hermes_cli/kanban_db.py:_is_linked_worktree_checkout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_is_linked_worktree_checkout"
    C_FN = "kdb_is_linked_worktree_checkout"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
