"""twin for agent/memory_provider.py:unavailable_reason (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_provider.py"
    PY_FN = "unavailable_reason"
    C_FN = "unavailable_reason"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
