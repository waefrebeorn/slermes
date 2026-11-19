"""twin for cli.py:_cleanup_failed_worktree_add (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_cleanup_failed_worktree_add"
    C_FN = "cli_cleanup_failed_worktree_add"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
