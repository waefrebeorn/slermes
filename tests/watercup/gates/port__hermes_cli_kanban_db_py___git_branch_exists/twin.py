"""twin for hermes_cli/kanban_db.py:_git_branch_exists (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_git_branch_exists"
    C_FN = "kdb_git_branch_exists"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
