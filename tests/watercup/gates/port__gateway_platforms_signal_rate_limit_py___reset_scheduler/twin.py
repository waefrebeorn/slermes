"""twin for gateway/platforms/signal_rate_limit.py:_reset_scheduler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal_rate_limit.py"
    PY_FN = "_reset_scheduler"
    C_FN = "srl_reset_scheduler"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
