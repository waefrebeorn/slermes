"""twin for agent/transports/base.py:extract_cache_stats (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/base.py"
    PY_FN = "extract_cache_stats"
    C_FN = "extract_cache_stats"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
