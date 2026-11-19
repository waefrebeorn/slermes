"""twin for gateway/memory_monitor.py:_get_rss_mb (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_monitor.py"
    PY_FN = "_get_rss_mb"
    C_FN = "get_rss_mb"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
