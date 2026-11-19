"""twin for gateway/delivery.py:_is_thread_not_found_delivery_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "_is_thread_not_found_delivery_error"
    C_FN = "is_thread_not_found_delivery_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
