"""twin for tools/self_repo_guard.py:_scratch_dir_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_scratch_dir_hint"
    C_FN = "_scratch_dir_hint"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
