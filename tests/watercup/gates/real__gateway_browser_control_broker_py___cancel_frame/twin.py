"""twin for gateway/browser_control_broker.py:_cancel_frame (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/browser_control_broker.py"
    PY_FN = "_cancel_frame"
    C_FN = "_cancel_frame"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
