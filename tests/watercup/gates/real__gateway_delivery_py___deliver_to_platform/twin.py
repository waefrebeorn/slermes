"""twin for gateway/delivery.py:_deliver_to_platform (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "_deliver_to_platform"
    C_FN = "_deliver_to_platform"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
