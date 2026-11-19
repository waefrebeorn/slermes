"""twin for tools/code_kernel.py:_resolve_owner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel.py"
    PY_FN = "_resolve_owner"
    C_FN = "_resolve_owner"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
