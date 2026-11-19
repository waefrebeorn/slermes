"""twin for tools/self_repo_guard.py:_inspect_git_worktree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_inspect_git_worktree"
    C_FN = "_inspect_git_worktree"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
