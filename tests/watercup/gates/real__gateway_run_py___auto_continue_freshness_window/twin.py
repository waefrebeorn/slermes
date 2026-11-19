"""twin for gateway/run.py:_auto_continue_freshness_window (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_auto_continue_freshness_window"
    C_FN = "gw_auto_continue_freshness_window"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
