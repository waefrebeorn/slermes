"""twin for tools/self_repo_guard.py:_mask_heredocs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_mask_heredocs"
    C_FN = "srg_mask_heredocs"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
