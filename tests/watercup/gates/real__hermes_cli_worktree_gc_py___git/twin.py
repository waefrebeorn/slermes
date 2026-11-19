"""twin for hermes_cli/worktree_gc.py:_git (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/worktree_gc.py"
    PY_FN = "_git"
    C_FN = "_git"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
