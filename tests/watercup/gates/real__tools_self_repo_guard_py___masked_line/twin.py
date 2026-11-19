"""twin for tools/self_repo_guard.py:_masked_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/self_repo_guard.py"
    PY_FN = "_masked_line"
    C_FN = "_masked_line"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
