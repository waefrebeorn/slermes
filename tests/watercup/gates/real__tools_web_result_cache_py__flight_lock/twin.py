"""twin for tools/web_result_cache.py:flight_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_result_cache.py"
    PY_FN = "flight_lock"
    C_FN = "flight_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
