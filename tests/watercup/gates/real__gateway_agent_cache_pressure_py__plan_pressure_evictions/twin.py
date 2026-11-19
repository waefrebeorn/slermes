"""twin for gateway/agent_cache_pressure.py:plan_pressure_evictions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "plan_pressure_evictions"
    C_FN = "plan_pressure_evictions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
