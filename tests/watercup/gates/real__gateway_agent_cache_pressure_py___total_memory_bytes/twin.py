"""twin for gateway/agent_cache_pressure.py:_total_memory_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "_total_memory_bytes"
    C_FN = "_total_memory_bytes"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
