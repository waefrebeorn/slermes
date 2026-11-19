"""twin for tools/self_repo_guard.py:_git_target_and_subcommand (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_git_target_and_subcommand"
    C_FN = "srg_git_target_and_subcommand"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
