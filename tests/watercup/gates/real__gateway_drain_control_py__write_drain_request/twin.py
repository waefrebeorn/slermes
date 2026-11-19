"""twin for gateway/drain_control.py:write_drain_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/drain_control.py"
    PY_FN = "write_drain_request"
    C_FN = "write_drain_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
