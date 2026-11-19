"""twin for tools/web_result_cache.py:_is_cache_exempt_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_result_cache.py"
    PY_FN = "_is_cache_exempt_host"
    C_FN = "_is_cache_exempt_host"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
