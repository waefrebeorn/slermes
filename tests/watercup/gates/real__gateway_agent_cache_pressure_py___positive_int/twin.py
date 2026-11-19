"""twin for gateway/agent_cache_pressure.py:_positive_int (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "_positive_int"
    C_FN = "acb_positive_int"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
