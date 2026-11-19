"""twin for gateway/run.py:_shutdown_gateway_health_export (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_shutdown_gateway_health_export"
    C_FN = "gw_shutdown_health_export"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
