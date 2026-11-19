"""twin for gateway/status.py:acquire_gateway_runtime_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "acquire_gateway_runtime_lock"
    C_FN = "gwstatus_acquire_gateway_runtime_lock"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
