"""twin for gateway/drain_control.py:clear_drain_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/drain_control.py"
    PY_FN = "clear_drain_request"
    C_FN = "gateway_drain_control_clear_drain_request"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
