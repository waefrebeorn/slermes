"""twin for gateway/run.py:_agent_cache_idle_ttl (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_agent_cache_idle_ttl"
    C_FN = "grun_u_agent_cache_idle_ttl"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
