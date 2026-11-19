"""twin for tools/self_repo_guard.py:_executable_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_executable_name"
    C_FN = "_executable_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
