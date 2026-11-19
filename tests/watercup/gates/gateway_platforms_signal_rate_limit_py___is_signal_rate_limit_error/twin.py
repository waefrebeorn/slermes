"""twin for gateway/platforms/signal_rate_limit.py:_is_signal_rate_limit_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal_rate_limit.py"
    PY_FN = "_is_signal_rate_limit_error"
    C_FN = "cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
