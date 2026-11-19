"""twin for gateway/status.py:resolve_gateway_liveness (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "resolve_gateway_liveness"
    C_FN = "resolve_gateway_liveness"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
