"""twin for tools/subagent_worktree.py:finalize_subagent_worktree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/subagent_worktree.py"
    PY_FN = "finalize_subagent_worktree"
    C_FN = "finalize_subagent_worktree"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
