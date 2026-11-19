"""twin for gateway/platforms/signal_rate_limit.py:_extract_retry_after_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal_rate_limit.py"
    PY_FN = "_extract_retry_after_seconds"
    C_FN = "_extract_retry_after_seconds"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
