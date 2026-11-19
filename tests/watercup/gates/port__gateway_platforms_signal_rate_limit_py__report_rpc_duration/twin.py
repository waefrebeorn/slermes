"""twin for gateway/platforms/signal_rate_limit.py:report_rpc_duration (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal_rate_limit.py"
    PY_FN = "report_rpc_duration"
    C_FN = "report_rpc_duration"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
