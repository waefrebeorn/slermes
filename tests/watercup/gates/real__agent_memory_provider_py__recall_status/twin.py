"""twin for agent/memory_provider.py:recall_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_provider.py"
    PY_FN = "recall_status"
    C_FN = "recall_status"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
