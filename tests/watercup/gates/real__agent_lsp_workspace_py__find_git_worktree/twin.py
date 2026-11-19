"""twin for agent/lsp/workspace.py:find_git_worktree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/workspace.py"
    PY_FN = "find_git_worktree"
    C_FN = "find_git_worktree"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
