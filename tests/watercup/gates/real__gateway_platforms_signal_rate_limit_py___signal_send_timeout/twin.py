"""twin for gateway/platforms/signal_rate_limit.py:_signal_send_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal_rate_limit.py"
    PY_FN = "_signal_send_timeout"
    C_FN = "srl_signal_send_timeout"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
