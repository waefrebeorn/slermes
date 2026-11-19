"""twin for agent/context_breakdown.py:_memory_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_breakdown.py"
    PY_FN = "_memory_blocks"
    C_FN = "_memory_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
