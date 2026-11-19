"""twin for gateway/status.py:release_gateway_runtime_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "release_gateway_runtime_lock"
    C_FN = "gwstatus_release_gateway_runtime_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
