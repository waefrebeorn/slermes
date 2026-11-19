"""twin for gateway/status.py:is_gateway_runtime_lock_active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "is_gateway_runtime_lock_active"
    C_FN = "is_gateway_runtime_lock_active"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
