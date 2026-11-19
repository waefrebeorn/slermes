"""twin for gateway/runtime_footer.py:_home_relative_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/runtime_footer.py"
    PY_FN = "_home_relative_cwd"
    C_FN = "_home_relative_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
