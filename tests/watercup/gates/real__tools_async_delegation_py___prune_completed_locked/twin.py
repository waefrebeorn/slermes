"""twin for tools/async_delegation.py:_prune_completed_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "_prune_completed_locked"
    C_FN = "prune_completed_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
