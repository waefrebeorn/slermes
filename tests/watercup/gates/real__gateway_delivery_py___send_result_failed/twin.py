"""twin for gateway/delivery.py:_send_result_failed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "_send_result_failed"
    C_FN = "send_result_failed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
