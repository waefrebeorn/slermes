"""twin for tools/subagent_worktree.py:build_worktree_context_note (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/subagent_worktree.py"
    PY_FN = "build_worktree_context_note"
    C_FN = "build_worktree_context_note"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
