"""twin for tools/subagent_worktree.py:_ensure_gitignore_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/subagent_worktree.py"
    PY_FN = "_ensure_gitignore_entry"
    C_FN = "_ensure_gitignore_entry"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
