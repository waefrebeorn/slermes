"""twin for tools/async_delegation.py:_begin_finalization (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "_begin_finalization"
    C_FN = "_begin_finalization"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
