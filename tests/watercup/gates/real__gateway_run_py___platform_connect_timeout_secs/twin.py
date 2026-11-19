"""twin for gateway/run.py:_platform_connect_timeout_secs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_platform_connect_timeout_secs"
    C_FN = "gw_platform_connect_timeout_secs"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
