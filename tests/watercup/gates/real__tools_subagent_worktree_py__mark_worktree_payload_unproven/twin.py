"""twin for tools/subagent_worktree.py:mark_worktree_payload_unproven (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/subagent_worktree.py"
    PY_FN = "mark_worktree_payload_unproven"
    C_FN = "mark_worktree_payload_unproven"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
