"""twin for tools/lazy_deps.py:_refresh_features (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/lazy_deps.py"
    PY_FN = "_refresh_features"
    C_FN = "_refresh_features"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
