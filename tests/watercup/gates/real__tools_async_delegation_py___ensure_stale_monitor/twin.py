"""twin for tools/async_delegation.py:_ensure_stale_monitor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "_ensure_stale_monitor"
    C_FN = "async_delegation_ensure_stale_monitor"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
