"""twin for tools/lazy_deps.py:is_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/lazy_deps.py"
    PY_FN = "is_available"
    C_FN = "tools_lazy_deps_available"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
