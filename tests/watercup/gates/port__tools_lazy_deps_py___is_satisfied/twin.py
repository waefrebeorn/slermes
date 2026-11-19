"""twin for tools/lazy_deps.py:_is_satisfied (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/lazy_deps.py"
    PY_FN = "_is_satisfied"
    C_FN = "tools_lazy_deps_u_is_satisfied"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
