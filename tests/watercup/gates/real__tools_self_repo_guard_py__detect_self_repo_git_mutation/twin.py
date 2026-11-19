"""twin for tools/self_repo_guard.py:detect_self_repo_git_mutation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "detect_self_repo_git_mutation"
    C_FN = "srg_detect_self_repo_git_mutation"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
