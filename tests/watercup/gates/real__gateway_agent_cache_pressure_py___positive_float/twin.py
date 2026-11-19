"""twin for gateway/agent_cache_pressure.py:_positive_float (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "_positive_float"
    C_FN = "acb_positive_float"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
