"""twin for hermes_cli/worktree_cmd.py:_repo_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/worktree_cmd.py"
    PY_FN = "_repo_root"
    C_FN = "worktree_cmd_repo_root"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
