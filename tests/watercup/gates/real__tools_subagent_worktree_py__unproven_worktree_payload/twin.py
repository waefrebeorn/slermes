"""twin for tools/subagent_worktree.py:unproven_worktree_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/subagent_worktree.py"
    PY_FN = "unproven_worktree_payload"
    C_FN = "unproven_worktree_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
